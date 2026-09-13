#include "relation.h"
#include "siecs.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/table_index.h"
#include "table.h"
#include "table_migration.h"
#include "type.h"
#include "world_internal.h"

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

static bool ecs_scene_is_entity_type(const sireflect_type_info_t *info) {
    return info && info->name && strcmp(info->name, "ecs_entity_t") == 0;
}

static bool ecs_scene_type_needs_codec(sireflect_handle_t type) {
    if (type == SIREFLECT_INVALID_HANDLE)
        return false;

    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info)
        return false;
    if (ecs_scene_is_entity_type(info))
        return true;

    if (info->kind == sireflect_kind_pointer || info->kind == sireflect_kind_ptr ||
        info->kind == sireflect_kind_function_pointer) {
        return true;
    }

    if (info->kind == sireflect_kind_array) {
        return ecs_scene_type_needs_codec(info->element_type);
    }

    if (info->kind == sireflect_kind_struct) {
        for (size_t i = 0; i < info->fields.field_count; i++) {
            if (ecs_scene_type_needs_codec(info->fields.fields[i].type))
                return true;
        }
    }

    return false;
}

static bool ecs_scene_type_has_unsupported_pointer(sireflect_handle_t type) {
    if (type == SIREFLECT_INVALID_HANDLE)
        return false;

    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info)
        return false;

    if (info->kind == sireflect_kind_pointer) {
        const sireflect_type_info_t *element = sireflect_type_info(info->element_type);
        return !element || element->kind != sireflect_kind_char;
    }
    if (info->kind == sireflect_kind_ptr || info->kind == sireflect_kind_function_pointer) {
        return true;
    }
    if (info->kind == sireflect_kind_array) {
        return ecs_scene_type_has_unsupported_pointer(info->element_type);
    }
    if (info->kind == sireflect_kind_struct) {
        for (size_t i = 0; i < info->fields.field_count; i++) {
            if (ecs_scene_type_has_unsupported_pointer(info->fields.fields[i].type))
                return true;
        }
    }
    return false;
}

static bool ecs_scene_component_use_codec(const ecs_component_record_t *record) {
    if (!record->info || !record->info->size)
        return false;
    if (record->info->type == SIREFLECT_INVALID_HANDLE) {
        return ecs_scene_has_type_ops(record);
    }
    return ecs_scene_has_type_ops(record) || ecs_scene_type_needs_codec(record->info->type);
}

static bool ecs_scene_component_supported(const ecs_component_record_t *record) {
    if (!record->info || !record->info->size)
        return true;

    if (ecs_scene_has_type_ops(record) && record->info->type == SIREFLECT_INVALID_HANDLE) {
        return false;
    }

    if (record->info->type != SIREFLECT_INVALID_HANDLE &&
        ecs_scene_type_has_unsupported_pointer(record->info->type))
        return false;

    return true;
}

static bool ecs_scene_save_value(
    ecs_scene_writer_t *w,
    sireflect_handle_t type,
    const void *value,
    const ecs_scene_save_ctx_t *ctx
);

static bool ecs_scene_load_value(
    ecs_scene_reader_t *r,
    sireflect_handle_t type,
    void *value,
    const ecs_scene_load_ctx_t *ctx
);

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

static bool ecs_scene_save_value(
    ecs_scene_writer_t *w,
    sireflect_handle_t type,
    const void *value,
    const ecs_scene_save_ctx_t *ctx
) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info)
        return false;

    if (ecs_scene_is_entity_type(info)) {
        ecs_entity_t entity = *(const ecs_entity_t *)value;
        if (!entity)
            return ecs_scene_write_u32(w, ECS_SCENE_NULL_INDEX);

        uint32_t id = ecs_entity_id(entity);
        if (id >= ctx->entity_to_local_count)
            return false;
        uint32_t local = ctx->entity_to_local[id];
        if (local == ECS_SCENE_NULL_INDEX)
            return false;
        return ecs_scene_write_u32(w, local);
    }

    switch (info->kind) {
    case sireflect_kind_struct:
        for (size_t i = 0; i < info->fields.field_count; i++) {
            const sireflect_field_info_t *field = &info->fields.fields[i];
            if (!ecs_scene_save_value(
                    w,
                    field->type,
                    (const unsigned char *)value + field->offset,
                    ctx
                )) {
                return false;
            }
        }
        return true;

    case sireflect_kind_array: {
        const sireflect_type_info_t *element = sireflect_type_info(info->element_type);
        if (!element)
            return false;
        for (size_t i = 0; i < info->element_count; i++) {
            if (!ecs_scene_save_value(
                    w,
                    info->element_type,
                    (const unsigned char *)value + i * element->size,
                    ctx
                )) {
                return false;
            }
        }
        return true;
    }

    case sireflect_kind_pointer: {
        const sireflect_type_info_t *element = sireflect_type_info(info->element_type);
        if (!element || element->kind != sireflect_kind_char)
            return false;
        const char *string;
        memcpy(&string, value, sizeof string);
        return ecs_scene_save_string(w, string);
    }

    case sireflect_kind_ptr:
    case sireflect_kind_function_pointer:
        return false;

    default:
        return ecs_scene_write(w, value, info->size);
    }
}

static bool ecs_scene_validate_value(
    ecs_scene_reader_t *r,
    sireflect_handle_t type,
    uint32_t entity_count
) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info)
        return false;

    if (ecs_scene_is_entity_type(info)) {
        uint32_t local = ecs_scene_read_u32(r);
        return r->ok && (local == ECS_SCENE_NULL_INDEX || local < entity_count);
    }

    switch (info->kind) {
    case sireflect_kind_struct:
        for (size_t i = 0; i < info->fields.field_count; i++) {
            if (!ecs_scene_validate_value(r, info->fields.fields[i].type, entity_count))
                return false;
        }
        return true;

    case sireflect_kind_array:
        for (size_t i = 0; i < info->element_count; i++) {
            if (!ecs_scene_validate_value(r, info->element_type, entity_count))
                return false;
        }
        return true;

    case sireflect_kind_pointer: {
        const sireflect_type_info_t *element = sireflect_type_info(info->element_type);
        if (!element || element->kind != sireflect_kind_char)
            return false;

        uint32_t length = ecs_scene_read_u32(r);
        if (!r->ok || length == ECS_SCENE_NULL_INDEX)
            return r->ok;
        return ecs_scene_reader_skip(r, length);
    }

    case sireflect_kind_ptr:
    case sireflect_kind_function_pointer:
        return false;

    default:
        return ecs_scene_reader_skip(r, info->size);
    }
}

static bool ecs_scene_load_value(
    ecs_scene_reader_t *r,
    sireflect_handle_t type,
    void *value,
    const ecs_scene_load_ctx_t *ctx
) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info)
        return false;

    if (ecs_scene_is_entity_type(info)) {
        uint32_t local = ecs_scene_read_u32(r);
        if (!r->ok)
            return false;
        if (local == ECS_SCENE_NULL_INDEX) {
            *(ecs_entity_t *)value = 0;
            return true;
        }
        if (local >= ctx->entity_count)
            return false;
        *(ecs_entity_t *)value = ctx->local_to_entity[local];
        return true;
    }

    switch (info->kind) {
    case sireflect_kind_struct:
        for (size_t i = 0; i < info->fields.field_count; i++) {
            const sireflect_field_info_t *field = &info->fields.fields[i];
            if (!ecs_scene_load_value(
                    r,
                    field->type,
                    (unsigned char *)value + field->offset,
                    ctx
                )) {
                return false;
            }
        }
        return true;

    case sireflect_kind_array: {
        const sireflect_type_info_t *element = sireflect_type_info(info->element_type);
        if (!element)
            return false;
        for (size_t i = 0; i < info->element_count; i++) {
            if (!ecs_scene_load_value(
                    r,
                    info->element_type,
                    (unsigned char *)value + i * element->size,
                    ctx
                )) {
                return false;
            }
        }
        return true;
    }

    case sireflect_kind_pointer: {
        const sireflect_type_info_t *element = sireflect_type_info(info->element_type);
        if (!element || element->kind != sireflect_kind_char)
            return false;
        return ecs_scene_load_string(r, value, ctx);
    }

    case sireflect_kind_ptr:
    case sireflect_kind_function_pointer:
        return false;

    default:
        return ecs_scene_reader_take(r, value, info->size);
    }
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

    ecs_scene_writer_t payload;
    ecs_scene_writer_init(&payload);

    bool codec = ecs_scene_component_use_codec(record);
    if (!codec) {
        size_t bytes = (size_t)column->size * table->entity_count;
        if (bytes > UINT32_MAX || !ecs_scene_write(&payload, column->data, bytes)) {
            ecs_scene_writer_fini(&payload);
            return false;
        }
    } else {
        if (record->info->type == SIREFLECT_INVALID_HANDLE) {
            ecs_scene_writer_fini(&payload);
            return false;
        }
        for (uint32_t row = 0; row < table->entity_count; row++) {
            const void *value = (const unsigned char *)column->data + (size_t)row * column->size;
            if (!ecs_scene_save_value(&payload, record->info->type, value, ctx)) {
                ecs_scene_writer_fini(&payload);
                return false;
            }
        }
    }

    if (!payload.ok || payload.size > UINT32_MAX ||
        !ecs_scene_write_u32(out, (uint32_t)payload.size) ||
        !ecs_scene_write(out, payload.data, payload.size)) {
        ecs_scene_writer_fini(&payload);
        return false;
    }

    ecs_scene_writer_fini(&payload);
    return true;
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

SIECS_API void ecs_scene_free(void *data) {
    free(data);
}

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

static bool ecs_scene_validate_tables(
    ecs_scene_reader_t *r,
    uint32_t table_count,
    uint32_t entity_count,
    const unsigned char **relations_begin
);

static bool ecs_scene_read_header(
    ecs_scene_reader_t *r,
    uint32_t *table_count,
    uint32_t *entity_count,
    uint32_t *relation_edge_count
) {
    unsigned char magic[ECS_SCENE_MAGIC_SIZE];
    if (!ecs_scene_reader_take(r, magic, sizeof magic))
        return false;
    if (memcmp(magic, ecs_scene_magic, sizeof magic) != 0)
        return false;

    uint32_t version = ecs_scene_read_u32(r);
    if (!r->ok || version != ECS_SCENE_VERSION)
        return false;

    *table_count = ecs_scene_read_u32(r);
    *entity_count = ecs_scene_read_u32(r);
    *relation_edge_count = ecs_scene_read_u32(r);
    return r->ok;
}

SIECS_API bool ecs_scene_validate(const void *input, size_t size) {
    if (!input || size < ECS_SCENE_MAGIC_SIZE + sizeof(uint32_t) * 4)
        return false;

    const unsigned char *data = input;
    ecs_scene_reader_t header = { .ptr = data, .end = data + size, .ok = true };
    uint32_t table_count = 0;
    uint32_t entity_count = 0;
    uint32_t relation_edge_count = 0;
    if (!ecs_scene_read_header(&header, &table_count, &entity_count, &relation_edge_count))
        return false;

    if (table_count > (size - (size_t)(header.ptr - data)) / 8u)
        return false;

    ecs_scene_reader_t tables = { .ptr = header.ptr, .end = data + size, .ok = true };
    const unsigned char *relations_begin = NULL;
    if (!ecs_scene_validate_tables(
            &tables, table_count, entity_count, &relations_begin
        )) {
        return false;
    }

    ecs_scene_reader_t relations = {
        .ptr = relations_begin,
        .end = data + size,
        .ok = true,
    };
    if (relation_edge_count > (size_t)(relations.end - relations.ptr) / 12u)
        return false;

    for (uint32_t i = 0; i < relation_edge_count; i++) {
        uint32_t source = ecs_scene_read_u32(&relations);
        uint16_t relation = ecs_scene_read_u16(&relations);
        uint16_t reserved = ecs_scene_read_u16(&relations);
        uint32_t target = ecs_scene_read_u32(&relations);
        if (!relations.ok || reserved != 0 || source >= entity_count || target >= entity_count ||
            relation == 0 || relation >= relation_index.records.size)
            return false;

        const ecs_relation_record_t *record = ecs_relation_record(relation);
        if (!record->info.name && !record->component && !record->ops)
            return false;
    }

    return relations.ok && relations.ptr == relations.end;
}

static bool ecs_scene_validate_component_id(ecs_component_t component) {
    return component != 0 && component < component_index.components.size &&
           ecs_component_index_get(component)->info != NULL &&
           !ecs_scene_component_is_relation_internal(component);
}

static bool ecs_scene_validate_tables(
    ecs_scene_reader_t *r,
    uint32_t table_count,
    uint32_t entity_count,
    const unsigned char **relations_begin
) {
    uint32_t local_base = 0;

    for (uint32_t t = 0; t < table_count; t++) {
        uint32_t row_count = ecs_scene_read_u32(r);
        uint16_t component_count = ecs_scene_read_u16(r);
        uint16_t reserved = ecs_scene_read_u16(r);
        if (!r->ok || reserved != 0 || local_base > entity_count ||
            row_count > entity_count - local_base)
            return false;

        ecs_component_t *components = component_count
            ? malloc((size_t)component_count * sizeof(ecs_component_t))
            : NULL;
        if (component_count && !components)
            return false;
        for (uint16_t i = 0; i < component_count; i++) {
            components[i] = ecs_scene_read_u16(r);
            if (!r->ok || !ecs_scene_validate_component_id(components[i]) ||
                (i && components[i - 1] >= components[i]))
                goto invalid_table;

            const ecs_component_record_t *record = ecs_component_index_get(components[i]);
            if (record->info->size > UINT32_MAX || !ecs_scene_component_supported(record))
                goto invalid_table;
        }

        for (uint16_t i = 0; i < component_count; i++) {
            const ecs_component_record_t *record = ecs_component_index_get(components[i]);
            uint32_t payload_size = ecs_scene_read_u32(r);
            uint32_t component_size = (uint32_t)record->info->size;
            if (!r->ok || (size_t)payload_size > (size_t)(r->end - r->ptr))
                goto invalid_table;

            const unsigned char *payload_end = r->ptr + payload_size;
            if (!component_size) {
                if (payload_size != 0)
                    goto invalid_table;
            } else if (!ecs_scene_component_use_codec(record)) {
                if (row_count && (size_t)component_size > SIZE_MAX / row_count)
                    goto invalid_table;
                if ((size_t)component_size * row_count != payload_size)
                    goto invalid_table;
            } else {
                ecs_scene_reader_t payload = { .ptr = r->ptr, .end = payload_end, .ok = true };
                for (uint32_t row = 0; row < row_count; row++) {
                    if (!ecs_scene_validate_value(&payload, record->info->type, entity_count))
                        goto invalid_table;
                }
                if (!payload.ok || payload.ptr != payload.end)
                    goto invalid_table;
            }
            r->ptr = payload_end;
        }

        free(components);
        local_base += row_count;
        continue;

    invalid_table:
        free(components);
        return false;
    }

    if (local_base != entity_count)
        return false;
    *relations_begin = r->ptr;
    return true;
}

static bool ecs_scene_create_table_entities(
    ecs_scene_reader_t *r,
    uint32_t row_count,
    uint16_t component_count,
    ecs_entity_t *local_to_entity,
    uint32_t local_base,
    uint32_t total_entities
) {
    if (local_base > total_entities || row_count > total_entities - local_base)
        return false;

    ecs_component_t *components =
        component_count ? malloc((size_t)component_count * sizeof(ecs_component_t)) : NULL;
    if (component_count && !components)
        return false;

    for (uint16_t i = 0; i < component_count; i++) {
        components[i] = ecs_scene_read_u16(r);
        if (!r->ok || !ecs_scene_validate_component_id(components[i])) {
            free(components);
            return false;
        }
        if (i && components[i - 1] >= components[i]) {
            free(components);
            return false;
        }
    }

    ecs_type_t type = { 0 };
    if (component_count) {
        type = ecs_type_with_ids(&type, components, component_count);
    }
    uint16_t table_id = component_count ? ecs_table_index_get_or_create(type) : 0;

    for (uint32_t row = 0; row < row_count; row++) {
        ecs_entity_t entity = ecs_new();
        if (table_id != 0) {
            ecs_entity_record_t *record = ecs_get_record(entity);
            ecs_table_t *from = ecs_get_table(record->table_id);
            ecs_migrate(record, entity, from, table_id, 0);
        }
        local_to_entity[local_base + row] = entity;
    }

    /* Skip column payloads during pass 1. */
    for (uint16_t i = 0; i < component_count; i++) {
        uint32_t payload_size = ecs_scene_read_u32(r);
        if (!r->ok || !ecs_scene_reader_skip(r, payload_size)) {
            free(components);
            return false;
        }
    }

    free(components);
    return true;
}

static bool ecs_scene_first_pass(
    ecs_scene_reader_t *r,
    uint32_t table_count,
    uint32_t entity_count,
    ecs_entity_t *local_to_entity
) {
    uint32_t local_base = 0;

    for (uint32_t t = 0; t < table_count; t++) {
        uint32_t row_count = ecs_scene_read_u32(r);
        uint16_t component_count = ecs_scene_read_u16(r);
        (void)ecs_scene_read_u16(r); /* reserved */
        if (!r->ok)
            return false;

        if (!ecs_scene_create_table_entities(
                r,
                row_count,
                component_count,
                local_to_entity,
                local_base,
                entity_count
            )) {
            return false;
        }
        local_base += row_count;
    }

    return local_base == entity_count;
}

static bool ecs_scene_apply_on_add_for_table(
    ecs_entity_t *entities,
    uint32_t row_count,
    const ecs_component_t *components,
    uint16_t component_count
) {
    for (uint16_t c = 0; c < component_count; c++) {
        ecs_component_t component = components[c];
        ecs_component_record_t *record = ecs_component_index_get(component);
        if (!record->on_add)
            continue;

        for (uint32_t row = 0; row < row_count; row++) {
            void *value = record->info->size ? ecs_get_cid(entities[row], component) : NULL;
            record->on_add(entities[row], component, value);
        }
    }
    return true;
}

static bool ecs_scene_load_component_payload(
    ecs_scene_reader_t *r,
    uint32_t payload_size,
    ecs_component_t component,
    ecs_entity_t *entities,
    uint32_t row_count,
    const ecs_scene_load_ctx_t *ctx
) {
    const ecs_component_record_t *record = ecs_component_index_get(component);
    uint64_t component_size64 = record->info->size;
    if (component_size64 > UINT32_MAX)
        return false;
    uint32_t component_size = (uint32_t)component_size64;

    if (!ecs_scene_component_supported(record))
        return false;
    if (!component_size)
        return payload_size == 0;
    if ((size_t)payload_size > (size_t)(r->end - r->ptr))
        return false;

    const unsigned char *payload_end = r->ptr + payload_size;
    ecs_scene_reader_t payload = { .ptr = r->ptr, .end = payload_end, .ok = true };

    bool codec = ecs_scene_component_use_codec(record);
    if (!codec) {
        size_t expected = (size_t)component_size * row_count;
        if (expected != payload_size)
            return false;

        for (uint32_t row = 0; row < row_count; row++) {
            void *dst = ecs_get_cid(entities[row], component);
            if (!ecs_scene_reader_take(&payload, dst, component_size))
                return false;
        }
    } else {
        if (record->info->type == SIREFLECT_INVALID_HANDLE)
            return false;

        ecs_scene_load_ctx_t component_ctx = *ctx;
        component_ctx.component_owns_strings = record->ops.dtor != NULL;

        for (uint32_t row = 0; row < row_count; row++) {
            void *temp = calloc(1, component_size);
            if (!temp)
                return false;

            bool ok = ecs_scene_load_value(&payload, record->info->type, temp, &component_ctx);
            if (ok)
                ecs_set_cid(entities[row], component, temp);
            if (record->ops.dtor)
                record->ops.dtor(temp, 1);
            free(temp);
            if (!ok)
                return false;
        }
    }

    if (!payload.ok || payload.ptr != payload.end)
        return false;
    r->ptr = payload_end;
    return true;
}

static bool ecs_scene_second_pass(
    ecs_scene_reader_t *r,
    uint32_t table_count,
    uint32_t entity_count,
    ecs_entity_t *local_to_entity
) {
    ecs_scene_load_ctx_t ctx = {
        .local_to_entity = local_to_entity,
        .entity_count = entity_count,
    };

    uint32_t local_base = 0;

    for (uint32_t t = 0; t < table_count; t++) {
        uint32_t row_count = ecs_scene_read_u32(r);
        uint16_t component_count = ecs_scene_read_u16(r);
        (void)ecs_scene_read_u16(r);
        if (!r->ok || local_base > entity_count || row_count > entity_count - local_base) {
            return false;
        }

        ecs_component_t *components =
            component_count ? malloc((size_t)component_count * sizeof(ecs_component_t)) : NULL;
        if (component_count && !components)
            return false;

        for (uint16_t i = 0; i < component_count; i++) {
            components[i] = ecs_scene_read_u16(r);
            if (!r->ok || !ecs_scene_validate_component_id(components[i])) {
                free(components);
                return false;
            }
        }

        ecs_entity_t *entities = local_to_entity + local_base;
        ecs_scene_apply_on_add_for_table(entities, row_count, components, component_count);

        for (uint16_t i = 0; i < component_count; i++) {
            uint32_t payload_size = ecs_scene_read_u32(r);
            if (!r->ok || !ecs_scene_load_component_payload(
                              r,
                              payload_size,
                              components[i],
                              entities,
                              row_count,
                              &ctx
                          )) {
                free(components);
                return false;
            }
        }

        free(components);
        local_base += row_count;
    }

    return local_base == entity_count;
}

static bool ecs_scene_load_relations(
    ecs_scene_reader_t *r,
    uint32_t relation_edge_count,
    ecs_entity_t *local_to_entity,
    uint32_t entity_count
) {
    for (uint32_t i = 0; i < relation_edge_count; i++) {
        uint32_t source = ecs_scene_read_u32(r);
        uint16_t relation = ecs_scene_read_u16(r);
        (void)ecs_scene_read_u16(r);
        uint32_t target = ecs_scene_read_u32(r);

        if (!r->ok || source >= entity_count || target >= entity_count || relation == 0 ||
            relation >= relation_index.records.size) {
            return false;
        }

        const ecs_relation_record_t *relation_record = ecs_relation_record(relation);
        if (!relation_record->info.name && !relation_record->component && !relation_record->ops) {
            return false;
        }

        ecs_relate_id(local_to_entity[source], relation, local_to_entity[target]);
    }
    return true;
}

SIECS_API bool ecs_load_memory(const void *input, size_t size) {
    if (!input || !ecs_scene_validate(input, size))
        return false;

    const unsigned char *data = input;

    ecs_scene_reader_t header_reader = {
        .ptr = data,
        .end = data + size,
        .ok = true,
    };

    uint32_t table_count = 0;
    uint32_t entity_count = 0;
    uint32_t relation_edge_count = 0;
    if (!ecs_scene_read_header(&header_reader, &table_count, &entity_count, &relation_edge_count)) {
        return false;
    }

    const unsigned char *tables_begin = header_reader.ptr;
    ecs_entity_t *local_to_entity =
        entity_count ? malloc((size_t)entity_count * sizeof(ecs_entity_t)) : NULL;
    if (entity_count && !local_to_entity) {
        return false;
    }

    ecs_scene_reader_t first = {
        .ptr = tables_begin,
        .end = data + size,
        .ok = true,
    };

    bool ok = ecs_scene_first_pass(&first, table_count, entity_count, local_to_entity);
    const unsigned char *relations_begin = first.ptr;

    if (ok) {
        ecs_scene_reader_t second = {
            .ptr = tables_begin,
            .end = relations_begin,
            .ok = true,
        };
        ok = ecs_scene_second_pass(&second, table_count, entity_count, local_to_entity) &&
             second.ptr == second.end;
    }

    if (ok) {
        ecs_scene_reader_t relations = {
            .ptr = relations_begin,
            .end = data + size,
            .ok = true,
        };
        ok = ecs_scene_load_relations(
                 &relations,
                 relation_edge_count,
                 local_to_entity,
                 entity_count
             ) &&
             relations.ptr == relations.end;
    }

    /*
     * A malformed file may have instantiated some entities before returning
     * false. The API intentionally stays minimal; callers should only load
     * trusted scene files produced by ecs_save().
     */
    free(local_to_entity);
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
