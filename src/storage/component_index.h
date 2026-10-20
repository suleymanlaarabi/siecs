#ifndef SIECS_STORAGE_COMPONENT_INDEX_H
#define SIECS_STORAGE_COMPONENT_INDEX_H
#include "../value.h"
#include "siecs.h"
#include "sireflect.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define ECS_COMPONENT_REQUIRE_CAPACITY 32

typedef enum {
    EcsComponentRelationTarget = 1 << 0,
    EcsComponentRelationSource = 1 << 1,
} ecs_component_internal_flags_t;

typedef struct {
    ecs_relation_id_t relation;
    ecs_entity_t target;
} ecs_component_required_relation_t;

#define ECS_COMPONENT_RELATION_ID(flags) ((ecs_relation_id_t)((flags) >> 16))
#define ECS_COMPONENT_RELATION_FLAGS(id, flags) ((uint32_t)(flags) | ((uint32_t)(id) << 16))

typedef struct {
    uint32_t size; // info->size, kept in the record for structural fast paths
    // Sized value without lifecycle ops or hooks, not a relation storage
    // component: setting it is a plain byte copy no user code can observe
    // unless an observer is registered.
    bool plain;
    ecs_component_info_t *info;
    uint16_t *required;
    uint32_t required_count;
    ecs_component_required_relation_t *default_relations;
    uint16_t default_relation_count;
    ecs_type_ops_t ops;
    ecs_component_on_set_t on_set;
    ecs_component_on_remove_t on_remove;
    ecs_component_on_add_t on_add;
    uint32_t relation_flags;
    sicore_vec_t tables; // uint16_t
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    sicore_vec_t components; // ecs_component_record_t
    sicore_map_t names; // public component name -> id; borrowed metadata keys
} ecs_component_index_t;

extern ecs_component_index_t component_index;

void ecs_component_index_register(
    ecs_component_t id,
    const ecs_component_desc_t *desc,
    uint32_t relation_flags,
    sireflect_handle_t type
);

void ecs_component_index_init();
void ecs_component_index_fini();

static inline ecs_component_record_t *ecs_component_index_get(ecs_component_t cid) {
    return sicore_vec_get_mut(&component_index.components, cid, ecs_component_record_t);
}

static inline ecs_component_required_relation_t *
ecs_component_default_relations(ecs_component_t component) {
    return ecs_component_index_get(component)->default_relations;
}

#endif
