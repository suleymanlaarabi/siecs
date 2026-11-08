#ifndef SIECS_PAIR_H
#define SIECS_PAIR_H
#include "siecs.h"

typedef enum { EcsPairAdd, EcsPairSet, EcsPairRemove, EcsPairModified } ecs_pair_op_t;
bool ecs_pair_apply(ecs_entity_t entity, ecs_pair_id_t pair, ecs_pair_op_t op, const void *value);
void ecs_pair_entity_fini(ecs_entity_t entity);
void ecs_pair_storage_fini(void);
void ecs_pair_inherit(ecs_entity_t entity, ecs_entity_t base);
typedef struct ecs_pair_schema_t ecs_pair_schema_t;
bool ecs_pair_schema_prepare(
    ecs_component_t component,
    sireflect_handle_t type,
    ecs_pair_schema_t **out
);
bool ecs_pair_schema_validate(const ecs_pair_schema_t *plan);
void ecs_pair_schema_apply(ecs_pair_schema_t *plan);
void ecs_pair_schema_notify(const ecs_pair_schema_t *plan);
void ecs_pair_schema_free(ecs_pair_schema_t *plan);

#endif
