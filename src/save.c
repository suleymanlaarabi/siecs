#include "relation.h"
#include "siecs.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/table_index.h"
#include "table.h"
#include "table_migration.h"
#include "type.h"
#include "world_internal.h"
#include <siecs/reflect_internal.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ECS_SCENE_VERSION 1u
#define ECS_SCENE_MAGIC_SIZE 8u
#define ECS_SCENE_NULL_INDEX UINT32_MAX

static const unsigned char ecs_scene_magic[ECS_SCENE_MAGIC_SIZE] = { 'S', 'I', 'E', 'C',
                                                                     'S', 'S', 'C', 'N' };

typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
    bool ok;
} ecs_scene_writer_t;

typedef struct {
    const unsigned char *ptr;
    const unsigned char *end;
    bool ok;
} ecs_scene_reader_t;

typedef struct {
    uint32_t *entity_to_local;
    uint32_t entity_to_local_count;
} ecs_scene_save_ctx_t;

typedef struct {
    ecs_entity_t *local_to_entity;
    uint32_t entity_count;
    bool component_owns_strings;
    Name *names; /* published only after ChildOf edges have been restored */
} ecs_scene_load_ctx_t;

static void ecs_scene_writer_init(ecs_scene_writer_t *w) {
    *w = (ecs_scene_writer_t){ .ok = true };
}

static void ecs_scene_writer_fini(ecs_scene_writer_t *w) {
    free(w->data);
    *w = (ecs_scene_writer_t){ 0 };
}

static bool ecs_scene_writer_reserve(ecs_scene_writer_t *w, size_t additional) {
    if (!w->ok)
        return false;
    if (additional > SIZE_MAX - w->size) {
        w->ok = false;
        return false;
    }

    size_t required = w->size + additional;
    if (required <= w->capacity)
        return true;

    size_t capacity = w->capacity ? w->capacity : 4096;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }

    void *new_data = realloc(w->data, capacity);
    if (!new_data) {
        w->ok = false;
        return false;
    }

    w->data = new_data;
    w->capacity = capacity;
    return true;
}

static bool ecs_scene_write(ecs_scene_writer_t *w, const void *data, size_t size) {
    if (!ecs_scene_writer_reserve(w, size))
        return false;
    if (size)
        memcpy(w->data + w->size, data, size);
    w->size += size;
    return true;
}

static bool ecs_scene_write_u16(ecs_scene_writer_t *w, uint16_t value) {
    return ecs_scene_write(w, &value, sizeof value);
}

static bool ecs_scene_write_u32(ecs_scene_writer_t *w, uint32_t value) {
    return ecs_scene_write(w, &value, sizeof value);
}

static bool ecs_scene_writer_patch_u32(ecs_scene_writer_t *w, size_t offset, uint32_t value) {
    if (!w->ok || offset > w->size || sizeof value > w->size - offset) {
        w->ok = false;
        return false;
    }
    memcpy(w->data + offset, &value, sizeof value);
    return true;
}

static bool ecs_scene_reader_take(ecs_scene_reader_t *r, void *dst, size_t size) {
    if (!r->ok || size > (size_t)(r->end - r->ptr)) {
        r->ok = false;
        return false;
    }
    if (dst && size)
        memcpy(dst, r->ptr, size);
    r->ptr += size;
    return true;
}

static uint16_t ecs_scene_read_u16(ecs_scene_reader_t *r) {
    uint16_t value = 0;
    ecs_scene_reader_take(r, &value, sizeof value);
    return value;
}

static uint32_t ecs_scene_read_u32(ecs_scene_reader_t *r) {
    uint32_t value = 0;
    ecs_scene_reader_take(r, &value, sizeof value);
    return value;
}

static bool ecs_scene_reader_skip(ecs_scene_reader_t *r, size_t size) {
    return ecs_scene_reader_take(r, NULL, size);
}

static bool ecs_scene_component_is_relation_internal(ecs_component_t component) {
    const ecs_component_record_t *record = ecs_component_index_get(component);
    return record->relation_flags != 0;
}

static bool ecs_scene_has_type_ops(const ecs_component_record_t *record) {
    const ecs_type_ops_t *ops = &record->ops;
    return ops->ctor || ops->dtor || ops->copy_ctor || ops->copy || ops->move_ctor || ops->move;
}

typedef struct {
    bool needs_codec;
    bool supported;
    bool contains_entity;
    bool contains_string;
    bool analyzed;
} ecs_scene_type_traits_t;

static ecs_scene_type_traits_t *ecs_scene_traits;
static size_t ecs_scene_traits_count;

static bool ecs_scene_traits_visit(const sireflect_type_visit_t *visit, void *user) {
    ecs_scene_type_traits_t *traits = user;
    switch (ecs_reflect_value_kind(visit->type)) {
    case EcsReflectValueEntity:
        traits->needs_codec = true;
        traits->contains_entity = true;
        break;
    case EcsReflectValueString:
        traits->needs_codec = true;
        traits->contains_string = true;
        break;
    case EcsReflectValuePointer:
        traits->needs_codec = true;
        traits->supported = false;
        break;
    default:
        break;
    }
    return true;
}

static ecs_scene_type_traits_t ecs_scene_type_traits(sireflect_handle_t type) {
    if (type == SIREFLECT_INVALID_HANDLE)
        return (ecs_scene_type_traits_t){ .supported = true };
    if (type >= ecs_scene_traits_count) {
        size_t count = ecs_scene_traits_count ? ecs_scene_traits_count : 64;
        while (type >= count)
            count *= 2;
        ecs_scene_type_traits_t *items = realloc(ecs_scene_traits, count * sizeof *items);
        if (!items)
            abort();
        memset(items + ecs_scene_traits_count, 0, (count - ecs_scene_traits_count) * sizeof *items);
        ecs_scene_traits = items;
        ecs_scene_traits_count = count;
    }
    ecs_scene_type_traits_t *cached = &ecs_scene_traits[type];
    if (!cached->analyzed) {
        *cached = (ecs_scene_type_traits_t){ .supported = true, .analyzed = true };
        if (!sireflect_walk_type(type, SIREFLECT_WALK_DEDUPLICATE, ecs_scene_traits_visit, cached))
            cached->supported = false;
    }
    return *cached;
}

void ecs_scene_type_cache_fini(void) {
    free(ecs_scene_traits);
    ecs_scene_traits = NULL;
    ecs_scene_traits_count = 0;
}

static bool ecs_scene_component_use_codec(const ecs_component_record_t *record) {
    if (!record->info || !record->info->size)
        return false;
    if (record->info->type == SIREFLECT_INVALID_HANDLE) {
        return ecs_scene_has_type_ops(record);
    }
    return ecs_scene_has_type_ops(record) || ecs_scene_type_traits(record->info->type).needs_codec;
}

static bool ecs_scene_component_supported(const ecs_component_record_t *record) {
    if (!record->info || !record->info->size)
        return true;

    if (ecs_scene_has_type_ops(record) && record->info->type == SIREFLECT_INVALID_HANDLE) {
        return false;
    }

    if (record->info->type != SIREFLECT_INVALID_HANDLE &&
        !ecs_scene_type_traits(record->info->type).supported)
        return false;

    return true;
}

static bool ecs_scene_save_string(ecs_scene_writer_t *w, const char *value) {
    if (!value)
        return ecs_scene_write_u32(w, ECS_SCENE_NULL_INDEX);
    size_t length = strlen(value);
    if (length > UINT32_MAX)
        return false;
    return ecs_scene_write_u32(w, (uint32_t)length) && ecs_scene_write(w, value, length);
}

static bool
ecs_scene_load_string(ecs_scene_reader_t *r, void *value, const ecs_scene_load_ctx_t *ctx) {
    uint32_t length = ecs_scene_read_u32(r);
    if (!r->ok)
        return false;
    if (length == ECS_SCENE_NULL_INDEX) {
        char *string = NULL;
        memcpy(value, &string, sizeof string);
        return true;
    }
    if ((size_t)length > (size_t)(r->end - r->ptr)) {
        r->ok = false;
        return false;
    }
    char *string = ctx->component_owns_strings
                       ? malloc((size_t)length + 1)
                       : ecs_arena_alloc(&ecs_world.scene_strings, length + 1);
    if (!string) {
        r->ok = false;
        return false;
    }
    if (!ecs_scene_reader_take(r, string, length)) {
        if (ctx->component_owns_strings)
            free(string);
        return false;
    }
    string[length] = '\0';
    memcpy(value, &string, sizeof string);
    return true;
}

typedef enum {
    EcsSceneSave,
    EcsSceneValidate,
    EcsSceneLoad,
} ecs_scene_codec_mode_t;

/* One type dispatch and child traversal for all three scene codec phases. */
static bool ecs_scene_codec_value(
    ecs_scene_codec_mode_t mode,
    ecs_scene_writer_t *writer,
    ecs_scene_reader_t *reader,
    sireflect_handle_t type,
    void *value,
    const ecs_scene_save_ctx_t *save_ctx,
    const ecs_scene_load_ctx_t *load_ctx,
    uint32_t entity_count
) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info)
        return false;
    switch (ecs_reflect_value_kind(type)) {
    case EcsReflectValueEntity: {
        if (mode == EcsSceneSave) {
            ecs_entity_t entity = *(const ecs_entity_t *)value;
            if (!entity)
                return ecs_scene_write_u32(writer, ECS_SCENE_NULL_INDEX);
            uint32_t id = ecs_entity_id(entity);
            if (id >= save_ctx->entity_to_local_count)
                return false;
            uint32_t local = save_ctx->entity_to_local[id];
            return local != ECS_SCENE_NULL_INDEX && ecs_scene_write_u32(writer, local);
        }
        uint32_t local = ecs_scene_read_u32(reader);
        if (!reader->ok || (local != ECS_SCENE_NULL_INDEX && local >= entity_count))
            return false;
        if (mode == EcsSceneLoad)
            *(ecs_entity_t *)value =
                local == ECS_SCENE_NULL_INDEX ? 0 : load_ctx->local_to_entity[local];
        return true;
    }
    case EcsReflectValueStruct:
        for (size_t i = 0; i < info->fields.field_count; i++) {
            const sireflect_field_info_t *field = &info->fields.fields[i];
            void *child = value ? (unsigned char *)value + field->offset : NULL;
            if (!ecs_scene_codec_value(
                    mode,
                    writer,
                    reader,
                    field->type,
                    child,
                    save_ctx,
                    load_ctx,
                    entity_count
                ))
                return false;
        }
        return true;
    case EcsReflectValueArray: {
        const sireflect_type_info_t *element = sireflect_type_info(info->element_type);
        if (!element)
            return false;
        for (size_t i = 0; i < info->element_count; i++) {
            void *child = value ? (unsigned char *)value + i * element->size : NULL;
            if (!ecs_scene_codec_value(
                    mode,
                    writer,
                    reader,
                    info->element_type,
                    child,
                    save_ctx,
                    load_ctx,
                    entity_count
                ))
                return false;
        }
        return true;
    }
    case EcsReflectValueString:
        if (mode == EcsSceneSave) {
            const char *string;
            memcpy(&string, value, sizeof string);
            return ecs_scene_save_string(writer, string);
        }
        if (mode == EcsSceneLoad)
            return ecs_scene_load_string(reader, value, load_ctx);
        {
            uint32_t length = ecs_scene_read_u32(reader);
            return reader->ok &&
                   (length == ECS_SCENE_NULL_INDEX || ecs_scene_reader_skip(reader, length));
        }
    case EcsReflectValuePointer:
        return false;
    default:
        if (mode == EcsSceneSave)
            return ecs_scene_write(writer, value, info->size);
        return ecs_scene_reader_take(reader, mode == EcsSceneLoad ? value : NULL, info->size);
    }
}

static bool ecs_scene_save_value(
    ecs_scene_writer_t *w,
    sireflect_handle_t type,
    const void *value,
    const ecs_scene_save_ctx_t *ctx
) {
    return ecs_scene_codec_value(EcsSceneSave, w, NULL, type, (void *)value, ctx, NULL, 0);
}

static bool
ecs_scene_validate_value(ecs_scene_reader_t *r, sireflect_handle_t type, uint32_t entity_count) {
    return ecs_scene_codec_value(EcsSceneValidate, NULL, r, type, NULL, NULL, NULL, entity_count);
}

static bool ecs_scene_load_value(
    ecs_scene_reader_t *r,
    sireflect_handle_t type,
    void *value,
    const ecs_scene_load_ctx_t *ctx
) {
    return ecs_scene_codec_value(EcsSceneLoad, NULL, r, type, value, NULL, ctx, ctx->entity_count);
}

static uint16_t ecs_scene_table_component_count(const ecs_table_t *table) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < table->type.component_count; i++) {
        if (!ecs_scene_component_is_relation_internal(table->type.ids[i]))
            count++;
    }
    return count;
}

static bool ecs_scene_build_entity_map(
    ecs_scene_save_ctx_t *ctx,
    uint32_t *entity_count,
    uint32_t *table_count
) {
    uint32_t map_count = entity_index.entities.size;
    uint32_t *map = malloc((size_t)map_count * sizeof(uint32_t));
    if (!map && map_count)
        return false;

    for (uint32_t i = 0; i < map_count; i++)
        map[i] = ECS_SCENE_NULL_INDEX;

    uint32_t local = 0;
    uint32_t tables = 0;
    for (uint16_t t = 0; t < table_index.table_count; t++) {
        const ecs_table_t *table = ecs_table_index_at(t);
        if (!table->entity_count)
            continue;
        tables++;

        for (uint32_t row = 0; row < table->entity_count; row++) {
            uint32_t id = ecs_entity_id(table->entities[row]);
            if (id >= map_count) {
                free(map);
                return false;
            }
            map[id] = local++;
        }
    }

    ctx->entity_to_local = map;
    ctx->entity_to_local_count = map_count;
    *entity_count = local;
    *table_count = tables;
    return true;
}

static bool ecs_scene_write_component_payload(
    ecs_scene_writer_t *out,
    const ecs_table_t *table,
    uint16_t column_index,
    const ecs_scene_save_ctx_t *ctx
) {
    ecs_component_t component = table->type.ids[column_index];
    const ecs_component_record_t *record = ecs_component_index_get(component);
    const ecs_column_t *column = &table->cls[column_index];

    if (!ecs_scene_component_supported(record))
        return false;
    if (!column->size)
        return ecs_scene_write_u32(out, 0);

    size_t length_at = out->size;
    if (!ecs_scene_write_u32(out, 0))
        return false;
    size_t begin = out->size;
    if (!ecs_scene_component_use_codec(record)) {
        uint64_t bytes = (uint64_t)column->size * table->entity_count;
        if (bytes > UINT32_MAX || !ecs_scene_write(out, column->data, (size_t)bytes))
            return false;
    } else {
        if (record->info->type == SIREFLECT_INVALID_HANDLE)
            return false;
        for (uint32_t row = 0; row < table->entity_count; row++) {
            const void *value = ecs_table_component_at_column(table, column_index, row);
            if (!ecs_scene_save_value(out, record->info->type, value, ctx))
                return false;
        }
    }
    size_t bytes = out->size - begin;
    return bytes <= UINT32_MAX && ecs_scene_writer_patch_u32(out, length_at, (uint32_t)bytes);
}

static bool ecs_scene_write_tables(ecs_scene_writer_t *w, const ecs_scene_save_ctx_t *ctx) {
    for (uint16_t t = 0; t < table_index.table_count; t++) {
        const ecs_table_t *table = ecs_table_index_at(t);
        if (!table->entity_count)
            continue;

        uint16_t component_count = ecs_scene_table_component_count(table);
        if (!ecs_scene_write_u32(w, table->entity_count) ||
            !ecs_scene_write_u16(w, component_count) || !ecs_scene_write_u16(w, 0)) {
            return false;
        }

        for (uint16_t i = 0; i < table->type.component_count; i++) {
            ecs_component_t component = table->type.ids[i];
            if (ecs_scene_component_is_relation_internal(component))
                continue;
            if (!ecs_scene_write_u16(w, component))
                return false;
        }

        for (uint16_t i = 0; i < table->type.component_count; i++) {
            ecs_component_t component = table->type.ids[i];
            if (ecs_scene_component_is_relation_internal(component))
                continue;
            if (!ecs_scene_write_component_payload(w, table, i, ctx))
                return false;
        }
    }
    return true;
}

static bool ecs_scene_write_relations(
    ecs_scene_writer_t *w,
    const ecs_scene_save_ctx_t *ctx,
    uint32_t *relation_edge_count
) {
    uint32_t count = 0;

    for (uint16_t t = 0; t < table_index.table_count; t++) {
        const ecs_table_t *table = ecs_table_index_at(t);
        for (uint32_t row = 0; row < table->entity_count; row++) {
            ecs_entity_t source = table->entities[row];
            uint32_t source_id = ecs_entity_id(source);
            if (source_id >= ctx->entity_to_local_count)
                return false;
            uint32_t source_local = ctx->entity_to_local[source_id];
            if (source_local == ECS_SCENE_NULL_INDEX)
                return false;

            for (uint32_t relation = 1; relation < relation_index.records.size; relation++) {
                const ecs_relation_record_t *relation_record =
                    ecs_relation_record((ecs_relation_id_t)relation);
                if (!relation_record->info.name && !relation_record->component &&
                    !relation_record->ops) {
                    continue;
                }

                ecs_entity_t target = ecs_target_id(source, (ecs_relation_id_t)relation);
                if (!target)
                    continue;

                uint32_t target_id = ecs_entity_id(target);
                if (target_id >= ctx->entity_to_local_count)
                    return false;
                uint32_t target_local = ctx->entity_to_local[target_id];
                if (target_local == ECS_SCENE_NULL_INDEX)
                    return false;

                if (!ecs_scene_write_u32(w, source_local) ||
                    !ecs_scene_write_u16(w, (uint16_t)relation) || !ecs_scene_write_u16(w, 0) ||
                    !ecs_scene_write_u32(w, target_local)) {
                    return false;
                }
                count++;
            }
        }
    }

    *relation_edge_count = count;
    return true;
}

SIECS_API void ecs_scene_free(void *data) { free(data); }

SIECS_API bool ecs_save_memory(void **data_out, size_t *size_out) {
    if (!data_out || !size_out)
        return false;

    *data_out = NULL;
    *size_out = 0;

    ecs_scene_save_ctx_t ctx = { 0 };
    uint32_t entity_count = 0;
    uint32_t table_count = 0;
    if (!ecs_scene_build_entity_map(&ctx, &entity_count, &table_count))
        return false;

    ecs_scene_writer_t writer;
    ecs_scene_writer_init(&writer);

    /* Header: magic, version, table_count, entity_count, relation_edge_count. */
    bool ok = ecs_scene_write(&writer, ecs_scene_magic, sizeof ecs_scene_magic) &&
              ecs_scene_write_u32(&writer, ECS_SCENE_VERSION) &&
              ecs_scene_write_u32(&writer, table_count) &&
              ecs_scene_write_u32(&writer, entity_count);

    size_t relation_count_offset = writer.size;
    ok = ok && ecs_scene_write_u32(&writer, 0);

    if (ok)
        ok = ecs_scene_write_tables(&writer, &ctx);

    uint32_t relation_edge_count = 0;
    if (ok)
        ok = ecs_scene_write_relations(&writer, &ctx, &relation_edge_count);
    if (ok)
        ok = ecs_scene_writer_patch_u32(&writer, relation_count_offset, relation_edge_count);

    if (ok) {
        *data_out = writer.data;
        *size_out = writer.size;
        writer.data = NULL;
    }

    free(ctx.entity_to_local);
    ecs_scene_writer_fini(&writer);
    return ok;
}

SIECS_API bool ecs_save(const char *path) {
    if (!path)
        return false;

    void *data = NULL;
    size_t size = 0;
    if (!ecs_save_memory(&data, &size))
        return false;

    FILE *file = fopen(path, "wb");
    bool ok = file != NULL;
    if (ok) {
        ok = fwrite(data, 1, size, file) == size;
        if (fclose(file) != 0)
            ok = false;
    }

    ecs_scene_free(data);
    return ok;
}

static bool ecs_scene_read_file(const char *path, unsigned char **data_out, size_t *size_out) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long end = ftell(file);
    if (end < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    size_t size = (size_t)end;
    unsigned char *data = size ? malloc(size) : NULL;
    if (size && !data) {
        fclose(file);
        return false;
    }

    bool ok = !size || fread(data, 1, size, file) == size;
    if (fclose(file) != 0)
        ok = false;
    if (!ok) {
        free(data);
        return false;
    }

    *data_out = data;
    *size_out = size;
    return true;
}

/* All three table passes use the same checked cursor. Component ids stay in
 * the input buffer and are read with memcpy: a preceding string payload may
 * leave the next table unaligned. Only the archetype takes an owned copy. */
typedef enum { EcsSceneCheck, EcsSceneCreate, EcsSceneApply } ecs_scene_table_pass_t;
typedef struct {
    uint32_t table_count, entity_count, relation_count;
    ecs_scene_reader_t tables, relations;
} ecs_scene_document_t;

static ecs_component_t ecs_scene_component_at(const unsigned char *ids, uint16_t at) {
    ecs_component_t id;
    memcpy(&id, ids + (size_t)at * sizeof id, sizeof id);
    return id;
}

static bool ecs_scene_read_payload(
    ecs_scene_reader_t *payload,
    ecs_component_t component,
    uint32_t rows,
    ecs_entity_t *entities,
    const ecs_scene_load_ctx_t *ctx,
    bool load
) {
    const ecs_component_record_t *record = ecs_component_index_get(component);
    size_t size = record->info->size;
    size_t bytes = (size_t)(payload->end - payload->ptr);
    if (record->info->size > UINT32_MAX || !ecs_scene_component_supported(record))
        return false;
    if (!size)
        return bytes == 0;
    if (!ecs_scene_component_use_codec(record)) {
        if ((rows && size > SIZE_MAX / rows) || size * rows != bytes)
            return false;
        if (load) {
            for (uint32_t row = 0; row < rows; row++)
                if (!ecs_scene_reader_take(payload, ecs_get_cid(entities[row], component), size))
                    return false;
        } else {
            payload->ptr = payload->end;
        }
    } else {
        if (record->info->type == SIREFLECT_INVALID_HANDLE)
            return false;
        ecs_scene_load_ctx_t component_ctx = *ctx;
        component_ctx.component_owns_strings = record->ops.dtor != NULL;
        for (uint32_t row = 0; row < rows; row++) {
            if (!load) {
                if (!ecs_scene_validate_value(payload, record->info->type, ctx->entity_count))
                    return false;
                continue;
            }
            void *temp = calloc(1, size);
            if (!temp)
                return false;
            bool ok = ecs_scene_load_value(payload, record->info->type, temp, &component_ctx);
            if (ok && component == ecs_id(Name)) {
                if (!ecs_name_valid(((Name *)temp)->value)) ok = false;
                else {
                    ctx->names[entities - ctx->local_to_entity + row] = *(Name *)temp;
                    ((Name *)temp)->value = NULL;
                }
            } else if (ok) {
                ecs_set_cid(entities[row], component, temp);
            }
            if (record->ops.dtor)
                record->ops.dtor(temp, 1);
            free(temp);
            if (!ok)
                return false;
        }
    }
    return payload->ok && payload->ptr == payload->end;
}

static bool ecs_scene_read_tables(
    ecs_scene_reader_t *r,
    uint32_t table_count,
    const ecs_scene_load_ctx_t *ctx,
    ecs_scene_table_pass_t pass
) {
    uint32_t local = 0;
    for (uint32_t t = 0; t < table_count; t++) {
        uint32_t rows = ecs_scene_read_u32(r);
        uint16_t count = ecs_scene_read_u16(r);
        uint16_t reserved = ecs_scene_read_u16(r);
        if (!r->ok || reserved || local > ctx->entity_count || rows > ctx->entity_count - local)
            return false;
        const unsigned char *ids = r->ptr;
        if (!ecs_scene_reader_skip(r, (size_t)count * sizeof(ecs_component_t)))
            return false;
        ecs_component_t previous = 0;
        for (uint16_t i = 0; i < count; i++) {
            ecs_component_t id = ecs_scene_component_at(ids, i);
            if (id <= previous || id >= component_index.components.size ||
                !ecs_component_index_get(id)->info || ecs_scene_component_is_relation_internal(id))
                return false;
            previous = id;
        }
        ecs_entity_t *entities = ctx->local_to_entity ? ctx->local_to_entity + local : NULL;
        if (pass == EcsSceneCreate) {
            ecs_type_t type = { 0 };
            uint16_t table_id =
                count ? ecs_table_index_get_or_create(ecs_type_with_ids(&type, ids, count)) : 0;
            for (uint32_t row = 0; row < rows; row++) {
                ecs_entity_t entity = ecs_new();
                if (table_id) {
                    ecs_entity_record_t *record = ecs_get_record(entity);
                    ecs_migrate(record, entity, ecs_get_table(record->table_id), table_id, 0);
                }
                entities[row] = entity;
            }
        } else if (pass == EcsSceneApply) {
            /* Keep component-major hook order, before any values are loaded. */
            for (uint16_t i = 0; i < count; i++) {
                ecs_component_t id = ecs_scene_component_at(ids, i);
                const ecs_component_record_t *record = ecs_component_index_get(id);
                if (record->on_add) {
                    for (uint32_t row = 0; row < rows; row++)
                        record->on_add(
                            entities[row],
                            id,
                            record->info->size ? ecs_get_cid(entities[row], id) : NULL
                        );
                }
            }
        }
        for (uint16_t i = 0; i < count; i++) {
            uint32_t bytes = ecs_scene_read_u32(r);
            if (!r->ok || bytes > (size_t)(r->end - r->ptr))
                return false;
            ecs_scene_reader_t payload = { .ptr = r->ptr, .end = r->ptr + bytes, .ok = true };
            if (pass != EcsSceneCreate && !ecs_scene_read_payload(
                                              &payload,
                                              ecs_scene_component_at(ids, i),
                                              rows,
                                              entities,
                                              ctx,
                                              pass == EcsSceneApply
                                          ))
                return false;
            r->ptr = payload.end;
        }
        local += rows;
    }
    return r->ok && local == ctx->entity_count;
}

static bool ecs_scene_read_relations(
    ecs_scene_reader_t r,
    uint32_t count,
    const ecs_scene_load_ctx_t *ctx,
    bool apply
) {
    if (count > (size_t)(r.end - r.ptr) / 12u)
        return false;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t source = ecs_scene_read_u32(&r);
        uint16_t relation = ecs_scene_read_u16(&r);
        uint16_t reserved = ecs_scene_read_u16(&r);
        uint32_t target = ecs_scene_read_u32(&r);
        if (!r.ok || reserved || source >= ctx->entity_count || target >= ctx->entity_count ||
            !relation || relation >= relation_index.records.size)
            return false;
        const ecs_relation_record_t *record = ecs_relation_record(relation);
        if (!record->info.name && !record->component && !record->ops)
            return false;
        if (!apply)
            continue;
        ecs_entity_t source_entity = ctx->local_to_entity[source];
        ecs_entity_t target_entity = ctx->local_to_entity[target];
        /* Reject scene cycles here instead of tripping the fatal relate check. */
        for (ecs_entity_t up = target_entity; record->info.desc.acyclic && up;
             up = ecs_target_id(up, relation))
            if (up == source_entity)
                return false;
        ecs_relate_id(source_entity, relation, target_entity);
    }
    return r.ok && r.ptr == r.end;
}

static bool ecs_scene_parse(const void *input, size_t size, ecs_scene_document_t *document) {
    if (!input || size < ECS_SCENE_MAGIC_SIZE + sizeof(uint32_t) * 4)
        return false;
    ecs_scene_reader_t r = { .ptr = input, .end = (const unsigned char *)input + size, .ok = true };
    unsigned char magic[ECS_SCENE_MAGIC_SIZE];
    if (!ecs_scene_reader_take(&r, magic, sizeof magic) ||
        memcmp(magic, ecs_scene_magic, sizeof magic) || ecs_scene_read_u32(&r) != ECS_SCENE_VERSION)
        return false;
    document->table_count = ecs_scene_read_u32(&r);
    document->entity_count = ecs_scene_read_u32(&r);
    document->relation_count = ecs_scene_read_u32(&r);
    if (!r.ok || document->table_count > (size_t)(r.end - r.ptr) / 8u)
        return false;
    document->tables = r;
    ecs_scene_load_ctx_t ctx = { .entity_count = document->entity_count };
    if (!ecs_scene_read_tables(&r, document->table_count, &ctx, EcsSceneCheck))
        return false;
    document->tables.end = r.ptr;
    document->relations = r;
    return ecs_scene_read_relations(r, document->relation_count, &ctx, false);
}

SIECS_API bool ecs_scene_validate(const void *input, size_t size) {
    ecs_scene_document_t document;
    return ecs_scene_parse(input, size, &document);
}

SIECS_API bool ecs_load_memory(const void *input, size_t size) {
    ecs_scene_document_t document;
    if (!ecs_scene_parse(input, size, &document))
        return false;
    uint32_t count = document.entity_count;
    ecs_entity_t *entities = count ? calloc(count, sizeof *entities) : NULL;
    if (count && !entities)
        return false;
    Name *names = count ? calloc(count, sizeof *names) : NULL;
    if (count && !names) {
        free(entities);
        return false;
    }
    ecs_scene_load_ctx_t ctx = {
        .local_to_entity = entities,
        .entity_count = count,
        .names = names,
    };
    ecs_scene_reader_t first = document.tables, second = document.tables;
    bool ok = ecs_scene_read_tables(&first, document.table_count, &ctx, EcsSceneCreate) &&
              first.ptr == first.end &&
              ecs_scene_read_tables(&second, document.table_count, &ctx, EcsSceneApply) &&
              second.ptr == second.end &&
              ecs_scene_read_relations(document.relations, document.relation_count, &ctx, true);
    if (ok) {
        for (uint32_t i = 0; i < count; i++) {
            if (names[i].value &&
                !ecs_name_update_now(entities[i], names[i].value, true, 0, false)) {
                ok = false;
                break;
            }
        }
    }
    for (uint32_t i = 0; i < count; i++) {
        free(names[i].value);
        /* Roll back: a failed load leaves none of the scene's entities behind. */
        if (!ok && entities[i] && ecs_is_alive(entities[i]))
            ecs_kill(entities[i]);
    }
    free(names);
    free(entities);
    return ok;
}

SIECS_API bool ecs_load(const char *path) {
    if (!path)
        return false;

    unsigned char *data = NULL;
    size_t size = 0;
    if (!ecs_scene_read_file(path, &data, &size))
        return false;

    bool ok = ecs_load_memory(data, size);
    ecs_scene_free(data);
    return ok;
}
