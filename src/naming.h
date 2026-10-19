#ifndef SIECS_NAMING_H
#define SIECS_NAMING_H

#include "siecs.h"

typedef struct {
    ecs_entity_t parent;
    ecs_entity_t entity;
    const char *name; /* borrowed; stable until the entry is removed */
    uint32_t hash;
    uint32_t length;
} ecs_name_entry_t;

typedef struct {
    ecs_name_entry_t *entries;
    uint32_t capacity;
    uint32_t count;
    uint32_t deleted;
} ecs_name_index_t;

void ecs_name_index_fini(ecs_name_index_t *index);
void ecs_names_fini(void);
void ecs_names_init(void);
ecs_entity_t ecs_name_index_find(
    const ecs_name_index_t *index,
    ecs_entity_t parent,
    const char *name,
    size_t length
);
void ecs_name_index_insert(
    ecs_name_index_t *index,
    ecs_entity_t parent,
    const char *name,
    ecs_entity_t entity
);
void ecs_name_index_remove(
    ecs_name_index_t *index,
    ecs_entity_t parent,
    const char *name,
    ecs_entity_t entity
);

bool ecs_name_entity_alive(ecs_entity_t entity);
bool ecs_name_valid(const char *name);
const char *ecs_name_local(ecs_entity_t entity);
ecs_entity_t ecs_name_parent(ecs_entity_t entity);
ecs_entity_t ecs_name_lookup_segment(ecs_entity_t parent, const char *name, size_t length);
void ecs_name_unregister(ecs_entity_t entity);
void ecs_name_on_remove(ecs_entity_t entity, ecs_component_t component, void *data);
bool ecs_name_update_now(
    ecs_entity_t entity,
    const char *name,
    bool set_name,
    ecs_entity_t parent,
    bool set_parent
);
/* Apply the structural ChildOf change without emitting its public relation event. */
void ecs_childof_update_now(ecs_entity_t entity, ecs_entity_t parent);
void ecs_emit_relation_event(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_event_t event,
    ecs_entity_t old_target,
    ecs_entity_t new_target
);

#endif
