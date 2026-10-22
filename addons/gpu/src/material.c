#include "sigpu_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Materials (RFC section 7). A parameter component is tied to a program by
 * sigpu_program_params; an entity with that component is a material, a GPU
 * resource of its own. Its uniforms are packed into the program's material
 * storage buffer at a stable slot, its textures form a texture set shared
 * with the materials that use the same ones.
 */

typedef enum { COPY_RAW, COPY_UNORM8, COPY_BOOL } copy_kind_t;

/* One field of the packing plan (RFC section 10.3). */
struct gpu_copy_s {
    uint32_t source;
    uint32_t destination;
    uint32_t count;
    uint8_t kind;
};

struct gpu_texture_field_s {
    char name[GPU_PACK_NAME_SIZE];
    uint32_t texture;
    uint32_t sampler;
    bool has_sampler;
};

/* Entity -> material index, open addressing; written on the main thread in
 * GpuSync only, read by the extraction systems. Index 0 means no material. */
static ecs_entity_t *index_keys;
static uint32_t *index_values;
static uint32_t index_capacity;

static uint32_t hash_entity(ecs_entity_t entity) {
    uint64_t x = entity * 0x9e3779b97f4a7c15ull;
    return (uint32_t)(x >> 32);
}

uint32_t gpu_material_index(ecs_entity_t material) {
    if (!index_capacity) {
        return GPU_NONE;
    }
    for (uint32_t i = hash_entity(material) & (index_capacity - 1);;
         i = (i + 1) & (index_capacity - 1)) {
        if (index_keys[i] == material) {
            return index_values[i];
        }
        if (!index_keys[i]) {
            return GPU_NONE;
        }
    }
}

static void index_insert(ecs_entity_t material, uint32_t value);

static bool index_grow(void) {
    const uint32_t capacity = index_capacity ? index_capacity * 2 : 256;
    ecs_entity_t *old_keys = index_keys;
    uint32_t *old_values = index_values;
    const uint32_t old_capacity = index_capacity;
    index_keys = calloc(capacity, sizeof(ecs_entity_t));
    index_values = calloc(capacity, sizeof(uint32_t));
    if (!index_keys || !index_values) {
        free(index_keys);
        free(index_values);
        index_keys = old_keys;
        index_values = old_values;
        return false;
    }
    index_capacity = capacity;
    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old_keys[i]) {
            index_insert(old_keys[i], old_values[i]);
        }
    }
    free(old_keys);
    free(old_values);
    return true;
}

static void index_insert(ecs_entity_t material, uint32_t value) {
    if ((gpu_session.materials.count + 1) * 2 > index_capacity && !index_grow()) {
        return;
    }
    for (uint32_t i = hash_entity(material) & (index_capacity - 1);;
         i = (i + 1) & (index_capacity - 1)) {
        if (!index_keys[i] || index_keys[i] == material) {
            index_keys[i] = material;
            index_values[i] = value;
            return;
        }
    }
}

/* The stable index of a material entity, created on first use. */
static gpu_material_t *material_entry(ecs_entity_t entity) {
    if (!gpu_session.materials.count) {
        /* Index 0: rows without a material. */
        gpu_vec_push(gpu_session.materials, ((gpu_material_t){ 0 }));
    }
    uint32_t index = gpu_material_index(entity);
    if (index == GPU_NONE) {
        if (!gpu_vec_push(gpu_session.materials, ((gpu_material_t){ .entity = entity,
                                                                    .slot = GPU_NONE }))) {
            return NULL;
        }
        index = gpu_session.materials.count - 1;
        index_insert(entity, index);
    }
    return &gpu_session.materials.items[index];
}

gpu_params_t *gpu_params_of(ecs_component_t component) {
    for (uint32_t i = 0; i < gpu_session.params.count; i++) {
        if (gpu_session.params.items[i].params == component) {
            return &gpu_session.params.items[i];
        }
    }
    return NULL;
}

gpu_params_t *gpu_params_for_program(ecs_entity_t program) {
    for (uint32_t i = 0; i < gpu_session.params.count; i++) {
        if (gpu_session.params.items[i].program == program) {
            return &gpu_session.params.items[i];
        }
    }
    return NULL;
}

bool sigpu_program_params(ecs_entity_t program, ecs_component_t params) {
    const ecs_component_info_t *info = ecs_component_info(params);
    if (!gpu_session.open || !info || !info->size || !info->type || !ecs_is_alive(program) ||
        !ecs_has(program, GpuProgram)) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "sigpu: parameters need a reflected component and a GpuProgram");
        return false;
    }
    gpu_params_t *existing = gpu_params_of(params);
    if (existing) {
        if (existing->program != program) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: %s already parameterizes another program",
                         info->name);
            return false;
        }
        return true;
    }
    if (!gpu_vec_push(gpu_session.params, ((gpu_params_t){ .program = program,
                                                           .params = params }))) {
        return false;
    }
    ecs_with_many(params, ecs_id(GpuResourceState), 0);
    /* Materials created before the call become resources too. */
    GPU_VEC(ecs_entity_t) existing_materials = { 0 };
    ecs_query_desc_t query = { .components = { { params, EcsFilter }, ecs_in_optional(Abstract),
                                               ecs_in_optional(Disabled) } };
    const ecs_query_id_t materials = ecs_query_init(&query);
    for (ecs_iter_t it = ecs_query_iter(materials); ecs_iter_next(&it);) {
        for (uint32_t i = 0; i < it.count; i++) {
            gpu_vec_push(existing_materials, it.entities[i]);
        }
    }
    ecs_query_fini(materials);
    for (uint32_t i = 0; i < existing_materials.count; i++) {
        if (ecs_has(existing_materials.items[i], GpuResourceState)) {
            gpu_touch(existing_materials.items[i], true);
        } else {
            ecs_add_cid(existing_materials.items[i], ecs_id(GpuResourceState));
        }
    }
    gpu_vec_free(existing_materials);
    return true;
}

/* ----------------------------------------------------------------------
 * Packing plan
 * ---------------------------------------------------------------------- */

static const sireflect_field_info_t *cpu_field(sireflect_handle_t type, const char *name) {
    const sireflect_fields_t *fields = sireflect_type_fields(type);
    for (size_t i = 0; fields && i < fields->field_count; i++) {
        if (strcmp(fields->fields[i].name, name) == 0) {
            return &fields->fields[i];
        }
    }
    return NULL;
}

/* Scalar kind of a reflected field and its element count (arrays). */
static bool cpu_shape(const sireflect_field_info_t *field, sireflect_kind_t *kind, uint32_t *count,
                      bool *entity) {
    const sireflect_type_info_t *info = sireflect_type_info(field->type);
    if (!info) {
        return false;
    }
    *entity = info->kind == sireflect_kind_struct && strcmp(info->name, "ecs_entity_t") == 0;
    *count = 1;
    if (info->kind == sireflect_kind_array) {
        const sireflect_type_info_t *element = sireflect_type_info(info->element_type);
        if (!element) {
            return false;
        }
        *kind = element->kind;
        *count = (uint32_t)info->element_count;
        return true;
    }
    *kind = info->kind;
    return true;
}

static bool is_kind(sireflect_kind_t kind, sireflect_kind_t a, sireflect_kind_t b,
                    sireflect_kind_t c) {
    return kind == a || kind == b || kind == c;
}

static bool plan_fail(gpu_params_t *params, const char *format, const char *field) {
    params->valid = false;
    snprintf(params->message, sizeof(params->message), format, field);
    SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: %s: %s",
                 ecs_component_name(params->params), params->message);
    return false;
}

/* The material blocks of a program: its storage buffer of scope material
 * and its material samplers, in every graphics stage. */
static bool plan_build(gpu_params_t *params, const gpu_record_t *program) {
    free(params->copies);
    free(params->textures);
    params->copies = NULL;
    params->textures = NULL;
    params->copy_count = params->texture_count = 0;
    params->stride = 0;
    params->block[0] = '\0';
    params->valid = true;
    params->planned = true;
    params->message[0] = '\0';
    params->record = (uint32_t)(program - gpu_session.records.items) + 1;
    params->record_generation = program->generation;
    const gpu_pack_t *pack = gpu_program_pack(program);
    const sireflect_handle_t type = ecs_component_info(params->params)->type;
    const gpu_pack_resource_t *block = NULL;
    for (uint32_t s = 0; s < GPU_PROGRAM_STAGES; s++) {
        const gpu_program_stage_t *stage = &program->as.program.stages[s];
        const uint32_t entries[2] = { stage->vertex_entry, stage->fragment_entry };
        for (uint32_t e = 0; stage->used && e < 2; e++) {
            const gpu_pack_entry_t *entry = gpu_pack_entry_at(pack, entries[e]);
            for (uint32_t r = 0; entry && r < entry->resource_count; r++) {
                const gpu_pack_resource_t *resource = &pack->resources[entry->first_resource + r];
                if (resource->scope != GpuPackScopeMaterial) {
                    continue;
                }
                if (resource->kind == GpuPackStorageBuffer) {
                    if (block && strcmp(block->name, resource->name) != 0) {
                        return plan_fail(params, "two material blocks (%s)", resource->name);
                    }
                    block = resource;
                } else if (resource->kind == GpuPackSampler) {
                    bool known = false;
                    for (uint32_t t = 0; t < params->texture_count; t++) {
                        known = known || strcmp(params->textures[t].name, resource->name) == 0;
                    }
                    if (known) {
                        continue;
                    }
                    const sireflect_field_info_t *field = cpu_field(type, resource->name);
                    sireflect_kind_t kind;
                    uint32_t count;
                    bool entity = false;
                    if (!field || !cpu_shape(field, &kind, &count, &entity) || !entity) {
                        return plan_fail(params, "texture '%s' needs an ecs_entity_t field",
                                         resource->name);
                    }
                    struct gpu_texture_field_s *grown = realloc(
                        params->textures, (params->texture_count + 1) * sizeof(*grown));
                    if (!grown || params->texture_count == 8) {
                        free(grown == params->textures ? NULL : grown);
                        return plan_fail(params, "more than 8 material textures (%s)",
                                         resource->name);
                    }
                    params->textures = grown;
                    struct gpu_texture_field_s *texture = &grown[params->texture_count++];
                    memset(texture, 0, sizeof(*texture));
                    SDL_strlcpy(texture->name, resource->name, sizeof(texture->name));
                    texture->texture = (uint32_t)field->offset;
                    char sampler_name[GPU_PACK_NAME_SIZE + 8];
                    snprintf(sampler_name, sizeof(sampler_name), "%s_sampler", resource->name);
                    const sireflect_field_info_t *sampler = cpu_field(type, sampler_name);
                    if (sampler && cpu_shape(sampler, &kind, &count, &entity) && entity) {
                        texture->sampler = (uint32_t)sampler->offset;
                        texture->has_sampler = true;
                    }
                }
            }
        }
    }
    if (!block) {
        return true;
    }
    SDL_strlcpy(params->block, block->name, sizeof(params->block));
    params->stride = block->stride;
    params->copies = calloc(block->field_count ? block->field_count : 1, sizeof(struct gpu_copy_s));
    if (!params->copies) {
        return plan_fail(params, "out of memory (%s)", block->name);
    }
    for (uint32_t f = 0; f < block->field_count; f++) {
        const gpu_pack_field_t *gpu = &pack->fields[block->first_field + f];
        const sireflect_field_info_t *field = cpu_field(type, gpu->name);
        if (!field) {
            continue; /* Left zero: padding and unused fields are initialized. */
        }
        sireflect_kind_t kind;
        uint32_t count;
        bool entity;
        if (!cpu_shape(field, &kind, &count, &entity) || entity) {
            return plan_fail(params, "field '%s' cannot be packed", gpu->name);
        }
        const uint32_t components = gpu->rows * gpu->columns;
        struct gpu_copy_s copy = { (uint32_t)field->offset, gpu->offset, components, COPY_RAW };
        const bool float_kind = kind == sireflect_kind_f32;
        const bool int_kind = is_kind(kind, sireflect_kind_i32, sireflect_kind_u32,
                                      sireflect_kind_int) ||
                              kind == sireflect_kind_unsigned_int;
        const bool byte_kind = is_kind(kind, sireflect_kind_u8, sireflect_kind_unsigned_char,
                                       sireflect_kind_u8);
        if (gpu->scalar == GpuPackScalarFloat32 && float_kind && count == components) {
            copy.kind = COPY_RAW;
        } else if (gpu->scalar == GpuPackScalarFloat32 && byte_kind && count == components &&
                   gpu->rows == 1) {
            copy.kind = COPY_UNORM8;
        } else if ((gpu->scalar == GpuPackScalarInt32 || gpu->scalar == GpuPackScalarUInt32) &&
                   int_kind && count == components) {
            copy.kind = COPY_RAW;
        } else if ((gpu->scalar == GpuPackScalarBool || gpu->scalar == GpuPackScalarUInt32) &&
                   kind == sireflect_kind_bool && count == components) {
            copy.kind = COPY_BOOL;
        } else {
            return plan_fail(params, "field '%s' does not match the shader type", gpu->name);
        }
        if (gpu->offset + components * 4 > block->stride) {
            return plan_fail(params, "field '%s' is outside the block", gpu->name);
        }
        params->copies[params->copy_count++] = copy;
    }
    return true;
}

static void pack_material(const gpu_params_t *params, const uint8_t *source, uint8_t *destination) {
    memset(destination, 0, params->stride);
    for (uint32_t i = 0; i < params->copy_count; i++) {
        const struct gpu_copy_s *copy = &params->copies[i];
        uint8_t *out = destination + copy->destination;
        const uint8_t *in = source + copy->source;
        switch (copy->kind) {
        case COPY_RAW:
            memcpy(out, in, copy->count * 4);
            break;
        case COPY_UNORM8:
            for (uint32_t c = 0; c < copy->count; c++) {
                const float value = in[c] / 255.0f;
                memcpy(out + c * 4, &value, 4);
            }
            break;
        case COPY_BOOL:
            for (uint32_t c = 0; c < copy->count; c++) {
                const uint32_t value = ((const bool *)in)[c] ? 1u : 0u;
                memcpy(out + c * 4, &value, 4);
            }
            break;
        default:
            break;
        }
    }
}

/* ----------------------------------------------------------------------
 * Texture sets
 * ---------------------------------------------------------------------- */

static uint32_t texset_find(const gpu_texset_t *set) {
    for (uint32_t i = 1; i < gpu_session.texsets.count; i++) {
        const gpu_texset_t *other = &gpu_session.texsets.items[i];
        if (other->count == set->count &&
            memcmp(other->bindings, set->bindings, set->count * sizeof(set->bindings[0])) == 0) {
            return i;
        }
    }
    return 0;
}

static uint32_t texset_acquire(const gpu_texset_t *set) {
    if (!set->count) {
        return 0;
    }
    if (!gpu_session.texsets.count) {
        gpu_vec_push(gpu_session.texsets, ((gpu_texset_t){ 0 }));
    }
    uint32_t id = texset_find(set);
    if (!id) {
        for (uint32_t i = 1; i < gpu_session.texsets.count && !id; i++) {
            id = gpu_session.texsets.items[i].users ? 0 : i;
        }
        if (!id) {
            if (!gpu_vec_push(gpu_session.texsets, *set)) {
                return 0;
            }
            id = gpu_session.texsets.count - 1;
        }
        gpu_session.texsets.items[id] = *set;
        gpu_session.texsets.items[id].users = 0;
        gpu_session.texset_revision++;
    }
    gpu_session.texsets.items[id].users++;
    return id;
}

static void texset_release(uint32_t id) {
    if (id && id < gpu_session.texsets.count && gpu_session.texsets.items[id].users) {
        gpu_session.texsets.items[id].users--;
    }
}

const gpu_texset_t *gpu_texset(uint32_t id) {
    return id && id < gpu_session.texsets.count ? &gpu_session.texsets.items[id] : NULL;
}

/* ----------------------------------------------------------------------
 * Building
 * ---------------------------------------------------------------------- */

static gpu_params_t *params_of_entity(ecs_entity_t entity) {
    gpu_params_t *found = NULL;
    for (uint32_t i = 0; i < gpu_session.params.count; i++) {
        if (ecs_has_cid(entity, gpu_session.params.items[i].params)) {
            found = &gpu_session.params.items[i];
        }
    }
    return found;
}

static bool slot_alloc(gpu_params_t *params, uint32_t *slot) {
    if (params->free_slots.count) {
        *slot = params->free_slots.items[--params->free_slots.count];
        return true;
    }
    if (params->stride) {
        uint8_t *grown = realloc(params->data, (size_t)(params->slots + 1) * params->stride);
        if (!grown) {
            return false;
        }
        params->data = grown;
    }
    *slot = params->slots++;
    return true;
}

static void mark_dirty(gpu_params_t *params, uint32_t slot) {
    if (params->dirty_hi <= params->dirty_lo) {
        params->dirty_lo = slot;
        params->dirty_hi = slot + 1;
        return;
    }
    params->dirty_lo = slot < params->dirty_lo ? slot : params->dirty_lo;
    params->dirty_hi = slot + 1 > params->dirty_hi ? slot + 1 : params->dirty_hi;
}

static void material_release(gpu_material_t *material) {
    gpu_params_t *params = gpu_params_for_program(material->program);
    if (params && material->slot != GPU_NONE) {
        gpu_vec_push(params->free_slots, material->slot);
    }
    texset_release(material->texset);
    material->slot = GPU_NONE;
    material->texset = 0;
    material->ready = false;
    material->program = 0;
}

void gpu_material_removed(ecs_entity_t entity) {
    const uint32_t index = gpu_material_index(entity);
    if (index != GPU_NONE && index < gpu_session.materials.count) {
        material_release(&gpu_session.materials.items[index]);
    }
}

void gpu_material_build(ecs_entity_t entity, void *opaque) {
    gpu_result_t *result = opaque;
    gpu_params_t *params = params_of_entity(entity);
    gpu_material_t *material = material_entry(entity);
    if (!params || !material) {
        gpu_fail(result, GpuErrorInvalid, 0, "not a material");
        return;
    }
    if (material->program && material->program != params->program) {
        material_release(material);
    }
    material->ready = false;
    gpu_edge_add(params->program, entity);
    const gpu_record_t *program =
        gpu_dependency_ready(result, params->program, GPU_RESOURCE_PROGRAM, "program");
    if (!program) {
        return;
    }
    const uint32_t record = (uint32_t)(program - gpu_session.records.items) + 1;
    if (!params->planned || params->record != record ||
        params->record_generation != program->generation) {
        plan_build(params, program);
        params->upload_all = true;
    }
    if (!params->valid) {
        gpu_fail(result, GpuErrorInterface, params->program, "%s", params->message);
        return;
    }
    const uint8_t *values = ecs_get_cid(entity, params->params);
    gpu_texset_t set = { 0 };
    for (uint32_t t = 0; t < params->texture_count; t++) {
        const struct gpu_texture_field_s *field = &params->textures[t];
        ecs_entity_t texture, sampler = 0;
        memcpy(&texture, values + field->texture, sizeof(texture));
        if (field->has_sampler) {
            memcpy(&sampler, values + field->sampler, sizeof(sampler));
        }
        if (texture) {
            gpu_edge_add(texture, entity);
        }
        if (sampler) {
            gpu_edge_add(sampler, entity);
        }
        /* 0 is the white default texture; a dead or unready one waits. */
        if (texture && (!ecs_is_alive(texture) ||
                        !gpu_resource_record(texture, GPU_RESOURCE_TEXTURE))) {
            gpu_result_set(result, GpuWaiting, GpuErrorNone, texture,
                           "waiting for texture '%s'", field->name);
            return;
        }
        if (sampler && !gpu_resource_record(sampler, GPU_RESOURCE_SAMPLER)) {
            gpu_result_set(result, GpuWaiting, GpuErrorNone, sampler,
                           "waiting for sampler '%s_sampler'", field->name);
            return;
        }
        gpu_texset_binding_t *binding = &set.bindings[set.count++];
        SDL_strlcpy(binding->name, field->name, sizeof(binding->name));
        binding->texture = texture;
        binding->sampler = sampler;
    }
    if (material->slot == GPU_NONE && !slot_alloc(params, &material->slot)) {
        gpu_fail(result, GpuErrorSdl, 0, "out of memory");
        return;
    }
    if (params->stride) {
        pack_material(params, values, params->data + (size_t)material->slot * params->stride);
        mark_dirty(params, material->slot);
    }
    const uint32_t texset = texset_acquire(&set);
    texset_release(material->texset);
    material->texset = texset;
    material->program = params->program;
    material->ready = true;
    gpu_stats()->materials_prepared++;
    result->status = GpuReady;
    result->built.kind = GPU_RESOURCE_MATERIAL;
    result->built.as.material.program = params->program;
    result->built.as.material.slot = material->slot;
    result->built.as.material.texset = texset;
}

const gpu_material_t *gpu_material_at(uint32_t index) {
    return index < gpu_session.materials.count ? &gpu_session.materials.items[index] : NULL;
}

/* GpuPrepare: the material buffers hold every slot; changed slots upload. */
void gpu_materials_prepare(void) {
    for (uint32_t i = 0; i < gpu_session.params.count; i++) {
        gpu_params_t *params = &gpu_session.params.items[i];
        if (!params->stride || !params->slots) {
            continue;
        }
        if (params->buffer_slots < params->slots) {
            if (params->buffer) {
                gpu_uploads_drop_raw(params->buffer);
                gpu_release_buffer(params->buffer);
                gpu_stats()->memory_reserved -= (uint64_t)params->buffer_slots * params->stride;
            }
            uint32_t slots = params->buffer_slots ? params->buffer_slots : 64;
            while (slots < params->slots) {
                slots *= 2;
            }
            params->buffer = gpu_create_buffer_flags(slots * params->stride,
                                                     SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
            params->buffer_slots = params->buffer ? slots : 0;
            gpu_stats()->memory_reserved += (uint64_t)params->buffer_slots * params->stride;
            params->upload_all = true;
        }
        if (!params->buffer) {
            continue;
        }
        if (params->upload_all) {
            params->dirty_lo = 0;
            params->dirty_hi = params->slots;
            params->upload_all = false;
        }
        if (params->dirty_hi > params->dirty_lo) {
            gpu_upload_raw(params->buffer, params->dirty_lo * params->stride,
                           params->data + (size_t)params->dirty_lo * params->stride,
                           (params->dirty_hi - params->dirty_lo) * params->stride);
            params->dirty_lo = params->dirty_hi = 0;
        }
        gpu_stats()->memory_used += (uint64_t)params->slots * params->stride;
    }
}

void gpu_materials_release(void) {
    for (uint32_t i = 0; i < gpu_session.params.count; i++) {
        gpu_params_t *params = &gpu_session.params.items[i];
        if (params->buffer) {
            gpu_uploads_drop_raw(params->buffer);
            gpu_release_buffer(params->buffer);
            gpu_stats()->memory_reserved -= (uint64_t)params->buffer_slots * params->stride;
        }
        params->buffer = NULL;
        params->buffer_slots = 0;
        params->planned = false;
    }
}

void gpu_materials_fini(void) {
    for (uint32_t i = 0; i < gpu_session.params.count; i++) {
        gpu_params_t *params = &gpu_session.params.items[i];
        free(params->copies);
        free(params->textures);
        free(params->data);
        gpu_vec_free(params->free_slots);
    }
    gpu_vec_free(gpu_session.params);
    gpu_vec_free(gpu_session.materials);
    gpu_vec_free(gpu_session.texsets);
    free(index_keys);
    free(index_values);
    index_keys = NULL;
    index_values = NULL;
    index_capacity = 0;
}
