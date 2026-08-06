#ifndef SIECS_RELATION_H
#define SIECS_RELATION_H

#include "siecs.h"
#include "table.h"

typedef struct {
    ecs_component_t component;
    uint8_t storage;
    uint8_t on_delete_target;
    uint8_t acyclic;

    char *name;
    ecs_relation_info_t info;

} ecs_relation_record_t;

typedef struct {
    sicore_vec_t records; /* ecs_relation_record_t */
} ecs_relation_index_t;

void ecs_relation_index_init(void);
void ecs_relation_index_fini(void);
void ecs_relation_target_on_remove(ecs_entity_t target, ecs_component_t component, void *ptr);
void ecs_relate_id_now(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target);
void ecs_unrelate_id_now(ecs_entity_t entity, ecs_relation_id_t relation);

ecs_component_t ecs_component_register_relation_internal(
    const char *name,
    ecs_relation_id_t relation,
    bool by_target
);

#define ecs_relation_record(id)                                                                    \
    sicore_vec_get(&ecs_world.relation_index.records, id, ecs_relation_record_t)

ecs_entity_t
ecs_relation_target_at_table(const ecs_table_t *table, ecs_relation_id_t relation, uint32_t row);

#endif
