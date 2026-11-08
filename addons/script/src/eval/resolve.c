#include "../backend/transaction.h"
#include "eval.h"

/* Imports contain native namespace entities. They are lexical declarations,
 * not aliases or a second symbol registry. */

static uint64_t lookup(const char *name, ecs_entity_t scope, script_symbol_kind_t kind) {
    if (scope && !script_entity_is_placeholder(scope) && !ecs_is_alive(scope))
        return 0;
    if (kind == ScriptSymbolEntity) {
        if (script_transaction_active) {
            script_transaction_lookup(script_transaction_active, name, scope);
            return script_transaction_entity(script_transaction_active, name, scope, false);
        }
        char *native = script_backend_qualified_name(name, 0);
        ecs_entity_t entity = ecs_lookup_from(scope, native);
        free(native);
        return entity;
    }
    char *qualified =
        scope || strchr(name, '.') ? script_backend_qualified_name(name, scope) : NULL;
    const char *key = qualified ? qualified : name;
    uint64_t result = kind == ScriptSymbolComponent  ? script_backend_component_lookup(key)
                      : kind == ScriptSymbolRelation ? ecs_relation_lookup(key)
                                                     : script_backend_type_lookup(key);
    free(qualified);
    return result;
}

static uint64_t resolve(script_eval_t *ctx, const char *name, script_symbol_kind_t kind) {
    if (!name)
        return 0;
    if (!strncmp(name, "::", 2))
        return lookup(name + 2, 0, kind);
    for (ecs_entity_t scope = ctx->parent; scope;
         scope = script_transaction_active
                     ? script_transaction_parent(script_transaction_active, scope)
                     : ecs_target_id(scope, ecs_rid(ChildOf))) {
        uint64_t value = lookup(name, scope, kind);
        if (value)
            return value;
    }
    for (const script_import_t *import = ctx->imports; import; import = import->next) {
        uint64_t value = lookup(name, import->scope, kind);
        if (value)
            return value;
    }
    return lookup(name, 0, kind);
}

ecs_entity_t script_resolve_entity(script_eval_t *ctx, const char *name) {
    return resolve(ctx, name, ScriptSymbolEntity);
}

ecs_component_t script_resolve_component(script_eval_t *ctx, const char *name) {
    return (ecs_component_t)resolve(ctx, name, ScriptSymbolComponent);
}

sireflect_handle_t script_resolve_type(script_eval_t *ctx, const char *name) {
    return (sireflect_handle_t)resolve(ctx, name, ScriptSymbolReflectType);
}

script_symbol_t script_resolve_symbol(script_eval_t *ctx, const char *name) {
    ecs_component_t component = script_resolve_component(ctx, name);
    if (component)
        return (script_symbol_t){ ScriptSymbolComponent, { .component = component } };
    ecs_relation_id_t relation = (ecs_relation_id_t)resolve(ctx, name, ScriptSymbolRelation);
    if (relation)
        return (script_symbol_t){ ScriptSymbolRelation, { .relation = relation } };
    ecs_entity_t entity = script_resolve_entity(ctx, name);
    if (entity)
        return (script_symbol_t){ ScriptSymbolEntity, { .entity = entity } };
    sireflect_handle_t type = script_resolve_type(ctx, name);
    return type ? (script_symbol_t){ ScriptSymbolReflectType, { .type = type } }
                : (script_symbol_t){ 0 };
}

static ecs_pair_element_t pair_element(script_eval_t *ctx, const char *name) {
    if (name && name[0] == '$') {
        const script_variable_t *variable = script_find_variable(ctx, name + 1);
        if (variable && variable->value.type == script_backend_type_lookup("entity")) {
            ecs_entity_t entity;
            memcpy(&entity, script_value_data(&variable->value), sizeof entity);
            return (ecs_pair_element_t){ entity, EcsPairEntity };
        }
    }
    script_symbol_t symbol = script_resolve_symbol(ctx, name);
    switch (symbol.kind) {
    case ScriptSymbolComponent:
        return (ecs_pair_element_t){ symbol.id.component, EcsPairComponent };
    case ScriptSymbolEntity:
        return (ecs_pair_element_t){ symbol.id.entity, EcsPairEntity };
    case ScriptSymbolRelation:
        return (ecs_pair_element_t){ symbol.id.relation, EcsPairRelation };
    default:
        script_eval_error(ctx, NULL, "unresolved pair element '%s'", name);
        return (ecs_pair_element_t){ 0 };
    }
}

script_key_t script_resolve_pair(script_eval_t *ctx, const char *first, const char *second) {
    ecs_pair_desc_t desc = { .first = pair_element(ctx, first),
                             .second = pair_element(ctx, second) };
    if (!desc.first.id || !desc.second.id)
        return 0;
    const ecs_pair_element_t elements[] = { desc.first, desc.second };
    for (unsigned i = 0; i < 2; i++) {
        if (elements[i].kind == EcsPairEntity && !script_entity_is_placeholder(elements[i].id) &&
            !ecs_is_alive(elements[i].id)) {
            script_eval_error(ctx, NULL, "pair element refers to a deleted entity");
            return 0;
        }
        if (elements[i].kind == EcsPairRelation &&
            (elements[i].id == ecs_rid(IsA) || elements[i].id == ecs_rid(ChildOf))) {
            script_eval_error(
                ctx,
                NULL,
                "builtin relations require an entity target without a payload"
            );
            return 0;
        }
    }
    desc.payload = desc.first.kind == EcsPairComponent    ? desc.first.id
                   : desc.second.kind == EcsPairComponent ? desc.second.id
                                                          : ecs_id(Abstract);
    script_key_t key = script_transaction_pair(script_transaction_active, &desc);
    if (!key)
        script_eval_error(ctx, NULL, "pair payload conflicts with its native registration");
    return key;
}
