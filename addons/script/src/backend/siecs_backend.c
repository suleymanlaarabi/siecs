#include "siecs_backend.h"
#include "../../../../src/storage/component_index.h"
#include "transaction.h"

/* Script paths use dots; native naming stores the same hierarchy with ::. */
static char *script_backend_path(const char *path) {
    size_t len = strlen(path);
    if (len > (SIZE_MAX - 1) / 2)
        return NULL;
    char *result = malloc(len * 2 + 1);
    if (!result)
        return NULL;
    char *out = result;
    for (const char *ptr = path; *ptr; ptr++) {
        if (*ptr == '.') {
            *out++ = ':';
            *out++ = ':';
        } else
            *out++ = *ptr;
    }
    *out = '\0';
    return result;
}

ecs_entity_t script_backend_entity_lookup(const char *path, ecs_entity_t parent) {
    if (!path)
        return 0;
    if (script_transaction_active) {
        do {
            script_transaction_lookup(script_transaction_active, path, parent);
            ecs_entity_t entity =
                script_transaction_entity(script_transaction_active, path, parent, false);
            if (entity || !parent)
                return entity;
            parent = script_transaction_parent(script_transaction_active, parent);
        } while (true);
    }
    char *native = strchr(path, '.') ? script_backend_path(path) : NULL;
    const char *lookup = native ? native : path;
    ecs_entity_t result = 0;
    do {
        result = ecs_lookup_from(parent, lookup);
        if (result || !parent)
            break;
        parent = ecs_target_id(parent, ecs_rid(ChildOf));
    } while (true);
    free(native);
    return result;
}

ecs_entity_t script_backend_entity_ensure(const char *path, ecs_entity_t parent) {
    if (script_transaction_active) {
        ecs_entity_t entity =
            script_transaction_entity(script_transaction_active, path, parent, true);
        if (entity)
            sicore_vec_push(&script_transaction_active->produced, &entity, sizeof entity);
        return entity;
    }
    if (!path) {
        ecs_entity_t entity = ecs_new();
        if (parent)
            ecs_relate_id(entity, ecs_rid(ChildOf), parent);
        return entity;
    }
    char *native = strchr(path, '.') ? script_backend_path(path) : NULL;
    ecs_entity_t result = ecs_new_named(parent, native ? native : path);
    free(native);
    return result;
}

ecs_component_t script_backend_component_lookup(const char *name) {
    return script_transaction_active
               ? script_transaction_type(script_transaction_active, name, NULL, false)
               : ecs_component_lookup(name);
}

ecs_component_t script_backend_component_register_dynamic(const char *name, const char *fields) {
    if (script_transaction_active)
        return script_transaction_type(script_transaction_active, name, fields, true);
    return ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = name,
        .fields = fields,
    });
}

script_symbol_t script_backend_symbol_lookup(const char *name, ecs_entity_t parent) {
    ecs_component_t component = script_backend_component_lookup(name);
    if (component)
        return (script_symbol_t){ ScriptSymbolComponent, { .component = component } };
    ecs_relation_id_t relation = ecs_relation_lookup(name);
    if (relation)
        return (script_symbol_t){ ScriptSymbolRelation, { .relation = relation } };
    ecs_entity_t entity = script_backend_entity_lookup(name, parent);
    if (entity)
        return (script_symbol_t){ ScriptSymbolEntity, { .entity = entity } };
    sireflect_handle_t type = script_backend_type_lookup(name);
    if (type)
        return (script_symbol_t){ ScriptSymbolReflectType, { .type = type } };
    return (script_symbol_t){ 0 };
}

int script_backend_relation_add(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
) {
    if (script_transaction_active)
        return script_transaction_relate(script_transaction_active, entity, relation, target);
    /* Reject unsupported multiple targets instead of silently dropping a pair. */
    ecs_entity_t old = ecs_target_id(entity, relation);
    if (old && old != target)
        return -1;
    ecs_relate_id(entity, relation, target);
    return 0;
}

void script_backend_component_add(ecs_entity_t entity, ecs_component_t component) {
    if (script_transaction_active)
        script_transaction_set(script_transaction_active, entity, component, NULL, false);
    else
        ecs_add_cid(entity, component);
}

void script_backend_component_set(
    ecs_entity_t entity,
    ecs_component_t component,
    const void *value
) {
    if (script_transaction_active)
        script_transaction_set(script_transaction_active, entity, component, value, false);
    else
        ecs_set_cid(entity, component, value);
}

const void *script_backend_component_get(ecs_entity_t entity, ecs_component_t component) {
    if (script_transaction_active)
        script_transaction_read(script_transaction_active, entity, component);
    return script_transaction_active
               ? script_transaction_get(script_transaction_active, entity, component)
               : ecs_get_cid(entity, component);
}

sireflect_handle_t script_backend_type_lookup(const char *name) {
    if (!strcmp(name, "string") || !strcmp(name, "char *"))
        name = "char*";
    if (!strcmp(name, "entity"))
        name = "ecs_entity_t";
    return sireflect_type_by_name(name);
}

ecs_component_t script_backend_tag_register(const char *name) {
    return script_transaction_active
               ? script_transaction_type(script_transaction_active, name, NULL, true)
               : ecs_tag_init(name);
}

const ecs_component_info_t *script_backend_component_info(ecs_component_t component) {
    return script_transaction_active ? script_transaction_info(script_transaction_active, component)
                                     : ecs_component_info(component);
}

char *script_backend_entity_path(ecs_entity_t entity) {
    return script_transaction_active ? script_transaction_path(script_transaction_active, entity)
                                     : ecs_entity_path(entity);
}

void script_backend_isa_add(ecs_entity_t entity, ecs_entity_t target) {
    if (script_transaction_active) {
        if (script_transaction_relate(script_transaction_active, entity, ecs_rid(IsA), target))
            script_transaction_active->failed = true;
    } else
        ecs_is_a(entity, target);
}

void script_backend_prefab(ecs_entity_t entity) {
    if (script_transaction_active)
        script_transaction_prefab(script_transaction_active, entity);
    else
        ecs_abstract(entity);
}

void script_backend_value_init(ecs_component_t component, void *destination, const void *source) {
    const ecs_type_ops_t *ops = script_transaction_active
                                    ? script_transaction_ops(script_transaction_active, component)
                                    : &ecs_component_index_get(component)->ops;
    size_t size = script_backend_component_info(component)->size;
    if (source)
        ecs_value_transfer(ops, size, destination, source, 1, false, true);
    else if (ops->ctor)
        ops->ctor(destination, 1);
    else
        memset(destination, 0, size);
}

void script_backend_value_fini(ecs_component_t component, void *value) {
    const ecs_type_ops_t *ops = script_transaction_active
                                    ? script_transaction_ops(script_transaction_active, component)
                                    : &ecs_component_index_get(component)->ops;
    ecs_value_fini(ops, value, 1);
}

void script_backend_value_copy(ecs_component_t component, void *destination, const void *source) {
    const ecs_type_ops_t *ops = script_transaction_active
                                    ? script_transaction_ops(script_transaction_active, component)
                                    : &ecs_component_index_get(component)->ops;
    ecs_value_transfer(
        ops,
        script_backend_component_info(component)->size,
        destination,
        source,
        1,
        false,
        false
    );
}

bool script_backend_component_has(ecs_entity_t entity, ecs_component_t component) {
    if (script_transaction_active)
        script_transaction_read(script_transaction_active, entity, component);
    return script_transaction_active
               ? script_transaction_has(script_transaction_active, entity, component)
               : ecs_has_cid(entity, component);
}

const ecs_type_ops_t *script_backend_value_ops(ecs_component_t component) {
    return script_transaction_active ? script_transaction_ops(script_transaction_active, component)
                                     : &ecs_component_index_get(component)->ops;
}
