#ifndef SIECS_STORAGE_COMPONENT_INDEX_H
#define SIECS_STORAGE_COMPONENT_INDEX_H
#include "siecs.h"
#if SIECS_HAS_META && !defined(SIREFLECT_H)
#include "sireflect.h"
#endif
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EcsComponentRelationTarget = 1 << 0,
    EcsComponentRelationSource = 1 << 1,
} ecs_component_internal_flags_t;

#define ECS_COMPONENT_RELATION_ID(flags) ((ecs_relation_id_t)((flags) >> 16))
#define ECS_COMPONENT_RELATION_FLAGS(id, flags) ((uint32_t)(flags) | ((uint32_t)(id) << 16))

typedef struct {
    ecs_component_info_t *info;
    uint16_t *required;
    uint32_t required_count;
    uint32_t size;
    ecs_type_ops_t ops;
    ecs_component_on_set_t on_set;
    ecs_component_on_remove_t on_remove;
    ecs_component_on_add_t on_add;
    uint32_t relation_flags;
    sicore_vec_t tables; // uint16_t
#if SIECS_HAS_META
    const sireflect_struct_desc_t *reflection_desc;
#endif
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    sicore_vec_t components; // ecs_component_record_t
} ecs_component_index_t;

void ecs_component_index_register(
    ecs_component_t id,
#if SIECS_HAS_NAMES
    const char *name,
#endif
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags
#if SIECS_HAS_META
    ,
    sireflect_handle_t type,
    const sireflect_struct_desc_t *reflection_desc
#endif
);

#define ecs_component_index_get(id)                                                                \
    sicore_vec_get(&ecs_world.component_index.components, id, ecs_component_record_t)
#define ecs_component_index_get_mut(id)                                                            \
    sicore_vec_get_mut(&ecs_world.component_index.components, id, ecs_component_record_t)

void ecs_component_index_init();
void ecs_component_index_fini();

void ecs_component_value_ctor(const ecs_component_record_t *record, void *dst, uint32_t count);
void ecs_component_value_dtor(const ecs_component_record_t *record, void *ptr, uint32_t count);
void ecs_component_value_copy_ctor(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
);
void ecs_component_value_copy(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
);
void ecs_component_value_move_ctor(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
);
void ecs_component_value_move(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
);

#endif
