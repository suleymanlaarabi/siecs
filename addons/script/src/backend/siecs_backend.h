#ifndef SIECS_SCRIPT_BACKEND_H
#define SIECS_SCRIPT_BACKEND_H
#include "../script.h"

typedef enum script_symbol_kind_t {
    ScriptSymbolNone,
    ScriptSymbolEntity,
    ScriptSymbolComponent,
    ScriptSymbolRelation,
    ScriptSymbolReflectType
} script_symbol_kind_t;

typedef struct script_symbol_t {
    script_symbol_kind_t kind;
    union {
        ecs_entity_t entity;
        ecs_component_t component;
        ecs_relation_id_t relation;
        sireflect_handle_t type;
    } id;
} script_symbol_t;

ecs_entity_t script_backend_entity_lookup(const char *path, ecs_entity_t parent);
ecs_entity_t script_backend_entity_ensure(const char *path, ecs_entity_t parent);
ecs_component_t script_backend_component_lookup(const char *name);
ecs_component_t script_backend_component_register_dynamic(const char *name, const char *fields);
script_symbol_t script_backend_symbol_lookup(const char *name, ecs_entity_t parent);
int script_backend_relation_add(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
);
void script_backend_component_add(ecs_entity_t entity, ecs_component_t component);
void script_backend_component_set(
    ecs_entity_t entity,
    ecs_component_t component,
    const void *value
);
bool script_backend_component_has(ecs_entity_t entity, ecs_component_t component);
const void *script_backend_component_get(ecs_entity_t entity, ecs_component_t component);
ecs_component_t script_backend_tag_register(const char *name);
const ecs_component_info_t *script_backend_component_info(ecs_component_t component);
char *script_backend_entity_path(ecs_entity_t entity);
void script_backend_isa_add(ecs_entity_t entity, ecs_entity_t target);
void script_backend_prefab(ecs_entity_t entity);
const ecs_type_ops_t *script_backend_value_ops(ecs_component_t component);
void script_backend_value_init(ecs_component_t component, void *destination, const void *source);
void script_backend_value_copy(ecs_component_t component, void *destination, const void *source);
void script_backend_value_fini(ecs_component_t component, void *value);
sireflect_handle_t script_backend_type_lookup(const char *name);
#endif
