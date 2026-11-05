#include "transaction.h"
#include "../../../../src/storage/component_index.h"
#include "../eval/async.h"

#if defined(_MSC_VER)
__declspec(thread) script_transaction_t *script_transaction_active;
#else
_Thread_local script_transaction_t *script_transaction_active;
#endif

static script_staged_entity_t *
staged_entity(script_transaction_t *transaction, ecs_entity_t entity) {
    if (!(entity & SCRIPT_ENTITY_PLACEHOLDER))
        return NULL;
    uint64_t index = (entity >> 32) - 1;
    return index < transaction->entities.size
               ? &((script_staged_entity_t *)transaction->entities.data)[index]
               : NULL;
}

ecs_entity_t script_transaction_parent(script_transaction_t *transaction, ecs_entity_t entity) {
    script_staged_entity_t *staged = staged_entity(transaction, entity);
    return staged ? staged->parent : ecs_target_id(entity, ecs_rid(ChildOf));
}

void script_transaction_prefab(script_transaction_t *transaction, ecs_entity_t entity) {
    sicore_vec_push(&transaction->prefabs, &entity, sizeof entity);
}

bool script_transaction_has(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_component_t component
) {
    script_staged_value_t *values = transaction->values.data;
    for (uint32_t i = transaction->values.size; i; i--) {
        if (values[i - 1].entity == entity && values[i - 1].component == component)
            return !values[i - 1].remove;
    }
    if (entity & SCRIPT_ENTITY_PLACEHOLDER || component >= ecs_component_count())
        return false;
    return ecs_has_cid(entity, component);
}

void script_transaction_lookup(
    script_transaction_t *transaction,
    const char *name,
    ecs_entity_t parent
) {
    if (parent & SCRIPT_ENTITY_PLACEHOLDER)
        return;
    script_lookup_t *lookups = transaction->lookups.data;
    for (uint32_t i = 0; i < transaction->lookups.size; i++)
        if (lookups[i].parent == parent && lookups[i].region == transaction->region &&
            !strcmp(lookups[i].name, name))
            return;
    script_lookup_t lookup = { .name = script_strdup(name),
                               .parent = parent,
                               .resolved =
                                   script_transaction_entity(transaction, name, parent, false),
                               .region = transaction->region };
    sicore_vec_push(&transaction->lookups, &lookup, sizeof lookup);
}

static void *read_copy(const script_read_t *read, const void *source) {
    if (!source || !read->type)
        return NULL;
    size_t size = sireflect_type_size(read->type);
    void *copy = calloc(1, size);
    if (!copy)
        abort();
    ecs_reflected_value_copy(read->type, copy, source, true);
    return copy;
}

bool script_read_changed(const script_read_t *read) {
    if (!read->component)
        return !ecs_is_alive(read->entity) || ecs_entity_base(read->entity) != read->base;
    const void *live =
        ecs_is_alive(read->entity) ? ecs_get_cid(read->entity, read->component) : NULL;
    bool present = ecs_is_alive(read->entity) && ecs_has_cid(read->entity, read->component);
    if (present != read->present)
        return true;
    return present && read->type &&
           !ecs_reflected_value_equal(
               read->type,
               read->snapshot,
               (const char *)live + read->offset
           );
}

void script_transaction_read_member(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_component_t component,
    sireflect_handle_t type,
    size_t offset
) {
    if (script_entity_is_placeholder(entity) || component >= ecs_component_count())
        return;
    if (!component) {
        const script_read_t *reads = transaction->reads.data;
        for (uint32_t i = 0; i < transaction->reads.size; i++)
            if (reads[i].entity == entity && !reads[i].component &&
                reads[i].region == transaction->region)
                return;
        script_read_t link = { .entity = entity,
                               .base = ecs_entity_base(entity),
                               .region = transaction->region };
        sicore_vec_push(&transaction->reads, &link, sizeof link);
        return;
    }
    script_read_t read = { .entity = entity,
                           .component = component,
                           .type = type,
                           .offset = offset,
                           .present = ecs_has_cid(entity, component),
                           .region = transaction->region };
    const script_staged_value_t *values = transaction->values.data;
    for (uint32_t i = 0; i < transaction->values.size; i++)
        if (values[i].entity == entity && values[i].component == component) {
            if (transaction->region)
                ((script_region_t *)transaction->regions.data)[transaction->region - 1].reusable =
                    false;
            if (script_output_reads(&values[i], &read))
                return;
        }
    script_read_t *reads = transaction->reads.data;
    for (uint32_t i = 0; i < transaction->reads.size; i++)
        if (reads[i].entity == entity && reads[i].component == component && reads[i].type == type &&
            reads[i].offset == offset && reads[i].region == transaction->region)
            return;
    const void *live = ecs_get_cid(entity, component);
    read.snapshot = read_copy(&read, live ? (const char *)live + offset : NULL);
    sicore_vec_push(&transaction->reads, &read, sizeof read);
    if (!ecs_has_cid_owned(entity, component)) {
        script_transaction_read_member(transaction, entity, 0, 0, 0);
        ecs_entity_t base = ecs_entity_base(entity);
        if (base)
            script_transaction_read_member(transaction, base, component, type, offset);
    }
}

void script_transaction_read(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_component_t component
) {
    if (component >= ecs_component_count())
        return;
    script_transaction_read_member(
        transaction,
        entity,
        component,
        ecs_component_info(component)->type,
        0
    );
}

void script_transaction_init(script_transaction_t *transaction) {
    *transaction = (script_transaction_t){ 0 };
    sicore_vec_init(&transaction->entities, sizeof(script_staged_entity_t));
    sicore_vec_init(&transaction->types, sizeof(script_staged_type_t));
    sicore_vec_init(&transaction->values, sizeof(script_staged_value_t));
    sicore_vec_init(&transaction->relations, sizeof(script_staged_relation_t));
    sicore_vec_init(&transaction->prefabs, sizeof(ecs_entity_t));
    sicore_vec_init(&transaction->reads, sizeof(script_read_t));
    sicore_vec_init(&transaction->lookups, sizeof(script_lookup_t));
    sicore_vec_init(&transaction->produced, sizeof(ecs_entity_t));
    sicore_vec_init(&transaction->identities, sizeof(script_identity_t));
    sicore_vec_init(&transaction->regions, sizeof(script_region_t));
    sicore_vec_init(&transaction->kills, sizeof(ecs_entity_t));
    sicore_vec_init(&transaction->async_blocks, sizeof(void *));
    sicore_vec_init(&transaction->async_launches, sizeof(ecs_script_task_t));
    transaction->reflection = sireflect_transaction_begin();
    transaction->failed = transaction->reflection == NULL;
}

const ecs_component_info_t *
script_transaction_info(script_transaction_t *transaction, ecs_component_t component) {
    script_staged_type_t *types = transaction->types.data;
    for (uint32_t i = 0; i < transaction->types.size; i++)
        if (types[i].handle == component)
            return &types[i].info;
    return ecs_component_info(component);
}

const ecs_type_ops_t *
script_transaction_ops(script_transaction_t *transaction, ecs_component_t component) {
    script_staged_type_t *types = transaction->types.data;
    for (uint32_t i = 0; i < transaction->types.size; i++)
        if (types[i].handle == component)
            return &types[i].ops;
    return &ecs_component_index_get(component)->ops;
}

static bool
same_schema(script_transaction_t *transaction, sireflect_handle_t existing, const char *fields) {
    if (!fields)
        return true;
    if (!existing)
        return false;
    uint32_t mark = sireflect_transaction_mark(transaction->reflection);
    char name[64];
    uint32_t suffix = mark;
    do {
        snprintf(name, sizeof name, "__script_schema_%u", suffix++);
    } while (sireflect_type_by_name(name));
    sireflect_handle_t prepared = sireflect_try_register_dynamic_struct(name, fields);
    bool equal = false;
    if (prepared) {
        const sireflect_type_info_t *left = sireflect_type_info(existing);
        const sireflect_type_info_t *right = sireflect_type_info(prepared);
        equal = sireflect_type_is_struct(left) && left->size == right->size &&
                left->align == right->align &&
                left->fields.field_count == right->fields.field_count;
        for (size_t i = 0; equal && i < left->fields.field_count; i++) {
            const sireflect_field_info_t *a = &left->fields.fields[i];
            const sireflect_field_info_t *b = &right->fields.fields[i];
            equal = a->type == b->type && a->offset == b->offset && !strcmp(a->name, b->name);
        }
    }
    if (!sireflect_transaction_rewind(transaction->reflection, mark))
        abort();
    return equal;
}

ecs_component_t script_transaction_type(
    script_transaction_t *transaction,
    const char *name,
    const char *fields,
    bool create
) {
    script_staged_type_t *types = transaction->types.data;
    for (uint32_t i = 0; i < transaction->types.size; i++)
        if (!strcmp(types[i].info.name, name)) {
            if (!same_schema(transaction, types[i].info.type, fields)) {
                transaction->failed = true;
                return 0;
            }
            return types[i].handle;
        }
    ecs_component_t existing = ecs_component_lookup(name);
    if (existing && !same_schema(transaction, ecs_component_info(existing)->type, fields)) {
        transaction->failed = true;
        return 0;
    }
    if (existing || !create)
        return existing;
    if (transaction->types.size + ecs_component_count() >= UINT16_MAX) {
        transaction->failed = true;
        return 0;
    }
    sireflect_handle_t type = fields ? sireflect_try_register_dynamic_struct(name, fields) : 0;
    if (fields && !type) {
        transaction->failed = true;
        return 0;
    }
    script_staged_type_t added = {
        .handle = (ecs_component_t)(UINT16_MAX - transaction->types.size),
        .info = { .name = script_strdup(name),
                  .type = type,
                  .size = type ? sireflect_type_size(type) : 0 },
        .fields = fields ? script_strdup(fields) : NULL,
        .ops = type ? ecs_reflected_ops(type) : (ecs_type_ops_t){ 0 },
    };
    sicore_vec_push(&transaction->types, &added, sizeof added);
    return added.handle;
}

ecs_entity_t script_transaction_entity(
    script_transaction_t *transaction,
    const char *name,
    ecs_entity_t parent,
    bool create
) {
    if (name && (strchr(name, '.') || strstr(name, "::"))) {
        const char *separator = strchr(name, '.');
        const char *native = strstr(name, "::");
        if (!separator || (native && native < separator))
            separator = native;
        size_t length = (size_t)(separator - name);
        if (!length)
            return script_transaction_entity(transaction, separator + 2, 0, create);
        char *segment = malloc(length + 1);
        if (!segment) {
            transaction->failed = true;
            return 0;
        }
        memcpy(segment, name, length);
        segment[length] = 0;
        ecs_entity_t prefix = script_transaction_entity(transaction, segment, parent, create);
        free(segment);
        if (!prefix)
            return 0;
        return script_transaction_entity(
            transaction,
            separator + (separator == native ? 2 : 1),
            prefix,
            create
        );
    }
    script_staged_entity_t *entities = transaction->entities.data;
    if (name) {
        for (uint32_t i = 0; i < transaction->entities.size; i++) {
            if (entities[i].parent == parent && entities[i].name && !strcmp(entities[i].name, name))
                return SCRIPT_ENTITY_PLACEHOLDER | ((uint64_t)(i + 1) << 32);
        }
        if (!(parent & SCRIPT_ENTITY_PLACEHOLDER)) {
            ecs_entity_t live = ecs_lookup_from(parent, name);
            if (live)
                return live;
        }
    }
    if (!create)
        return 0;
    if (name && (!*name || *name == '#' || strchr(name, ':'))) {
        transaction->failed = true;
        return 0;
    }
    script_staged_entity_t added = { .parent = parent, .name = name ? script_strdup(name) : NULL };
    sicore_vec_push(&transaction->entities, &added, sizeof added);
    return SCRIPT_ENTITY_PLACEHOLDER | ((uint64_t)transaction->entities.size << 32);
}

char *script_transaction_path(script_transaction_t *transaction, ecs_entity_t entity) {
    script_staged_entity_t *staged = staged_entity(transaction, entity);
    if (!staged)
        return ecs_entity_path(entity);
    char *parent =
        staged->parent ? script_transaction_path(transaction, staged->parent) : script_strdup("");
    if (!parent)
        return NULL;
    const char *name = staged->name ? staged->name : "_";
    size_t a = strlen(parent), b = strlen(name);
    char *path = malloc(a + b + 3);
    if (path)
        snprintf(path, a + b + 3, "%s::%s", parent, name);
    free(parent);
    return path;
}

static void transaction_set(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *value,
    bool remove,
    const uint8_t *members
) {
    const ecs_component_info_t *info = script_transaction_info(transaction, component);
    const ecs_type_ops_t *ops = script_transaction_ops(transaction, component);
    script_staged_value_t *values = transaction->values.data;
    for (uint32_t i = 0; i < transaction->values.size; i++) {
        if (values[i].entity != entity || values[i].component != component)
            continue;
        if (values[i].region != transaction->region) {
            script_region_t *regions = transaction->regions.data;
            if (values[i].region)
                regions[values[i].region - 1].reusable = false;
            if (transaction->region)
                regions[transaction->region - 1].reusable = false;
            values[i].region = 0;
        }
        values[i].cached = false;
        if (value && values[i].data) {
            ecs_value_transfer(ops, info->size, values[i].data, value, 1, false, false);
        } else if (value && info->size) {
            values[i].data = malloc((size_t)info->size);
            if (!values[i].data) {
                transaction->failed = true;
                return;
            }
            ecs_value_transfer(ops, info->size, values[i].data, value, 1, false, true);
        }
        values[i].remove = remove;
        if (!members) {
            free(values[i].members);
            values[i].members = NULL;
        } else if (values[i].members) {
            size_t count = sireflect_type_fields(info->type)->field_count;
            for (size_t j = 0; j < count; j++)
                values[i].members[j] |= members[j];
        }
        return;
    }
    script_staged_value_t added = { .entity = entity,
                                    .component = component,
                                    .remove = remove,
                                    .region = transaction->region };
    if (value && info->size) {
        added.data = malloc((size_t)info->size);
        if (!added.data) {
            transaction->failed = true;
            return;
        }
        ecs_value_transfer(ops, info->size, added.data, value, 1, false, true);
    }
    if (members) {
        size_t count = sireflect_type_fields(info->type)->field_count;
        added.members = malloc(count);
        if (!added.members)
            abort();
        memcpy(added.members, members, count);
    }
    sicore_vec_push(&transaction->values, &added, sizeof added);
}

void script_transaction_set(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *value,
    bool remove
) {
    transaction_set(transaction, entity, component, value, remove, NULL);
}

void script_transaction_set_members(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *value,
    const uint8_t *members
) {
    transaction_set(transaction, entity, component, value, false, members);
}

const void *script_transaction_get(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_component_t component
) {
    script_staged_value_t *values = transaction->values.data;
    for (uint32_t i = transaction->values.size; i; i--) {
        if (values[i - 1].entity == entity && values[i - 1].component == component)
            return values[i - 1].remove ? NULL : values[i - 1].data;
    }
    script_staged_relation_t *relations = transaction->relations.data;
    for (uint32_t i = transaction->relations.size; i; i--) {
        if (relations[i - 1].entity == entity && relations[i - 1].relation == ecs_rid(IsA))
            return script_transaction_get(transaction, relations[i - 1].target, component);
    }
    if (entity & SCRIPT_ENTITY_PLACEHOLDER)
        return NULL;
    if (component >= ecs_component_count())
        return NULL;
    return ecs_get_cid(entity, component);
}

int script_transaction_relate(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
) {
    const ecs_relation_info_t *info = ecs_relation_info(relation);
    if (!info || !entity || !target)
        return -1;
    if (relation == ecs_rid(IsA) || relation == ecs_rid(ChildOf) || info->desc.acyclic) {
        ecs_entity_t cursor = target;
        for (uint32_t depth = 0; cursor; depth++) {
            if (cursor == entity || depth == 1000000) {
                transaction->failed = true;
                return -1;
            }
            ecs_entity_t next = 0;
            script_staged_relation_t *pending = transaction->relations.data;
            bool found = false;
            for (uint32_t i = transaction->relations.size; i; i--) {
                if (pending[i - 1].entity == cursor && pending[i - 1].relation == relation) {
                    next = pending[i - 1].target;
                    found = true;
                    break;
                }
            }
            if (!found) {
                script_staged_entity_t *staged = staged_entity(transaction, cursor);
                if (staged && relation == ecs_rid(ChildOf))
                    next = staged->parent;
                else if (!staged)
                    next = ecs_target_id(cursor, relation);
            }
            cursor = next;
        }
    }
    script_staged_relation_t *relations = transaction->relations.data;
    for (uint32_t i = 0; i < transaction->relations.size; i++) {
        if (relations[i].entity != entity || relations[i].relation != relation)
            continue;
        if (relations[i].target != target)
            return -1;
        return 0;
    }
    script_staged_relation_t added = { entity, relation, target };
    sicore_vec_push(&transaction->relations, &added, sizeof added);
    return 0;
}

ecs_entity_t
script_transaction_committed_entity(script_transaction_t *transaction, ecs_entity_t entity) {
    script_staged_entity_t *staged = staged_entity(transaction, entity);
    return staged ? staged->handle : entity;
}

ecs_component_t script_transaction_committed_component(
    script_transaction_t *transaction,
    ecs_component_t component
) {
    script_staged_type_t *types = transaction->types.data;
    for (uint32_t i = 0; i < transaction->types.size; i++)
        if (types[i].handle == component)
            return types[i].committed;
    return component;
}

void script_transaction_remap_value(
    script_transaction_t *transaction,
    sireflect_handle_t type,
    void *data
) {
    if (type == sireflect_type_by_name("ecs_entity_t")) {
        ecs_entity_t entity;
        memcpy(&entity, data, sizeof entity);
        entity = script_transaction_committed_entity(transaction, entity);
        memcpy(data, &entity, sizeof entity);
        return;
    }
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info->kind == sireflect_kind_array) {
        size_t size = sireflect_type_size(info->element_type);
        for (size_t i = 0; i < info->element_count; i++)
            script_transaction_remap_value(
                transaction,
                info->element_type,
                (char *)data + i * size
            );
    } else if (sireflect_type_is_struct(info)) {
        for (size_t i = 0; i < info->fields.field_count; i++) {
            const sireflect_field_info_t *field = &info->fields.fields[i];
            script_transaction_remap_value(transaction, field->type, (char *)data + field->offset);
        }
    }
}

int script_transaction_commit(script_transaction_t *transaction) {
    if (transaction->failed || !transaction->reflection)
        return -1;
    if (!sireflect_transaction_commit(transaction->reflection))
        return -1;
    transaction->reflection = NULL;
    ecs_sync_begin();
    ecs_observer_batch_begin();
    script_staged_type_t *types = transaction->types.data;
    for (uint32_t i = 0; i < transaction->types.size; i++) {
        types[i].committed = types[i].fields
                                 ? ecs_component_dynamic_init(&(
                                       ecs_dynamic_component_desc_t
                                   ){ .name = types[i].info.name, .fields = types[i].fields })
                                 : ecs_tag_init(types[i].info.name);
        ecs_assert(types[i].committed != 0, "validated component registration failed");
    }
    script_staged_entity_t *entities = transaction->entities.data;
    for (uint32_t i = 0; i < transaction->entities.size; i++) {
        ecs_entity_t parent = script_transaction_committed_entity(transaction, entities[i].parent);
        entities[i].handle = entities[i].name ? ecs_new_named(parent, entities[i].name) : ecs_new();
        if (!entities[i].name && parent)
            ecs_relate_id(entities[i].handle, ecs_rid(ChildOf), parent);
        ecs_assert(entities[i].handle != 0, "validated entity creation failed");
    }
    script_staged_relation_t *relations = transaction->relations.data;
    for (uint32_t i = 0; i < transaction->relations.size; i++) {
        relations[i].entity = script_transaction_committed_entity(transaction, relations[i].entity);
        relations[i].target = script_transaction_committed_entity(transaction, relations[i].target);
        if (relations[i].target)
            ecs_relate_id(relations[i].entity, relations[i].relation, relations[i].target);
        else if (ecs_target_id(relations[i].entity, relations[i].relation))
            ecs_unrelate_id(relations[i].entity, relations[i].relation);
    }
    script_staged_value_t *values = transaction->values.data;
    for (uint32_t i = 0; i < transaction->values.size; i++) {
        values[i].entity = script_transaction_committed_entity(transaction, values[i].entity);
        ecs_component_t component =
            script_transaction_committed_component(transaction, values[i].component);
        if (values[i].unchanged)
            continue;
        if (values[i].remove) {
            ecs_remove_cid(values[i].entity, component);
        } else if (values[i].data) {
            const ecs_component_info_t *info = ecs_component_info(component);
            if (info->type)
                script_transaction_remap_value(transaction, info->type, values[i].data);
            ecs_set_cid(values[i].entity, component, values[i].data);
        } else {
            ecs_add_cid(values[i].entity, component);
        }
    }
    const ecs_entity_t *prefabs = transaction->prefabs.data;
    for (uint32_t i = 0; i < transaction->prefabs.size; i++)
        ecs_abstract(script_transaction_committed_entity(transaction, prefabs[i]));
    for (uint32_t i = 0; i < transaction->values.size; i++)
        values[i].component =
            script_transaction_committed_component(transaction, values[i].component);
    ecs_entity_t *produced = transaction->produced.data;
    for (uint32_t i = 0; i < transaction->produced.size; i++)
        produced[i] = script_transaction_committed_entity(transaction, produced[i]);
    script_identity_t *identities = transaction->identities.data;
    for (uint32_t i = 0; i < transaction->identities.size; i++) {
        identities[i].entity =
            script_transaction_committed_entity(transaction, identities[i].entity);
        identities[i].parent =
            script_transaction_committed_entity(transaction, identities[i].parent);
    }
    script_region_t *regions = transaction->regions.data;
    for (uint32_t i = 0; i < transaction->regions.size; i++) {
        regions[i].parent = script_transaction_committed_entity(transaction, regions[i].parent);
        script_context_remap(transaction, regions[i].captures);
    }
    script_lookup_t *lookups = transaction->lookups.data;
    for (uint32_t i = 0; i < transaction->lookups.size; i++)
        lookups[i].resolved = script_transaction_committed_entity(transaction, lookups[i].resolved);
    const ecs_entity_t *kills = transaction->kills.data;
    for (uint32_t i = 0; i < transaction->kills.size; i++)
        if (ecs_is_alive(kills[i]))
            ecs_kill(kills[i]);
    if (transaction->publish)
        transaction->publish(transaction, transaction->context);
    ecs_observer_batch_end();
    script_async_transaction_commit(transaction);
    ecs_sync_end();
    return 0;
}

void script_transaction_fini(script_transaction_t *transaction) {
    script_transaction_t *previous = script_transaction_active;
    if (transaction->reflection)
        script_transaction_active = transaction;
    script_async_transaction_fini(transaction);
    script_region_t *regions = transaction->regions.data;
    for (uint32_t i = 0; i < transaction->regions.size; i++) {
        script_context_fini(regions[i].captures);
        free(regions[i].captures);
        free(regions[i].name);
    }
    script_read_t *reads = transaction->reads.data;
    for (uint32_t i = 0; i < transaction->reads.size; i++) {
        if (reads[i].snapshot) {
            ecs_reflected_value_fini(reads[i].type, reads[i].snapshot);
            free(reads[i].snapshot);
        }
    }
    script_staged_value_t *values = transaction->values.data;
    for (uint32_t i = 0; i < transaction->values.size; i++) {
        free(values[i].members);
        if (values[i].data) {
            const ecs_type_ops_t *ops = script_transaction_ops(transaction, values[i].component);
            ecs_value_fini(ops, values[i].data, 1);
            free(values[i].data);
        }
    }
    script_staged_entity_t *entities = transaction->entities.data;
    for (uint32_t i = 0; i < transaction->entities.size; i++)
        free(entities[i].name);
    script_staged_type_t *types = transaction->types.data;
    for (uint32_t i = 0; i < transaction->types.size; i++) {
        free((char *)types[i].info.name);
        free(types[i].fields);
    }
    if (transaction->reflection)
        sireflect_transaction_abort(transaction->reflection);
    sicore_vec_fini(&transaction->entities);
    sicore_vec_fini(&transaction->types);
    sicore_vec_fini(&transaction->values);
    sicore_vec_fini(&transaction->relations);
    sicore_vec_fini(&transaction->prefabs);
    script_lookup_t *lookups = transaction->lookups.data;
    for (uint32_t i = 0; i < transaction->lookups.size; i++)
        free(lookups[i].name);
    sicore_vec_fini(&transaction->reads);
    sicore_vec_fini(&transaction->lookups);
    sicore_vec_fini(&transaction->produced);
    script_identity_t *identities = transaction->identities.data;
    for (uint32_t i = 0; i < transaction->identities.size; i++)
        free(identities[i].source);
    sicore_vec_fini(&transaction->identities);
    sicore_vec_fini(&transaction->regions);
    sicore_vec_fini(&transaction->kills);
    sicore_vec_fini(&transaction->async_blocks);
    sicore_vec_fini(&transaction->async_launches);
    script_transaction_active = previous == transaction ? NULL : previous;
}

static void
snapshot_vector(sicore_vec_t *destination, const sicore_vec_t *source, uint32_t stride) {
    sicore_vec_init(destination, stride);
    for (uint32_t i = 0; i < source->size; i++)
        sicore_vec_push(destination, (char *)source->data + i * stride, stride);
}

void script_transaction_savepoint(
    script_transaction_t *transaction,
    script_savepoint_t *savepoint
) {
    *savepoint = (script_savepoint_t){ .reflection_mark =
                                           sireflect_transaction_mark(transaction->reflection),
                                       .block_count = transaction->async_blocks.size,
                                       .launch_count = transaction->async_launches.size };
    script_transaction_t *copy = &savepoint->saved;
    copy->failed = transaction->failed;
    snapshot_vector(&copy->entities, &transaction->entities, sizeof(script_staged_entity_t));
    script_staged_entity_t *entities = copy->entities.data;
    for (uint32_t i = 0; i < copy->entities.size; i++)
        entities[i].name = entities[i].name ? script_strdup(entities[i].name) : NULL;
    snapshot_vector(&copy->types, &transaction->types, sizeof(script_staged_type_t));
    script_staged_type_t *types = copy->types.data;
    for (uint32_t i = 0; i < copy->types.size; i++) {
        types[i].info.name = script_strdup(types[i].info.name);
        types[i].fields = types[i].fields ? script_strdup(types[i].fields) : NULL;
    }
    snapshot_vector(&copy->values, &transaction->values, sizeof(script_staged_value_t));
    script_staged_value_t *values = copy->values.data;
    for (uint32_t i = 0; i < copy->values.size; i++) {
        if (values[i].members) {
            size_t count = sireflect_type_fields(
                               script_transaction_info(transaction, values[i].component)->type
            )
                               ->field_count;
            uint8_t *mask = malloc(count);
            if (!mask)
                abort();
            memcpy(mask, values[i].members, count);
            values[i].members = mask;
        }
        if (!values[i].data)
            continue;
        const ecs_component_info_t *info =
            script_transaction_info(transaction, values[i].component);
        void *snapshot = malloc((size_t)info->size);
        if (!snapshot)
            abort();
        ecs_value_copy(
            script_transaction_ops(transaction, values[i].component),
            info->size,
            snapshot,
            values[i].data,
            1,
            true
        );
        values[i].data = snapshot;
    }
    snapshot_vector(&copy->relations, &transaction->relations, sizeof(script_staged_relation_t));
    snapshot_vector(&copy->prefabs, &transaction->prefabs, sizeof(ecs_entity_t));
    snapshot_vector(&copy->reads, &transaction->reads, sizeof(script_read_t));
    script_read_t *reads = copy->reads.data;
    for (uint32_t i = 0; i < copy->reads.size; i++)
        reads[i].snapshot = read_copy(&reads[i], reads[i].snapshot);
    snapshot_vector(&copy->lookups, &transaction->lookups, sizeof(script_lookup_t));
    script_lookup_t *lookups = copy->lookups.data;
    for (uint32_t i = 0; i < copy->lookups.size; i++)
        lookups[i].name = script_strdup(lookups[i].name);
    snapshot_vector(&copy->produced, &transaction->produced, sizeof(ecs_entity_t));
    snapshot_vector(&copy->identities, &transaction->identities, sizeof(script_identity_t));
    snapshot_vector(&copy->regions, &transaction->regions, sizeof(script_region_t));
    script_region_t *regions = copy->regions.data;
    for (uint32_t i = 0; i < copy->regions.size; i++) {
        script_eval_t *snapshot = malloc(sizeof(*snapshot));
        assert(snapshot != NULL);
        script_context_copy(snapshot, regions[i].captures);
        regions[i].captures = snapshot;
        regions[i].name = regions[i].name ? script_strdup(regions[i].name) : NULL;
    }
    copy->region = transaction->region;
    script_identity_t *identities = copy->identities.data;
    for (uint32_t i = 0; i < copy->identities.size; i++)
        identities[i].source = script_strdup(identities[i].source);
    snapshot_vector(&copy->kills, &transaction->kills, sizeof(ecs_entity_t));
}

void script_transaction_discard_savepoint(script_savepoint_t *savepoint) {
    script_transaction_fini(&savepoint->saved);
    *savepoint = (script_savepoint_t){ 0 };
}

void script_transaction_rewind(script_transaction_t *transaction, script_savepoint_t *savepoint) {
    script_transaction_t discarded = *transaction;
    discarded.reflection = NULL;
    /* A caught failure still depends on the reads that selected its fallback. */
    discarded.reads = savepoint->saved.reads;
    savepoint->saved.reads = transaction->reads;
    discarded.lookups = savepoint->saved.lookups;
    savepoint->saved.lookups = transaction->lookups;
    /* Only jobs created after the checkpoint belong to the abandoned branch. */
    if (transaction->async_blocks.data)
        discarded.async_blocks.data =
            (char *)transaction->async_blocks.data + savepoint->block_count * sizeof(void *);
    discarded.async_blocks.size -= savepoint->block_count;
    if (transaction->async_launches.data)
        discarded.async_launches.data = (char *)transaction->async_launches.data +
                                        savepoint->launch_count * sizeof(ecs_script_task_t);
    discarded.async_launches.size -= savepoint->launch_count;
    script_async_transaction_fini(&discarded);
    discarded.async_blocks = (sicore_vec_t){ 0 };
    discarded.async_launches = (sicore_vec_t){ 0 };
    script_transaction_fini(&discarded);
    sireflect_transaction_t *reflection = transaction->reflection;
    sicore_vec_t blocks = transaction->async_blocks, launches = transaction->async_launches;
    blocks.size = savepoint->block_count;
    launches.size = savepoint->launch_count;
    void (*publish)(script_transaction_t *, void *) = transaction->publish;
    void *context = transaction->context;
    ecs_script_instance_t *instance = transaction->instance;
    bool reconcile = transaction->reconcile_async;
    *transaction = savepoint->saved;
    transaction->reflection = reflection;
    transaction->async_blocks = blocks;
    transaction->async_launches = launches;
    transaction->publish = publish;
    transaction->context = context;
    transaction->instance = instance;
    transaction->reconcile_async = reconcile;
    sireflect_transaction_rewind(reflection, savepoint->reflection_mark);
    *savepoint = (script_savepoint_t){ 0 };
}
