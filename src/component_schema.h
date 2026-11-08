#ifndef SIECS_COMPONENT_SCHEMA_H
#define SIECS_COMPONENT_SCHEMA_H
#include "siecs.h"

typedef struct ecs_component_schema_t ecs_component_schema_t;
/* Prepare all replacement columns without changing the registry or tables. */
ecs_component_schema_t *ecs_component_schema_prepare(
    ecs_component_t component,
    sireflect_handle_t type,
    const char *fields
);
bool ecs_component_schema_validate(const ecs_component_schema_t *schema);
void ecs_component_schema_apply(ecs_component_schema_t *schema);
void ecs_component_schema_notify(
    const ecs_component_schema_t *schema,
    bool (*already_written)(ecs_entity_t, ecs_component_t, void *),
    void *context
);
void ecs_component_schema_free(ecs_component_schema_t *schema);
#endif
