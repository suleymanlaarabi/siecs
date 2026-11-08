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

char *script_backend_qualified_name(const char *name, ecs_entity_t scope);
ecs_entity_t script_backend_entity_lookup(const char *path, ecs_entity_t parent);
ecs_entity_t script_backend_entity_ensure(const char *path, ecs_entity_t parent);
ecs_component_t script_backend_component_lookup(const char *name);
ecs_component_t script_backend_component_register_dynamic(const char *name, const char *fields);
int script_backend_relation_add(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
);
bool script_key_valid(script_key_t key);
bool script_key_has(ecs_entity_t entity, script_key_t key);
bool script_key_owned(ecs_entity_t entity, script_key_t key);
const void *script_key_get(ecs_entity_t entity, script_key_t key);
void script_key_write(ecs_entity_t entity, script_key_t key, const void *value, bool remove);
const ecs_component_info_t *script_key_info(script_key_t key);
const ecs_type_ops_t *script_key_ops(script_key_t key);
bool script_key_event(script_key_t key, const ecs_observer_event_t *event);
ecs_observer_id_t script_key_observe(
    ecs_entity_t entity,
    script_key_t key,
    uint32_t slot,
    ecs_observer_callback_t callback,
    uintptr_t context,
    void (*destroy)(uintptr_t)
);
void script_backend_component_add(ecs_entity_t entity, script_key_t component);
void script_backend_component_set(ecs_entity_t entity, script_key_t component, const void *value);
bool script_backend_component_has(ecs_entity_t entity, script_key_t component);
ecs_component_t script_backend_tag_register(const char *name);
const ecs_component_info_t *script_backend_component_info(script_key_t component);
char *script_backend_entity_path(ecs_entity_t entity);
void script_backend_isa_add(ecs_entity_t entity, ecs_entity_t target);
void script_backend_prefab(ecs_entity_t entity);
const ecs_type_ops_t *script_backend_value_ops(script_key_t component);
void script_backend_value_init(script_key_t component, void *destination, const void *source);
void script_backend_value_copy(script_key_t component, void *destination, const void *source);
void script_backend_value_fini(script_key_t component, void *value);
sireflect_handle_t script_backend_type_lookup(const char *name);
#endif
