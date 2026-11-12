#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../src/storage/component_index.h"
#include "backend/transaction.h"
#include "eval/async.h"
#include <sys/stat.h>

typedef struct script_watch_t script_watch_t;
typedef struct script_file_stamp_t {
    const ecs_script_t *program;
    uint64_t modified;
    uint64_t size;
    uint64_t content;
    bool exists;
    bool readable;
} script_file_stamp_t;
typedef struct script_baseline_t {
    ecs_entity_t entity;
    script_key_t component;
    void *value;
    bool present;
    sireflect_handle_t schema;
    ecs_type_ops_t ops;
} script_baseline_t;
typedef struct script_async_baseline_t {
    script_baseline_t initial;
    script_staged_value_t last;
    bool committed;
} script_async_baseline_t;
typedef struct script_created_t {
    ecs_entity_t entity;
    ecs_entity_t parent;
    char *name;
} script_created_t;
typedef struct script_relation_baseline_t {
    ecs_entity_t entity;
    ecs_relation_id_t relation;
    ecs_entity_t target;
} script_relation_baseline_t;
typedef struct script_watch_observer_t {
    script_watch_t *watch;
    uint32_t slot;
} script_watch_observer_t;
struct script_watch_t {
    ecs_entity_t entity;
    script_key_t component;
    ecs_observer_id_t observers[3];
    script_watch_observer_t contexts[3];
    sicore_vec_t listeners;
};
struct ecs_script_instance_t {
    struct ecs_script_instance_t *next;
    const ecs_script_t *program;
    ecs_script_instance_desc_t desc;
    ecs_script_instance_state_t state;
    ecs_script_eval_result_t diagnostic;
    ecs_script_stats_t stats;
    script_transaction_t active;
    sicore_vec_t baselines;
    sicore_vec_t async_baselines;
    sicore_vec_t relation_baselines;
    sicore_vec_t subscriptions;
    sicore_vec_t created;
    sicore_vec_t schemas;
    sicore_vec_t files;
    double next_file_check;
    bool watch_failed;
    bool has_active;
    bool has_async;
    bool queued;
    bool force_evaluation;
    bool evaluating;
    bool busy_async;
    bool destroy_pending;
    bool destroying;
    uint32_t async_failures;
    uint32_t async_pending;
    bool detached;
    bool pending_names;
};

typedef struct script_owner_t {
    ecs_entity_t entity;
    script_key_t component;
    ecs_script_instance_t *instance;
    uint8_t *members;
    sireflect_handle_t schema;
    bool async;
} script_owner_t;
typedef struct script_manager_t {
    ecs_script_instance_t *instances;
    sicore_vec_t watches;
    sicore_vec_t owners;
    sicore_vec_t queue;
    sicore_vec_t watched;
    ecs_sync_id_t sync;
    ecs_observer_id_t names[2];
    bool initialized;
    bool flushing;
    bool finishing;
    bool freeing;
} script_manager_t;
static script_manager_t manager;

bool script_instance_owns_schema(ecs_script_instance_t *instance, script_key_t component) {
    if (!instance)
        return false;
    const ecs_component_t *schemas = instance->schemas.data;
    for (uint32_t i = 0; i < instance->schemas.size; i++)
        if (schemas[i] == component)
            return true;
    return false;
}

static bool identity_claimed(const script_transaction_t *transaction, ecs_entity_t entity) {
    const script_identity_t *identities = transaction->identities.data;
    for (uint32_t i = 0; i < transaction->identities.size; i++)
        if (identities[i].entity == entity)
            return true;
    return false;
}

static bool identity_reserved(const ecs_script_t *program, uint64_t fingerprint) {
    for (const ecs_script_entity_t *node = siecs_script_impl(program)->anonymous; node;
         node = node->next_anonymous)
        if (node->identity_hash == fingerprint)
            return true;
    return false;
}

ecs_entity_t script_instance_anonymous(
    ecs_script_instance_t *instance,
    script_transaction_t *transaction,
    const ecs_script_t *program,
    const ecs_script_entity_t *node,
    ecs_entity_t parent
) {
    uint32_t occurrence = 0;
    const script_identity_t *identities = transaction->identities.data;
    for (uint32_t i = 0; i < transaction->identities.size; i++)
        if (identities[i].node == node->anonymous_id && identities[i].parent == parent &&
            !strcmp(identities[i].source, program->name))
            occurrence++;
    ecs_entity_t entity = 0, fallback = 0;
    identities = instance->active.identities.data;
    for (uint32_t i = 0; instance->has_active && i < instance->active.identities.size; i++) {
        const script_identity_t *previous = &identities[i];
        if (previous->parent != parent || previous->occurrence != occurrence ||
            strcmp(previous->source, program->name) || !ecs_is_alive(previous->entity) ||
            ecs_target_id(previous->entity, ecs_rid(ChildOf)) != parent ||
            identity_claimed(transaction, previous->entity))
            continue;
        if (previous->fingerprint == node->identity_hash) {
            entity = previous->entity;
            if (previous->node == node->anonymous_id)
                break;
        } else if (
            previous->node == node->anonymous_id &&
            !identity_reserved(program, previous->fingerprint)
        ) {
            fallback = previous->entity;
        }
    }
    if (!entity)
        entity = fallback ? fallback : script_transaction_entity(transaction, NULL, parent, true);
    script_identity_t identity = { .source = script_strdup(program->name),
                                   .node = node->anonymous_id,
                                   .fingerprint = node->identity_hash,
                                   .occurrence = occurrence,
                                   .parent = parent,
                                   .entity = entity };
    sicore_vec_push(&transaction->identities, &identity, sizeof identity);
    sicore_vec_push(&transaction->produced, &entity, sizeof entity);
    return entity;
}

static size_t owner_position(ecs_entity_t entity, script_key_t component) {
    const script_owner_t *owners = manager.owners.data;
    size_t low = 0, high = manager.owners.size;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (owners[middle].entity < entity ||
            (owners[middle].entity == entity && owners[middle].component < component))
            low = middle + 1;
        else
            high = middle;
    }
    return low;
}

static void release_output(
    ecs_script_instance_t *instance,
    ecs_entity_t entity,
    script_key_t component,
    bool async
) {
    size_t at = owner_position(entity, component);
    script_owner_t *owners = manager.owners.data;
    for (; at < manager.owners.size && owners[at].entity == entity &&
           owners[at].component == component;
         at++) {
        if (owners[at].instance != instance || owners[at].async != async)
            continue;
        free(owners[at].members);
        memmove(owners + at, owners + at + 1, (--manager.owners.size - at) * sizeof(*owners));
        return;
    }
}

static void
claim_output(ecs_script_instance_t *instance, const script_staged_value_t *output, bool async) {
    release_output(instance, output->entity, output->component, async);
    size_t at = owner_position(output->entity, output->component);
    sicore_vec_push_empty(&manager.owners, sizeof(script_owner_t));
    script_owner_t *owners = manager.owners.data;
    memmove(owners + at + 1, owners + at, (manager.owners.size - at - 1) * sizeof(*owners));
    owners[at] =
        (script_owner_t){ .entity = output->entity,
                          .component = output->component,
                          .instance = instance,
                          .schema = output->schema,
                          .members = script_output_copy_mask(output->schema, output->members),
                          .async = async };
}

static bool
conflicting_outputs(ecs_script_instance_t *instance, const script_transaction_t *transaction) {
    const script_staged_value_t *outputs = transaction->values.data;
    const script_owner_t *owners = manager.owners.data;
    for (uint32_t i = 0; i < transaction->values.size; i++) {
        if (script_entity_is_placeholder(outputs[i].entity))
            continue;
        size_t at = owner_position(outputs[i].entity, outputs[i].component);
        for (; at < manager.owners.size && owners[at].entity == outputs[i].entity &&
               owners[at].component == outputs[i].component;
             at++)
            if (owners[at].instance != instance && owners[at].schema == outputs[i].schema &&
                script_output_overlap(outputs[i].component, owners[at].members, outputs[i].members))
                return true;
    }
    return false;
}

static bool equal_component(script_key_t component, const void *left, const void *right) {
    const ecs_component_info_t *info = script_key_info(component);
    return info->type ? ecs_reflected_value_equal(info->type, left, right) : !info->size;
}

static void enqueue(ecs_script_instance_t *instance) {
    if (instance->queued || instance->detached)
        return;
    instance->queued = true;
    instance->stats.invalidations++;
    sicore_vec_push(&manager.queue, &instance, sizeof instance);
}

static void watch_changed(ecs_observer_event_t *event) {
    script_watch_observer_t *context = (void *)event->user_data;
    script_watch_t *watch = context->watch;
    if (!script_key_event(watch->component, event))
        return;
    ecs_script_instance_t **listeners = watch->listeners.data;
    for (uint32_t i = 0; i < watch->listeners.size; i++) {
        ecs_script_instance_t *instance = listeners[i];
        const script_read_t *reads = instance->active.reads.data;
        bool tracked = false;
        for (uint32_t j = 0; instance->has_active && j < instance->active.reads.size; j++) {
            if (reads[j].entity != watch->entity || reads[j].component != watch->component)
                continue;
            tracked = true;
        }
        instance->force_evaluation |= !tracked;
        enqueue(instance);
    }
}

static void watch_removed(uintptr_t data) {
    script_watch_observer_t *context = (void *)data;
    context->watch->observers[context->slot] = UINT32_MAX;
}

static size_t watch_position(ecs_entity_t entity, script_key_t component) {
    script_watch_t **watches = manager.watches.data;
    size_t low = 0, high = manager.watches.size;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        script_watch_t *watch = watches[middle];
        if (watch->entity < entity || (watch->entity == entity && watch->component < component))
            low = middle + 1;
        else
            high = middle;
    }
    return low;
}

static void
subscribe(ecs_script_instance_t *instance, ecs_entity_t entity, script_key_t component) {
    if (!entity || !ecs_is_alive(entity) || !script_key_valid(component))
        return;
    script_watch_t **subscriptions = instance->subscriptions.data;
    for (uint32_t i = 0; i < instance->subscriptions.size; i++)
        if (subscriptions[i]->entity == entity && subscriptions[i]->component == component)
            return;
    size_t at = watch_position(entity, component);
    script_watch_t **watches = manager.watches.data;
    script_watch_t *watch = at < manager.watches.size ? watches[at] : NULL;
    if (!watch || watch->entity != entity || watch->component != component) {
        watch = calloc(1, sizeof(*watch));
        if (!watch)
            abort();
        watch->entity = entity;
        watch->component = component;
        sicore_vec_init(&watch->listeners, sizeof(ecs_script_instance_t *));
        for (uint32_t slot = 0; slot < 3; slot++) {
            watch->observers[slot] = UINT32_MAX;
            if (!component && slot == 2)
                continue;
            watch->contexts[slot] = (script_watch_observer_t){ watch, slot };
            watch->observers[slot] = script_key_observe(
                entity,
                component,
                slot,
                watch_changed,
                (uintptr_t)&watch->contexts[slot],
                watch_removed
            );
        }
        sicore_vec_push_empty(&manager.watches, sizeof(watch));
        watches = manager.watches.data;
        memmove(watches + at + 1, watches + at, (manager.watches.size - at - 1) * sizeof(watch));
        watches[at] = watch;
    }
    sicore_vec_push(&watch->listeners, &instance, sizeof instance);
    sicore_vec_push(&instance->subscriptions, &watch, sizeof watch);
}

static void unsubscribe_all(ecs_script_instance_t *instance) {
    script_watch_t **subscriptions = instance->subscriptions.data;
    for (uint32_t i = 0; i < instance->subscriptions.size; i++) {
        script_watch_t *watch = subscriptions[i];
        ecs_script_instance_t **listeners = watch->listeners.data;
        for (uint32_t j = 0; j < watch->listeners.size; j++) {
            if (listeners[j] != instance)
                continue;
            listeners[j] = listeners[--watch->listeners.size];
            break;
        }
        if (watch->listeners.size)
            continue;
        for (uint32_t slot = 0; slot < 3; slot++)
            if (watch->observers[slot] != UINT32_MAX)
                ecs_observer_fini(watch->observers[slot]);
        size_t at = watch_position(watch->entity, watch->component);
        script_watch_t **watches = manager.watches.data;
        memmove(watches + at, watches + at + 1, (--manager.watches.size - at) * sizeof(*watches));
        sicore_vec_fini(&watch->listeners);
        free(watch);
    }
    instance->subscriptions.size = 0;
}

static void
subscribe_reads(ecs_script_instance_t *instance, const script_transaction_t *transaction) {
    const script_read_t *reads = transaction->reads.data;
    for (uint32_t i = 0; i < transaction->reads.size; i++)
        subscribe(instance, reads[i].entity, reads[i].component);
    const script_lookup_t *lookups = transaction->lookups.data;
    for (uint32_t i = 0; i < transaction->lookups.size; i++) {
        ecs_entity_t entity = script_backend_entity_lookup(lookups[i].name, lookups[i].parent);
        if (entity)
            subscribe(instance, entity, ecs_id(Name));
    }
}

static script_baseline_t *
find_baseline(sicore_vec_t *baselines, ecs_entity_t entity, script_key_t component) {
    script_baseline_t *values = baselines->data;
    for (uint32_t i = 0; i < baselines->size; i++)
        if (values[i].entity == entity && values[i].component == component)
            return &values[i];
    return NULL;
}

static void *copy_value(script_key_t component, const void *value) {
    const ecs_component_info_t *info = script_key_info(component);
    if (!value || !info->size)
        return NULL;
    void *copy = malloc((size_t)info->size);
    if (!copy)
        abort();
    script_backend_value_init(component, copy, value);
    return copy;
}

static void clear_baselines(sicore_vec_t *baselines) {
    script_baseline_t *values = baselines->data;
    for (uint32_t i = 0; i < baselines->size; i++) {
        if (!values[i].value)
            continue;
        ecs_value_fini(&values[i].ops, values[i].value, 1);
        free(values[i].value);
    }
    sicore_vec_fini(baselines);
}

bool script_instance_region_replay(
    ecs_script_instance_t *instance,
    script_transaction_t *transaction,
    const ecs_script_t *program,
    const ecs_script_node_t *node,
    ecs_entity_t parent,
    script_eval_t *context
) {
    if (!instance->has_active)
        return false;
    const script_transaction_t *active = &instance->active;
    const script_region_t *regions = active->regions.data;
    uint32_t next_id = transaction->region;
    script_region_t *candidate = &((script_region_t *)transaction->regions.data)[next_id - 1];
    uint32_t index = 0;
    for (; index < active->regions.size; index++)
        if (regions[index].program == program && regions[index].node == node &&
            regions[index].parent == parent &&
            script_context_same_captures(regions[index].captures, candidate->captures))
            break;
    if (index == active->regions.size || !regions[index].reusable)
        return false;
    const script_region_t *region = &regions[index];
    const ecs_entity_t *produced = active->produced.data;
    for (uint32_t i = region->produced_begin; i < region->produced_end; i++)
        if (!ecs_is_alive(produced[i]))
            return false;
    for (uint32_t i = index; i < region->children_end; i++) {
        const script_region_t *child = &regions[i];
        if (child->node->kind == EcsAstEntity && child->produced_begin < child->produced_end &&
            child->name &&
            script_transaction_entity(transaction, child->name, child->parent, false) !=
                produced[child->produced_begin])
            return false;
    }
    const script_read_t *reads = active->reads.data;
    for (uint32_t i = region->reads_begin; i < region->reads_end; i++)
        if (script_transaction_find(transaction, reads[i].entity, reads[i].component) ||
            script_read_changed(&reads[i]))
            return false;
    const script_lookup_t *lookups = active->lookups.data;
    for (uint32_t i = region->lookups_begin; i < region->lookups_end; i++)
        if (script_transaction_entity(transaction, lookups[i].name, lookups[i].parent, false) !=
            lookups[i].resolved)
            return false;
    const script_staged_value_t *outputs = active->values.data;
    for (uint32_t i = region->values_begin; i < region->values_end; i++)
        if (!script_key_valid(outputs[i].component) ||
            outputs[i].schema != script_key_info(outputs[i].component)->type ||
            script_transaction_find(transaction, outputs[i].entity, outputs[i].component))
            return false;
    /* Preserve the subtree, including dependency ownership. If its parent is
     * invalidated next time, the unchanged descendants can still be scheduled. */
    for (uint32_t i = index; i < region->children_end; i++) {
        script_region_t copy = script_region_clone(&regions[i]);
        copy.children_end = next_id - 1 + copy.children_end - index;
#define REBASE_RANGE(field)                                                                        \
    copy.field##_begin =                                                                           \
        candidate->field##_begin + regions[i].field##_begin - region->field##_begin;               \
    copy.field##_end = candidate->field##_begin + regions[i].field##_end - region->field##_begin
        REBASE_RANGE(values);
        REBASE_RANGE(reads);
        REBASE_RANGE(lookups);
        REBASE_RANGE(produced);
        REBASE_RANGE(relations);
        REBASE_RANGE(prefabs);
        REBASE_RANGE(identities);
#undef REBASE_RANGE
        if (i == index) {
            copy.captures = candidate->captures;
            *candidate = copy;
        } else {
            copy.captures = script_context_clone(copy.captures);
            sicore_vec_push(&transaction->regions, &copy, sizeof copy);
            candidate = &((script_region_t *)transaction->regions.data)[next_id - 1];
        }
    }
    for (uint32_t i = region->reads_begin; i < region->reads_end; i++) {
        script_read_t copy = script_read_clone(&reads[i]);
        copy.region = next_id + copy.region - (index + 1);
        sicore_vec_push(&transaction->reads, &copy, sizeof copy);
    }
    for (uint32_t i = region->lookups_begin; i < region->lookups_end; i++) {
        script_lookup_t copy = script_lookup_clone(&lookups[i]);
        copy.region = next_id + copy.region - (index + 1);
        sicore_vec_push(&transaction->lookups, &copy, sizeof copy);
    }
    for (uint32_t i = region->produced_begin; i < region->produced_end; i++)
        sicore_vec_push(&transaction->produced, &produced[i], sizeof produced[i]);
    for (uint32_t i = region->values_begin; i < region->values_end; i++) {
        const script_staged_value_t *output = &outputs[i];
        transaction->region = next_id + output->region - (index + 1);
        script_transaction_set_members(
            transaction,
            output->entity,
            output->component,
            output->data,
            output->members
        );
        script_staged_value_t *cached =
            script_transaction_find(transaction, output->entity, output->component);
        cached->cached = true;
    }
    transaction->region = next_id;
    const script_staged_relation_t *relations = active->relations.data;
    for (uint32_t i = region->relations_begin; i < region->relations_end; i++)
        sicore_vec_push(&transaction->relations, &relations[i], sizeof relations[i]);
    const ecs_entity_t *prefabs = active->prefabs.data;
    for (uint32_t i = region->prefabs_begin; i < region->prefabs_end; i++)
        sicore_vec_push(&transaction->prefabs, &prefabs[i], sizeof prefabs[i]);
    const script_identity_t *identities = active->identities.data;
    for (uint32_t i = region->identities_begin; i < region->identities_end; i++) {
        script_identity_t copy = script_identity_clone(&identities[i]);
        sicore_vec_push(&transaction->identities, &copy, sizeof copy);
    }
    if (region->result) {
        script_eval_t result;
        script_context_copy(&result, region->result);
        script_context_restore(context, &result, context->owned_values.size);
    }
    return true;
}

static bool output_is_current(const script_staged_value_t *output) {
    if (!ecs_is_alive(output->entity) || !script_key_valid(output->component))
        return false;
    if (!script_key_has(output->entity, output->component))
        return output->remove;
    if (!output->data)
        return !output->remove;
    return script_output_equal(output, script_key_get(output->entity, output->component));
}

/* Restore each released member only while its published value is still live. */
static void restore_contribution(
    script_transaction_t *transaction,
    const script_staged_value_t *previous,
    const script_baseline_t *baseline,
    const script_staged_value_t *next
) {
    if (!script_key_valid(previous->component) ||
        previous->schema != script_key_info(previous->component)->type ||
        (transaction && script_transaction_replaces(transaction, previous->component)))
        return;
    if (!baseline || baseline->schema != previous->schema || !ecs_is_alive(previous->entity) ||
        !script_key_has(previous->entity, previous->component))
        return;
    if (next && !next->members)
        return;
    const void *live = script_key_get(previous->entity, previous->component);
    const ecs_component_info_t *info = script_key_info(previous->component);
    const sireflect_fields_t *fields = sireflect_type_fields(info->type);
    if (!previous->members && !next && output_is_current(previous)) {
        if (transaction)
            script_transaction_set(
                transaction,
                previous->entity,
                previous->component,
                baseline->value,
                !baseline->present
            );
        else if (!baseline->present)
            script_key_write(previous->entity, previous->component, NULL, true);
        else if (baseline->value)
            script_key_write(previous->entity, previous->component, baseline->value, false);
        return;
    }
    if (!fields->field_count || !previous->data || !live)
        return;
    void *buffer = malloc((size_t)info->size);
    void *initial = malloc((size_t)info->size);
    uint8_t *mask = calloc(fields->field_count, 1);
    if (!buffer || !initial || !mask)
        abort();
    script_backend_value_init(previous->component, buffer, next ? next->data : live);
    script_backend_value_init(previous->component, initial, baseline->value);
    bool changed = false;
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        if ((previous->members && !previous->members[i]) || (next && next->members[i]))
            continue;
        if (!ecs_reflected_value_equal(
                field->type,
                (const char *)live + field->offset,
                (const char *)previous->data + field->offset
            ))
            continue;
        ecs_reflected_value_copy(
            field->type,
            (char *)buffer + field->offset,
            (const char *)initial + field->offset,
            false
        );
        mask[i] = 1;
        changed = true;
    }
    if (changed) {
        if (transaction)
            script_transaction_set_members(
                transaction,
                previous->entity,
                previous->component,
                buffer,
                mask
            );
        else
            script_key_write(previous->entity, previous->component, buffer, false);
    }
    script_backend_value_fini(previous->component, buffer);
    script_backend_value_fini(previous->component, initial);
    free(buffer);
    free(initial);
    free(mask);
}

static bool contains_entity(const sicore_vec_t *entities, ecs_entity_t entity) {
    const ecs_entity_t *values = entities->data;
    for (uint32_t i = 0; i < entities->size; i++)
        if (values[i] == entity)
            return true;
    return false;
}

static script_staged_relation_t *
find_relation(script_transaction_t *transaction, ecs_entity_t entity, ecs_relation_id_t relation) {
    script_staged_relation_t *values = transaction->relations.data;
    for (uint32_t i = 0; i < transaction->relations.size; i++)
        if (values[i].entity == entity && values[i].relation == relation)
            return &values[i];
    return NULL;
}

static script_relation_baseline_t *
find_relation_baseline(sicore_vec_t *baselines, ecs_entity_t entity, ecs_relation_id_t relation) {
    script_relation_baseline_t *values = baselines->data;
    for (uint32_t i = 0; i < baselines->size; i++)
        if (values[i].entity == entity && values[i].relation == relation)
            return &values[i];
    return NULL;
}

static bool keeps_incoming_relation(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_relation_id_t relation
) {
    const ecs_relation_info_t *info = ecs_relation_info(relation);
    if (relation != ecs_rid(IsA) && info->desc.storage != EcsRelationByTarget) {
        ecs_relation_sources_t sources = ecs_relation_sources(entity, relation);
        for (uint32_t i = 0; i < sources.count; i++) {
            if (contains_entity(&transaction->kills, sources.entities[i]))
                continue;
            const script_staged_relation_t *change =
                find_relation(transaction, sources.entities[i], relation);
            if (!change || change->target == entity)
                return true;
        }
        return false;
    }
    ecs_query_desc_t desc = { 0 };
    if (relation == ecs_rid(IsA))
        desc.is_a = entity;
    else
        desc.relations[0] = (ecs_query_relation_term_t){ .target = entity,
                                                         .id = relation,
                                                         .kind = EcsRelationTarget };
    ecs_query_id_t query = ecs_query_init(&desc);
    ecs_iter_t iterator = ecs_query_iter(query);
    bool retained = false;
    while (ecs_iter_next(&iterator)) {
        for (uint32_t i = 0; i < iterator.count; i++) {
            ecs_entity_t source = iterator.entities[i];
            if (contains_entity(&transaction->kills, source))
                continue;
            const script_staged_relation_t *change = find_relation(transaction, source, relation);
            if (change && change->target != entity)
                continue;
            retained = true;
        }
    }
    ecs_query_fini(query);
    return retained;
}

static bool
created_entity_is_removable(script_transaction_t *transaction, const script_created_t *created) {
    ecs_entity_t entity = created->entity;
    if (!ecs_is_alive(entity) || contains_entity(&transaction->produced, entity) ||
        ecs_pair_entity_in_use(entity))
        return false;
    const Name *explicit_name = script_key_get(entity, ecs_id(Name));
    const char *name = explicit_name ? explicit_name->value : NULL;
    if ((name && !created->name) || (!name && created->name) ||
        (name && strcmp(name, created->name)) ||
        ecs_target_id(entity, ecs_rid(ChildOf)) != created->parent)
        return false;
    uint32_t count = 0;
    const ecs_component_t *components = ecs_entity_component_ids(entity, &count);
    for (uint32_t i = 0; i < count; i++) {
        script_key_t component = components[i];
        if (component == ecs_id(Name) || component == ecs_id(Abstract) ||
            ecs_component_index_get(component)->relation_flags)
            continue;
        script_staged_value_t *change = script_transaction_find(transaction, entity, component);
        if (!change || !change->remove)
            return false;
    }
    const script_staged_entity_t *new_entities = transaction->entities.data;
    for (uint32_t i = 0; i < transaction->entities.size; i++)
        if (new_entities[i].parent == entity)
            return false;
    for (ecs_relation_id_t relation = 1; relation < ecs_relation_count(); relation++) {
        if (relation != ecs_rid(ChildOf)) {
            const script_staged_relation_t *change = find_relation(transaction, entity, relation);
            ecs_entity_t target = change ? change->target : ecs_target_id(entity, relation);
            if (target)
                return false;
        }
        if (keeps_incoming_relation(transaction, entity, relation))
            return false;
    }
    return true;
}

static void
plan_entity_removal(ecs_script_instance_t *instance, script_transaction_t *transaction) {
    const script_created_t *created = instance->created.data;
    for (uint32_t i = instance->created.size; i; i--) {
        if (created_entity_is_removable(transaction, &created[i - 1]))
            sicore_vec_push(&transaction->kills, &created[i - 1].entity, sizeof(ecs_entity_t));
    }
}

static void
remember_created(ecs_script_instance_t *instance, const script_transaction_t *transaction) {
    const script_staged_type_t *types = transaction->types.data;
    for (uint32_t i = 0; i < transaction->types.size; i++)
        if (!script_instance_owns_schema(instance, types[i].committed))
            sicore_vec_push_u16(&instance->schemas, types[i].committed);
    const script_staged_entity_t *entities = transaction->entities.data;
    for (uint32_t i = 0; i < transaction->entities.size; i++) {
        script_created_t created = { .entity = entities[i].handle,
                                     .parent = ecs_target_id(entities[i].handle, ecs_rid(ChildOf)),
                                     .name = entities[i].name ? script_strdup(entities[i].name)
                                                              : NULL };
        sicore_vec_push(&instance->created, &created, sizeof created);
    }
    script_created_t *created = instance->created.data;
    uint32_t index = 0;
    while (index < instance->created.size) {
        if (ecs_is_alive(created[index].entity)) {
            index++;
            continue;
        }
        free(created[index].name);
        memmove(
            created + index,
            created + index + 1,
            (--instance->created.size - index) * sizeof(*created)
        );
    }
}

static void names_changed(ecs_observer_event_t *event) {
    if (event->component != ecs_id(Name))
        return;
    script_async_names_changed();
    for (ecs_script_instance_t *instance = manager.instances; instance; instance = instance->next) {
        bool changed = instance->pending_names;
        const script_lookup_t *lookups = instance->active.lookups.data;
        for (uint32_t i = 0; !changed && instance->has_active && i < instance->active.lookups.size;
             i++) {
            char *path = script_backend_qualified_name(lookups[i].name, 0);
            changed = ecs_lookup_from(lookups[i].parent, path) != lookups[i].resolved;
            free(path);
        }
        if (changed) {
            instance->force_evaluation = true;
            enqueue(instance);
        }
    }
}

static void collect_files(ecs_script_instance_t *instance, const ecs_script_t *program);
static void manager_sync(void *context);
static void manager_fini(void *context);

static void manager_init(void) {
    if (manager.initialized)
        return;
    manager = (script_manager_t){ .initialized = true };
    sicore_vec_init(&manager.watches, sizeof(script_watch_t *));
    sicore_vec_init(&manager.owners, sizeof(script_owner_t));
    sicore_vec_init(&manager.queue, sizeof(ecs_script_instance_t *));
    sicore_vec_init(&manager.watched, sizeof(ecs_script_instance_t *));
    manager.sync = ecs_sync_register(manager_sync, NULL);
    const ecs_event_t events[] = { EcsOnSet, EcsOnRemove };
    for (uint32_t i = 0; i < 2; i++)
        manager.names[i] = ecs_observer_init(&(ecs_observer_desc_t){
            .on = events[i],
            .query.components = { { ecs_id(Name), EcsIn },
                                  { ecs_id(Abstract), EcsInOptional },
                                  { ecs_id(Disabled), EcsInOptional } },
            .callback = names_changed });
    ecs_at_fini_init(&(ecs_fini_desc_t){ .callback = manager_fini });
}

static int instance_error(ecs_script_eval_result_t *result, const char *message) {
    if (result)
        *result = (ecs_script_eval_result_t){ .error = script_strdup(message) };
    return -1;
}

static void
retain_diagnostic(ecs_script_instance_t *instance, const ecs_script_eval_result_t *result) {
    free(instance->diagnostic.error);
    instance->diagnostic = *result;
    instance->diagnostic.error = result->error ? script_strdup(result->error) : NULL;
}

static void published(script_transaction_t *transaction, void *context) {
    ecs_script_instance_t *instance = context;
    if (instance->has_active) {
        const script_staged_value_t *previous = instance->active.values.data;
        for (uint32_t i = 0; i < instance->active.values.size; i++)
            release_output(instance, previous[i].entity, previous[i].component, false);
    }
    const script_staged_value_t *outputs = transaction->values.data;
    for (uint32_t i = 0; i < transaction->owned_count; i++)
        claim_output(instance, &outputs[i], false);
    instance->stats.commits++;
}

static int evaluate_instance(
    ecs_script_instance_t *instance,
    const ecs_script_t *program,
    bool reload,
    ecs_script_eval_result_t *result
) {
    if (instance->evaluating || instance->busy_async || instance->detached ||
        instance->destroy_pending)
        return instance_error(result, "instance is unavailable or already evaluating");
    instance->evaluating = true;
    ecs_sync_begin();
    instance->stats.evaluations++;
    script_transaction_t candidate;
    script_transaction_init(&candidate);
    candidate.instance = instance;
    candidate.reconcile_async = siecs_script_impl(program)->has_async || instance->has_async;
    ecs_script_eval_result_t evaluation = { 0 };
    ecs_script_eval_desc_t desc = { .parent = instance->desc.parent,
                                    .instruction_budget = instance->desc.instruction_budget,
                                    .ir = true };
    int status = -1;
    if (!candidate.failed) {
        script_transaction_active = &candidate;
        script_current_instance = instance;
        status = ecs_script_eval(program, &desc, &evaluation);
        script_transaction_active = NULL;
        script_current_instance = NULL;
    }
    if (!status && candidate.failed)
        status = instance_error(&evaluation, "transaction preparation failed");
    /* A persistent output may not read its own previous result as an input. */
    if (!status) {
        const script_read_t *reads = candidate.reads.data;
        for (uint32_t i = 0; i < candidate.reads.size && !status; i++) {
            const script_staged_value_t *output =
                script_transaction_find(&candidate, reads[i].entity, reads[i].component);
            if (output && script_output_reads(output, &reads[i]))
                status = instance_error(&evaluation, "cyclic script output dependency");
        }
    }
    if (!status && conflicting_outputs(instance, &candidate))
        status = instance_error(&evaluation, "script output already has a producer");
    sicore_vec_t next_baselines;
    sicore_vec_init(&next_baselines, sizeof(script_baseline_t));
    uint32_t produced_count = candidate.values.size;
    candidate.owned_count = produced_count;
    uint32_t relation_count = candidate.relations.size;
    sicore_vec_t next_relations;
    sicore_vec_init(&next_relations, sizeof(script_relation_baseline_t));
    if (!status) {
        script_staged_value_t *outputs = candidate.values.data;
        for (uint32_t i = 0; i < produced_count; i++) {
            script_staged_value_t *output = &outputs[i];
            script_baseline_t baseline = { .entity = output->entity,
                                           .component = output->component,
                                           .schema = output->schema,
                                           .ops = output->ops };
            if (!script_entity_is_placeholder(output->entity) &&
                script_key_valid(output->component) &&
                !script_transaction_replaces(&candidate, output->component)) {
                script_baseline_t *previous =
                    find_baseline(&instance->baselines, output->entity, output->component);
                if (previous && previous->schema != output->schema)
                    previous = NULL;
                baseline.present = previous ? previous->present
                                            : script_key_has(output->entity, output->component);
                baseline.value = copy_value(
                    output->component,
                    previous           ? previous->value
                    : baseline.present ? script_key_get(output->entity, output->component)
                                       : NULL
                );
                output->unchanged =
                    output->cached ||
                    (script_key_has(output->entity, output->component) && output->data &&
                     equal_component(
                         output->component,
                         output->data,
                         script_key_get(output->entity, output->component)
                     ));
            }
            sicore_vec_push(&next_baselines, &baseline, sizeof baseline);
        }
        if (instance->has_active) {
            script_staged_value_t *previous = instance->active.values.data;
            for (uint32_t i = 0; i < instance->active.values.size; i++) {
                script_staged_value_t *next =
                    script_transaction_find(&candidate, previous[i].entity, previous[i].component);
                script_baseline_t *baseline =
                    find_baseline(&instance->baselines, previous[i].entity, previous[i].component);
                uint8_t *authored =
                    next ? script_output_copy_mask(next->schema, next->members) : NULL;
                restore_contribution(&candidate, &previous[i], baseline, next);
                next =
                    script_transaction_find(&candidate, previous[i].entity, previous[i].component);
                if (authored && next) {
                    free(next->members);
                    next->members = authored;
                }
            }
        }
        const script_staged_relation_t *relations = candidate.relations.data;
        for (uint32_t i = 0; i < relation_count; i++) {
            script_relation_baseline_t baseline = { .entity = relations[i].entity,
                                                    .relation = relations[i].relation };
            if (!script_entity_is_placeholder(baseline.entity)) {
                const script_relation_baseline_t *previous = find_relation_baseline(
                    &instance->relation_baselines,
                    baseline.entity,
                    baseline.relation
                );
                baseline.target =
                    previous ? previous->target : ecs_target_id(baseline.entity, baseline.relation);
            }
            sicore_vec_push(&next_relations, &baseline, sizeof baseline);
        }
        if (instance->has_active) {
            const script_staged_relation_t *previous = instance->active.relations.data;
            for (uint32_t i = 0; i < instance->active.relations.size; i++) {
                if (!ecs_is_alive(previous[i].entity) ||
                    find_relation(&candidate, previous[i].entity, previous[i].relation) ||
                    ecs_target_id(previous[i].entity, previous[i].relation) != previous[i].target)
                    continue;
                const script_relation_baseline_t *baseline = find_relation_baseline(
                    &instance->relation_baselines,
                    previous[i].entity,
                    previous[i].relation
                );
                if (baseline) {
                    script_staged_relation_t restore = { baseline->entity,
                                                         baseline->relation,
                                                         baseline->target };
                    sicore_vec_push(&candidate.relations, &restore, sizeof restore);
                }
            }
        }
        plan_entity_removal(instance, &candidate);
        candidate.publish = published;
        candidate.context = instance;
        status = script_transaction_commit(&candidate);
        if (status)
            instance_error(&evaluation, "transaction commit validation failed");
    }
    if (!status) {
        unsubscribe_all(instance);
        if (instance->has_active)
            script_transaction_fini(&instance->active);
        clear_baselines(&instance->baselines);
        script_staged_value_t *transient = candidate.values.data;
        for (uint32_t i = produced_count; i < candidate.values.size; i++) {
            script_output_fini(&transient[i]);
        }
        candidate.values.size = produced_count;
        instance->active = candidate;
        instance->has_active = true;
        instance->baselines = next_baselines;
        candidate.relations.size = relation_count;
        instance->active.relations.size = relation_count;
        sicore_vec_fini(&instance->relation_baselines);
        instance->relation_baselines = next_relations;
        script_relation_baseline_t *relation_baselines = instance->relation_baselines.data;
        const script_staged_relation_t *relations = instance->active.relations.data;
        for (uint32_t i = 0; i < relation_count; i++)
            relation_baselines[i].entity = relations[i].entity;
        script_baseline_t *baselines = instance->baselines.data;
        const script_staged_value_t *outputs = instance->active.values.data;
        for (uint32_t i = 0; i < produced_count; i++) {
            baselines[i].entity = outputs[i].entity;
            baselines[i].component = outputs[i].component;
            baselines[i].schema = outputs[i].schema;
            baselines[i].ops = outputs[i].ops;
        }
        remember_created(instance, &candidate);
        const ecs_script_t *retained = ecs_script_retain(program);
        ecs_script_free((ecs_script_t *)instance->program);
        instance->program = retained;
        instance->state = EcsScriptActive;
        instance->has_async = siecs_script_impl(program)->has_async;
        instance->pending_names = false;
        if (instance->desc.watch) {
            instance->files.size = 0;
            collect_files(instance, instance->program);
        }
        subscribe_reads(instance, &instance->active);
    } else {
        if (!reload) {
            subscribe_reads(instance, &candidate);
            instance->pending_names =
                evaluation.error &&
                (strstr(evaluation.error, "unresolved identifier") ||
                 strstr(evaluation.error, "unresolved entity") ||
                 strstr(evaluation.error, "unresolved target") ||
                 strstr(evaluation.error, "unresolved namespace") ||
                 strstr(evaluation.error, "has no requested component or pair"));
            instance->state = instance->pending_names ? EcsScriptPending : EcsScriptFailed;
        }
        script_transaction_fini(&candidate);
        clear_baselines(&next_baselines);
        sicore_vec_fini(&next_relations);
    }
    retain_diagnostic(instance, &evaluation);
    instance->evaluating = false;
    if (result)
        *result = evaluation;
    else
        free(evaluation.error);
    ecs_sync_end();
    return status;
}

static uint64_t content_hash(uint64_t hash, const unsigned char *bytes, size_t count) {
    for (size_t i = 0; i < count; i++)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

static script_file_stamp_t file_stamp(const ecs_script_t *program) {
    script_file_stamp_t stamp = { .program = program };
    struct stat info;
    if (stat(program->name, &info))
        return stamp;
    stamp.exists = true;
    stamp.size = (uint64_t)info.st_size;
    stamp.modified = (uint64_t)info.st_mtime * UINT64_C(1000000000);
#if defined(__APPLE__)
    stamp.modified += (uint64_t)info.st_mtimespec.tv_nsec;
#elif !defined(_WIN32)
    stamp.modified += (uint64_t)info.st_mtim.tv_nsec;
#endif
    FILE *file = fopen(program->name, "rb");
    if (file) {
        unsigned char buffer[4096];
        size_t count;
        stamp.content = UINT64_C(14695981039346656037);
        while ((count = fread(buffer, 1, sizeof buffer, file)))
            stamp.content = content_hash(stamp.content, buffer, count);
        stamp.readable = !ferror(file);
        fclose(file);
    }
    return stamp;
}

static void collect_files(ecs_script_instance_t *instance, const ecs_script_t *program) {
    const ecs_script_impl_t *impl = siecs_script_impl(program);
    if (impl->from_file) {
        script_file_stamp_t stamp = file_stamp(program);
        /* Compare the next poll with the bytes that were actually parsed. */
        stamp.content = content_hash(
            UINT64_C(14695981039346656037),
            (const unsigned char *)program->code,
            strlen(program->code)
        );
        sicore_vec_push(&instance->files, &stamp, sizeof stamp);
    }
    ecs_script_t *const *nested = impl->nested_scripts.data;
    for (uint32_t i = 0; i < impl->nested_scripts.size; i++)
        collect_files(instance, nested[i]);
}

static void check_files(void) {
    if (!manager.watched.size)
        return;
    double now = script_monotonic_time();
    uint32_t count = manager.watched.size;
    for (uint32_t i = 0; i < count; i++) {
        ecs_script_instance_t *instance = ((ecs_script_instance_t **)manager.watched.data)[i];
        if (instance->evaluating || now < instance->next_file_check)
            continue;
        instance->next_file_check =
            now + (instance->desc.watch_interval > 0 ? instance->desc.watch_interval : 0.25);
        script_file_stamp_t *files = instance->files.data;
        bool changed = instance->watch_failed;
        for (uint32_t j = 0; j < instance->files.size; j++) {
            script_file_stamp_t current = file_stamp(files[j].program);
            if (current.exists != files[j].exists || current.readable != files[j].readable ||
                current.content != files[j].content)
                changed = true;
            files[j] = current;
        }
        if (!changed)
            continue;
        ecs_script_eval_result_t result = { 0 };
        ecs_script_t *program =
            siecs_script_impl(instance->program)->from_file
                ? ecs_script_parse_file(instance->program->name, NULL, &result)
                : ecs_script_parse(instance->program->name, instance->program->code, NULL, &result);
        if (!program) {
            instance->watch_failed = true;
            retain_diagnostic(instance, &result);
        } else {
            instance->watch_failed = evaluate_instance(instance, program, true, &result) != 0;
            if (!instance->watch_failed) {
                instance->files.size = 0;
                collect_files(instance, instance->program);
            }
            ecs_script_free(program);
        }
        free(result.error);
    }
}

int ecs_script_flush(ecs_script_eval_result_t *result) {
    if (result)
        *result = (ecs_script_eval_result_t){ 0 };
    if (!manager.initialized || manager.flushing)
        return 0;
    manager.flushing = true;
    check_files();
    int status = 0;
    uint32_t cursor = 0;
    bool first = true;
    for (;;) {
        while (cursor < manager.queue.size && cursor < 10000) {
            ecs_script_instance_t *instance =
                ((ecs_script_instance_t **)manager.queue.data)[cursor++];
            if (!instance || instance->detached)
                continue;
            instance->queued = false;
            bool changed = instance->force_evaluation || !instance->has_active ||
                           instance->state != EcsScriptActive;
            instance->force_evaluation = false;
            const script_read_t *reads = instance->active.reads.data;
            for (uint32_t i = 0; !changed && i < instance->active.reads.size; i++)
                changed = script_read_changed(&reads[i]);
            if (!changed)
                continue;
            ecs_script_eval_result_t evaluation = { 0 };
            if (evaluate_instance(instance, instance->program, false, &evaluation)) {
                status = -1;
                if (result && !result->error) {
                    *result = evaluation;
                    evaluation.error = NULL;
                }
            }
            free(evaluation.error);
        }
        if (cursor >= 10000) {
            if (!status)
                status = instance_error(result, "script stabilization budget exhausted");
            break;
        }
        if (script_async_flush(first))
            status = -1;
        first = false;
        if (cursor == manager.queue.size)
            break;
    }
    ecs_script_instance_t **queue = manager.queue.data;
    if (cursor < manager.queue.size)
        memmove(queue, queue + cursor, (manager.queue.size - cursor) * sizeof(*queue));
    manager.queue.size -= cursor;
    manager.freeing = true;
    for (;;) {
        ecs_script_instance_t *pending = manager.instances;
        while (pending && !pending->destroy_pending)
            pending = pending->next;
        if (!pending)
            break;
        ecs_script_instance_free(pending);
    }
    manager.freeing = false;
    manager.flushing = false;
    return status;
}

static void manager_sync(void *context) {
    (void)context;
    ecs_script_flush(NULL);
}

ecs_script_instance_t *ecs_script_instance_new(
    const ecs_script_t *program,
    const ecs_script_instance_desc_t *desc,
    ecs_script_eval_result_t *result
) {
    if (result)
        *result = (ecs_script_eval_result_t){ 0 };
    if (!program || (desc && desc->parent && !ecs_is_alive(desc->parent))) {
        instance_error(result, "invalid program or instance parent");
        return NULL;
    }
    manager_init();
    ecs_script_instance_t *instance = calloc(1, sizeof(*instance));
    if (!instance) {
        instance_error(result, "out of memory");
        return NULL;
    }
    if (desc)
        instance->desc = *desc;
    sicore_vec_init(&instance->baselines, sizeof(script_baseline_t));
    sicore_vec_init(&instance->async_baselines, sizeof(script_async_baseline_t));
    sicore_vec_init(&instance->subscriptions, sizeof(script_watch_t *));
    sicore_vec_init(&instance->created, sizeof(script_created_t));
    sicore_vec_init(&instance->schemas, sizeof(ecs_component_t));
    sicore_vec_init(&instance->relation_baselines, sizeof(script_relation_baseline_t));
    sicore_vec_init(&instance->files, sizeof(script_file_stamp_t));
    instance->next = manager.instances;
    manager.instances = instance;
    if (evaluate_instance(instance, program, false, result)) {
        if (instance->state != EcsScriptPending) {
            ecs_script_instance_free(instance);
            return NULL;
        }
        instance->program = ecs_script_retain(program);
    }
    if (instance->desc.watch) {
        instance->files.size = 0;
        collect_files(instance, instance->program);
        sicore_vec_push(&manager.watched, &instance, sizeof instance);
        instance->next_file_check =
            script_monotonic_time() +
            (instance->desc.watch_interval > 0 ? instance->desc.watch_interval : 0.25);
    }
    return instance;
}

ecs_script_instance_t *ecs_script_instance_run(
    const char *name,
    const char *code,
    const ecs_script_instance_desc_t *desc,
    ecs_script_eval_result_t *result
) {
    ecs_script_t *program = ecs_script_parse(name, code, NULL, result);
    if (!program)
        return NULL;
    ecs_script_instance_t *instance = ecs_script_instance_new(program, desc, result);
    ecs_script_free(program);
    return instance;
}

int ecs_script_instance_reload(
    ecs_script_instance_t *instance,
    const ecs_script_t *program,
    ecs_script_eval_result_t *result
) {
    if (!instance || !program)
        return instance_error(result, "instance and program are required");
    return evaluate_instance(instance, program, true, result);
}

static void remove_outputs(ecs_script_instance_t *instance) {
    if (!instance->has_active)
        return;
    ecs_sync_begin();
    ecs_observer_batch_begin();
    script_staged_value_t *outputs = instance->active.values.data;
    for (uint32_t i = 0; i < instance->active.values.size; i++) {
        script_baseline_t *baseline =
            find_baseline(&instance->baselines, outputs[i].entity, outputs[i].component);
        restore_contribution(NULL, &outputs[i], baseline, NULL);
    }
    const script_staged_relation_t *relations = instance->active.relations.data;
    for (uint32_t i = 0; i < instance->active.relations.size; i++) {
        if (!ecs_is_alive(relations[i].entity) ||
            ecs_target_id(relations[i].entity, relations[i].relation) != relations[i].target)
            continue;
        const script_relation_baseline_t *baseline = find_relation_baseline(
            &instance->relation_baselines,
            relations[i].entity,
            relations[i].relation
        );
        if (!baseline)
            continue;
        if (baseline->target)
            ecs_relate_id(baseline->entity, baseline->relation, baseline->target);
        else
            ecs_unrelate_id(baseline->entity, baseline->relation);
    }
    script_transaction_t removal = { 0 };
    sicore_vec_init(&removal.kills, sizeof(ecs_entity_t));
    plan_entity_removal(instance, &removal);
    const ecs_entity_t *kills = removal.kills.data;
    for (uint32_t i = 0; i < removal.kills.size; i++)
        if (ecs_is_alive(kills[i]))
            ecs_kill(kills[i]);
    sicore_vec_fini(&removal.kills);
    ecs_observer_batch_end();
    ecs_sync_end();
}

void ecs_script_instance_free(ecs_script_instance_t *instance) {
    if (!instance || instance->destroying)
        return;
    if ((instance->evaluating || instance->busy_async || manager.flushing) && !manager.finishing &&
        !manager.freeing) {
        instance->destroy_pending = true;
        return;
    }
    instance->destroying = true;
    if (!instance->detached) {
        instance->evaluating = true;
        script_async_instance_free(instance);
        unsubscribe_all(instance);
        ecs_script_instance_t **link = &manager.instances;
        while (*link && *link != instance)
            link = &(*link)->next;
        if (*link)
            *link = instance->next;
        ecs_script_instance_t **queue = manager.queue.data;
        for (uint32_t i = 0; i < manager.queue.size; i++)
            if (queue[i] == instance)
                queue[i] = NULL;
        const script_staged_value_t *owned_outputs = instance->active.values.data;
        for (uint32_t i = 0; instance->has_active && i < instance->active.values.size; i++)
            release_output(instance, owned_outputs[i].entity, owned_outputs[i].component, false);
        script_async_baseline_t *async_values = instance->async_baselines.data;
        ecs_sync_begin();
        for (uint32_t i = 0; i < instance->async_baselines.size; i++) {
            if (async_values[i].committed) {
                release_output(
                    instance,
                    async_values[i].last.entity,
                    async_values[i].last.component,
                    true
                );
                if (!manager.finishing)
                    restore_contribution(
                        NULL,
                        &async_values[i].last,
                        &async_values[i].initial,
                        NULL
                    );
            }
            if (async_values[i].initial.value) {
                ecs_value_fini(&async_values[i].initial.ops, async_values[i].initial.value, 1);
                free(async_values[i].initial.value);
            }
            script_output_fini(&async_values[i].last);
        }
        if (!manager.finishing)
            remove_outputs(instance);
        ecs_sync_end();
        sicore_vec_fini(&instance->async_baselines);
        if (instance->has_active)
            script_transaction_fini(&instance->active);
        clear_baselines(&instance->baselines);
        ecs_script_free((ecs_script_t *)instance->program);
        sicore_vec_fini(&instance->subscriptions);
        script_created_t *created = instance->created.data;
        for (uint32_t i = 0; i < instance->created.size; i++)
            free(created[i].name);
        sicore_vec_fini(&instance->created);
        sicore_vec_fini(&instance->schemas);
        sicore_vec_fini(&instance->relation_baselines);
        sicore_vec_fini(&instance->files);
        ecs_script_instance_t **watched = manager.watched.data;
        for (uint32_t i = 0; i < manager.watched.size; i++) {
            if (watched[i] == instance) {
                watched[i] = watched[--manager.watched.size];
                break;
            }
        }
    }
    free(instance->diagnostic.error);
    free(instance);
}

static void manager_fini(void *context) {
    (void)context;
    manager.finishing = true;
    while (manager.instances)
        ecs_script_instance_free(manager.instances);
    script_async_fini();
    for (uint32_t i = 0; i < 2; i++)
        ecs_observer_fini(manager.names[i]);
    ecs_sync_unregister(manager.sync);
    sicore_vec_fini(&manager.queue);
    sicore_vec_fini(&manager.watched);
    sicore_vec_fini(&manager.owners);
    sicore_vec_fini(&manager.watches);
    manager = (script_manager_t){ 0 };
}

ecs_script_instance_state_t ecs_script_instance_state(const ecs_script_instance_t *instance) {
    if (!instance || instance->state == EcsScriptFailed || instance->async_failures)
        return EcsScriptFailed;
    return instance->state == EcsScriptPending || instance->async_pending ? EcsScriptPending
                                                                          : EcsScriptActive;
}

ecs_script_stats_t ecs_script_instance_stats(const ecs_script_instance_t *instance) {
    return instance ? instance->stats : (ecs_script_stats_t){ 0 };
}

const ecs_script_eval_result_t *
ecs_script_instance_diagnostic(const ecs_script_instance_t *instance) {
    return instance ? &instance->diagnostic : NULL;
}

ecs_script_instance_t *ecs_script_instance_load(
    const char *filename,
    const ecs_script_instance_desc_t *desc,
    ecs_script_eval_result_t *result
) {
    ecs_script_t *program = ecs_script_parse_file(filename, NULL, result);
    if (!program)
        return NULL;
    ecs_script_instance_t *instance = ecs_script_instance_new(program, desc, result);
    ecs_script_free(program);
    return instance;
}

void script_instance_runtime_init(void) { manager_init(); }
void script_instance_async_activity(ecs_script_instance_t *instance, bool suspended) {
    if (suspended)
        instance->stats.suspensions++;
    else
        instance->stats.resumptions++;
}

bool script_instance_async_begin(ecs_script_instance_t *instance) {
    if (instance->evaluating || instance->busy_async || instance->destroy_pending ||
        instance->detached)
        return false;
    instance->busy_async = true;
    return true;
}

void script_instance_async_end(ecs_script_instance_t *instance) { instance->busy_async = false; }

void script_instance_async_state(
    ecs_script_instance_t *instance,
    int before,
    int after,
    const char *message,
    int32_t line,
    int32_t column
) {
    if (before == 1)
        instance->async_pending--;
    if (before == 2)
        instance->async_failures--;
    if (after == 1)
        instance->async_pending++;
    if (after == 2)
        instance->async_failures++;
    if (message || (!instance->async_pending && !instance->async_failures &&
                    instance->state == EcsScriptActive)) {
        ecs_script_eval_result_t diagnostic = { .error = (char *)message,
                                                .line = line,
                                                .column = column };
        retain_diagnostic(instance, &diagnostic);
    }
}

static script_async_baseline_t *
async_baseline(ecs_script_instance_t *instance, ecs_entity_t entity, script_key_t component) {
    script_async_baseline_t *values = instance->async_baselines.data;
    for (uint32_t i = 0; i < instance->async_baselines.size; i++)
        if (values[i].initial.entity == entity && values[i].initial.component == component)
            return &values[i];
    return NULL;
}

int script_instance_async_prepare(
    ecs_script_instance_t *instance,
    script_transaction_t *transaction
) {
    if (conflicting_outputs(instance, transaction))
        return -1;
    const script_staged_value_t *values = transaction->values.data;
    for (uint32_t i = 0; i < transaction->values.size; i++) {
        if (script_entity_is_placeholder(values[i].entity) ||
            !script_key_valid(values[i].component) ||
            async_baseline(instance, values[i].entity, values[i].component))
            continue;
        script_async_baseline_t baseline = { .initial = { .entity = values[i].entity,
                                                          .component = values[i].component } };
        baseline.initial.schema = values[i].schema;
        baseline.initial.ops = values[i].ops;
        baseline.initial.present = script_key_has(values[i].entity, values[i].component);
        baseline.initial.value = copy_value(
            values[i].component,
            baseline.initial.present ? script_key_get(values[i].entity, values[i].component) : NULL
        );
        sicore_vec_push(&instance->async_baselines, &baseline, sizeof baseline);
    }
    transaction->publish = script_instance_async_published;
    transaction->context = instance;
    return 0;
}

void script_instance_async_published(script_transaction_t *transaction, void *context) {
    ecs_script_instance_t *instance = context;
    const script_staged_value_t *values = transaction->values.data;
    for (uint32_t i = 0; i < transaction->values.size; i++) {
        script_async_baseline_t *baseline =
            async_baseline(instance, values[i].entity, values[i].component);
        if (!baseline) {
            script_async_baseline_t added = { .initial = { .entity = values[i].entity,
                                                           .component = values[i].component } };
            sicore_vec_push(&instance->async_baselines, &added, sizeof added);
            baseline = async_baseline(instance, values[i].entity, values[i].component);
        }
        if (baseline->initial.schema != values[i].schema) {
            if (baseline->initial.value) {
                ecs_value_fini(&baseline->initial.ops, baseline->initial.value, 1);
                free(baseline->initial.value);
            }
            script_output_fini(&baseline->last);
            *baseline = (script_async_baseline_t){ .initial = { .entity = values[i].entity,
                                                                .component = values[i].component,
                                                                .schema = values[i].schema,
                                                                .ops = values[i].ops } };
        }
        void *last = copy_value(values[i].component, values[i].data);
        uint8_t *members = script_output_copy_mask(values[i].schema, values[i].members);
        if (baseline->committed && values[i].members) {
            const sireflect_fields_t *fields =
                sireflect_type_fields(script_key_info(values[i].component)->type);
            uint8_t *retained = calloc(fields->field_count, 1);
            if (!retained)
                abort();
            for (size_t j = 0; j < fields->field_count; j++) {
                bool owned = !baseline->last.members || baseline->last.members[j];
                retained[j] = owned && !values[i].members[j];
                members[j] |= owned;
            }
            if (last && baseline->last.data)
                script_output_copy_members(
                    values[i].component,
                    last,
                    baseline->last.data,
                    retained
                );
            free(retained);
        }
        script_output_fini(&baseline->last);
        baseline->last = values[i];
        baseline->last.members = members;
        baseline->last.data = last;
        claim_output(instance, &baseline->last, true);
        baseline->committed = true;
    }
    remember_created(instance, transaction);
    instance->stats.commits++;
}
