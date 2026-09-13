#ifndef SIECS_RELATION_H
#define SIECS_RELATION_H

#include "siecs.h"
#include "table.h"

typedef struct {
    void (*set_now)(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target);
    void (*remove_now)(ecs_entity_t entity, ecs_relation_id_t relation);
    bool (*has)(ecs_entity_t entity, ecs_relation_id_t relation);
    ecs_entity_t (*target)(ecs_entity_t entity, ecs_relation_id_t relation);
} ecs_relation_ops_t;

typedef struct {
    ecs_component_t component;
    ecs_relation_info_t info;
    const ecs_relation_ops_t *ops;
} ecs_relation_record_t;

typedef struct {
    sicore_vec_t records; /* ecs_relation_record_t */
} ecs_relation_index_t;

extern ecs_relation_index_t relation_index;

void ecs_relation_index_init(void);
void ecs_relation_index_fini(void);
ecs_relation_id_t ecs_relation_register_virtual(
    ecs_relation_id_t *id,
    const char *name,
    const ecs_relation_desc_t *desc,
    const ecs_relation_ops_t *ops
);
void ecs_relation_target_on_remove(ecs_entity_t target, ecs_component_t component, void *ptr);
void ecs_relate_id_now(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target);
void ecs_unrelate_id_now(ecs_entity_t entity, ecs_relation_id_t relation);
ecs_entity_t ecs_entity_base_raw(ecs_entity_t entity);

extern const ecs_relation_ops_t ecs_relation_ops_isa;

ecs_component_t ecs_component_register_relation_internal(
    const char *name,
    ecs_relation_id_t relation,
    bool by_target
);

#define ecs_relation_record(id)                                                                    \
    sicore_vec_get(&relation_index.records, id, ecs_relation_record_t)

ecs_entity_t
ecs_relation_target_at_table(const ecs_table_t *table, ecs_relation_id_t relation, uint32_t row);

#endif
