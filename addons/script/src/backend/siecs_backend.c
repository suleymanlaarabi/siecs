#include "siecs_backend.h"
#include "../../../../src/storage/component_index.h"
#include "transaction.h"

char *script_backend_qualified_name(const char *name, ecs_entity_t scope) {
    char *path = scope ? script_backend_entity_path(scope) : NULL;
    const char *prefix = path && !strncmp(path, "::", 2) ? path + 2 : path;
    size_t length = prefix ? strlen(prefix) : 0;
    size_t count = strlen(name);
    if (count > (SIZE_MAX - length - 3) / 2) {
        free(path);
        return NULL;
    }
    char *qualified = malloc(length + count * 2 + 3);
    assert(qualified != NULL);
    char *out = qualified;
    if (length) {
        memcpy(out, prefix, length);
        out += length;
        *out++ = ':';
        *out++ = ':';
    }
    for (const char *at = name; *at; at++) {
        if (*at == '.') {
            *out++ = ':';
            *out++ = ':';
        } else
            *out++ = *at;
    }
    *out = 0;
    free(path);
    return qualified;
}

ecs_entity_t script_backend_entity_lookup(const char *path, ecs_entity_t parent) {
    if (!path)
        return 0;
    char *native = strchr(path, '.') ? script_backend_qualified_name(path, 0) : NULL;
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
    ecs_entity_t entity = script_transaction_entity(script_transaction_active, path, parent, true);
    if (entity)
        sicore_vec_push(&script_transaction_active->produced, &entity, sizeof entity);
    return entity;
}

ecs_component_t script_backend_component_lookup(const char *name) {
    return script_transaction_active
               ? script_transaction_type(script_transaction_active, name, NULL, false)
               : ecs_component_lookup(name);
}

ecs_component_t script_backend_component_register_dynamic(const char *name, const char *fields) {
    return script_transaction_type(script_transaction_active, name, fields, true);
}

int script_backend_relation_add(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
) {
    return script_transaction_relate(script_transaction_active, entity, relation, target);
}

void script_backend_component_add(ecs_entity_t entity, script_key_t component) {
    script_transaction_set(script_transaction_active, entity, component, NULL, false);
}

void script_backend_component_set(ecs_entity_t entity, script_key_t component, const void *value) {
    script_transaction_set(script_transaction_active, entity, component, value, false);
}

sireflect_handle_t script_backend_type_lookup(const char *name) {
    if (!strcmp(name, "string") || !strcmp(name, "char *"))
        name = "char*";
    if (!strcmp(name, "entity"))
        name = "ecs_entity_t";
    return sireflect_type_by_name(name);
}

ecs_component_t script_backend_tag_register(const char *name) {
    return script_backend_component_register_dynamic(name, NULL);
}

const ecs_component_info_t *script_backend_component_info(script_key_t component) {
    return script_transaction_active ? script_transaction_info(script_transaction_active, component)
                                     : script_key_info(component);
}

char *script_backend_entity_path(ecs_entity_t entity) {
    return script_transaction_active ? script_transaction_path(script_transaction_active, entity)
                                     : ecs_entity_path(entity);
}

void script_backend_isa_add(ecs_entity_t entity, ecs_entity_t target) {
    if (script_transaction_relate(script_transaction_active, entity, ecs_rid(IsA), target))
        script_transaction_active->failed = true;
}

void script_backend_prefab(ecs_entity_t entity) {
    script_transaction_prefab(script_transaction_active, entity);
}

void script_backend_value_init(script_key_t component, void *destination, const void *source) {
    const ecs_type_ops_t *ops = script_backend_value_ops(component);
    size_t size = script_backend_component_info(component)->size;
    if (source)
        ecs_value_transfer(ops, size, destination, source, 1, false, true);
    else if (ops->ctor)
        ops->ctor(destination, 1);
    else
        memset(destination, 0, size);
}

void script_backend_value_fini(script_key_t component, void *value) {
    const ecs_type_ops_t *ops = script_backend_value_ops(component);
    ecs_value_fini(ops, value, 1);
}

void script_backend_value_copy(script_key_t component, void *destination, const void *source) {
    const ecs_type_ops_t *ops = script_backend_value_ops(component);
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

bool script_backend_component_has(ecs_entity_t entity, script_key_t component) {
    if (script_transaction_active)
        script_transaction_read(script_transaction_active, entity, component);
    return script_transaction_active
               ? script_transaction_has(script_transaction_active, entity, component)
               : script_key_has(entity, component);
}

const ecs_type_ops_t *script_backend_value_ops(script_key_t component) {
    return script_transaction_active ? script_transaction_ops(script_transaction_active, component)
                                     : script_key_ops(component);
}

const ecs_component_info_t *script_key_info(script_key_t key) {
    return ecs_component_info(script_key_payload(NULL, key));
}
const ecs_type_ops_t *script_key_ops(script_key_t key) {
    return &ecs_component_index_get(script_key_payload(NULL, key))->ops;
}
bool script_key_valid(script_key_t key) {
    if (key & SCRIPT_PAIR_PENDING)
        return false;
    return key & SCRIPT_PAIR_KEY ? ecs_pair_info((ecs_pair_id_t)key) != NULL
                                 : key < ecs_component_count();
}
bool script_key_has(ecs_entity_t entity, script_key_t key) {
    if (key & SCRIPT_PAIR_PENDING)
        return false;
    return key & SCRIPT_PAIR_KEY ? ecs_pair_has(entity, (ecs_pair_id_t)key)
                                 : ecs_has_cid(entity, (ecs_component_t)key);
}
bool script_key_owned(ecs_entity_t entity, script_key_t key) {
    if (key & SCRIPT_PAIR_PENDING)
        return false;
    return key & SCRIPT_PAIR_KEY ? ecs_pair_has_owned(entity, (ecs_pair_id_t)key)
                                 : ecs_has_cid_owned(entity, (ecs_component_t)key);
}
const void *script_key_get(ecs_entity_t entity, script_key_t key) {
    if (key & SCRIPT_PAIR_PENDING)
        return NULL;
    return key & SCRIPT_PAIR_KEY ? ecs_pair_get(entity, (ecs_pair_id_t)key)
                                 : ecs_get_cid(entity, (ecs_component_t)key);
}
void script_key_write(ecs_entity_t entity, script_key_t key, const void *value, bool remove) {
    if (key & SCRIPT_PAIR_KEY) {
        bool ok = remove  ? ecs_pair_remove(entity, (ecs_pair_id_t)key)
                  : value ? ecs_pair_set(entity, (ecs_pair_id_t)key, value)
                          : ecs_pair_add(entity, (ecs_pair_id_t)key);
        ecs_assert(ok, "validated Script pair write failed");
    } else if (remove)
        ecs_remove_cid(entity, (ecs_component_t)key);
    else if (value)
        ecs_set_cid(entity, (ecs_component_t)key, value);
    else
        ecs_add_cid(entity, (ecs_component_t)key);
}
bool script_key_event(script_key_t key, const ecs_observer_event_t *event) {
    if (key & SCRIPT_PAIR_KEY)
        return event->trigger_data &&
               ((const ecs_pair_event_t *)event->trigger_data)->pair == (ecs_pair_id_t)key;
    if (event->component != key)
        return false;
    return key || (event->trigger_data &&
                   ((const ecs_relation_event_t *)event->trigger_data)->relation == ecs_rid(IsA));
}
ecs_observer_id_t script_key_observe(
    ecs_entity_t entity,
    script_key_t key,
    uint32_t slot,
    ecs_observer_callback_t callback,
    uintptr_t context,
    void (*destroy)(uintptr_t)
) {
    const ecs_event_t values[] = { EcsOnAdd, EcsOnSet, EcsOnRemove };
    const ecs_event_t pairs[] = { EcsOnPairSet, EcsOnPairRemove };
    const ecs_event_t links[] = { EcsOnRelationSet, EcsOnRelationRemove };
    if (slot == 2 && (!key || (key & SCRIPT_PAIR_KEY)))
        return UINT32_MAX;
    return ecs_observer_init(&(ecs_observer_desc_t){
        .on = key & SCRIPT_PAIR_KEY ? pairs[slot]
              : key                 ? values[slot]
                                    : links[slot],
        .entity = entity,
        .query.components = { { ecs_id(Abstract), EcsInOptional },
                              { ecs_id(Disabled), EcsInOptional },
                              { key & SCRIPT_PAIR_KEY ? 0 : (ecs_component_t)key, EcsInOptional } },
        .callback = callback,
        .user_data = context,
        .user_data_dtor = destroy });
}
