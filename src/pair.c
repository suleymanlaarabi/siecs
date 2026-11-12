#include "pair.h"
#include "storage/query_index.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    ecs_pair_id_t pair;
    uint32_t row;
} pair_entry_t;
typedef struct {
    ecs_entity_t entity;
    pair_entry_t *entries;
    uint32_t count, capacity;
    ecs_pair_id_t *targets;
    uint32_t target_count, target_capacity;
    bool retiring;
} pair_source_t;
typedef struct {
    ecs_pair_desc_t desc;
    ecs_pair_id_t id;
    ecs_type_ops_t ops;
    ecs_entity_t *entities;
    void *data;
    uint32_t count, capacity;
    size_t size;
    uint64_t tick;
    bool active;
    bool retiring;
} pair_column_t;
static struct {
    pair_column_t **columns;
    uint32_t count, capacity;
    ecs_pair_id_t *slots;
    uint32_t slot_capacity;
    pair_source_t **pages;
    uint32_t page_capacity;
    ecs_pair_id_t first;
} pairs;
static ecs_pair_id_t pair_sequence;

static bool reserve(void **data, uint32_t *capacity, uint32_t count, size_t stride) {
    if (count <= *capacity)
        return true;
    uint32_t next = *capacity ? *capacity : 8;
    while (next < count) {
        if (next > UINT32_MAX / 2)
            return false;
        next *= 2;
    }
    if (next > SIZE_MAX / stride)
        return false;
    void *replacement = realloc(*data, next * stride);
    if (!replacement)
        return false;
    *data = replacement;
    *capacity = next;
    return true;
}

static pair_column_t *column(ecs_pair_id_t pair) {
    return pair && pair >= pairs.first && pair - pairs.first < pairs.count
               ? pairs.columns[pair - pairs.first]
               : NULL;
}

static pair_source_t *source(ecs_entity_t entity, bool create) {
    uint32_t page = ecs_entity_id(entity) >> 8;
    if (page >= pairs.page_capacity) {
        if (!create)
            return NULL;
        uint32_t previous = pairs.page_capacity;
        if (!reserve((void **)&pairs.pages, &pairs.page_capacity, page + 1, sizeof(*pairs.pages)))
            return NULL;
        memset(pairs.pages + previous, 0, (pairs.page_capacity - previous) * sizeof(*pairs.pages));
    }
    if (!pairs.pages[page] && create)
        pairs.pages[page] = calloc(256, sizeof(pair_source_t));
    pair_source_t *owner =
        pairs.pages[page] ? &pairs.pages[page][ecs_entity_id(entity) & 255] : NULL;
    if (owner && owner->entity != entity) {
        if (!create)
            return NULL;
        owner->entity = entity;
        owner->retiring = false;
    }
    return owner;
}

static uint32_t entry_index(const pair_source_t *source, ecs_pair_id_t pair) {
    uint32_t lo = 0, hi = source ? source->count : 0;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (source->entries[mid].pair < pair)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static pair_entry_t *entry(ecs_entity_t entity, ecs_pair_id_t pair) {
    pair_source_t *owner = source(entity, false);
    uint32_t index = entry_index(owner, pair);
    return owner && index < owner->count && owner->entries[index].pair == pair
               ? &owner->entries[index]
               : NULL;
}

static bool element_valid(ecs_pair_element_t element) {
    switch (element.kind) {
    case EcsPairEntity:
        return element.id && ecs_is_alive(element.id);
    case EcsPairComponent:
        return element.id <= UINT16_MAX && ecs_component_info((ecs_component_t)element.id);
    case EcsPairRelation:
        return element.id <= UINT16_MAX && ecs_relation_info((ecs_relation_id_t)element.id);
    default:
        return false;
    }
}

static bool same_element(ecs_pair_element_t a, ecs_pair_element_t b) {
    return a.kind == b.kind && a.id == b.id;
}

static uint64_t identity_hash(const ecs_pair_desc_t *desc) {
    uint64_t hash = UINT64_C(14695981039346656037);
    const uint64_t words[] = { desc->first.id,
                               desc->first.kind,
                               desc->second.id,
                               desc->second.kind };
    for (unsigned i = 0; i < 4; i++) {
        hash ^= words[i];
        hash *= UINT64_C(1099511628211);
        hash ^= hash >> 32;
    }
    return hash;
}

static bool reserve_identity(void) {
    if (pairs.count + 1 <= pairs.slot_capacity / 2)
        return true;
    uint32_t capacity = pairs.slot_capacity ? pairs.slot_capacity * 2 : 16;
    if (capacity < pairs.slot_capacity)
        return false;
    ecs_pair_id_t *slots = calloc(capacity, sizeof(*slots));
    if (!slots)
        return false;
    for (uint32_t i = 0; i < pairs.count; i++) {
        uint32_t slot = (uint32_t)identity_hash(&pairs.columns[i]->desc) & (capacity - 1);
        while (slots[slot])
            slot = (slot + 1) & (capacity - 1);
        slots[slot] = pairs.first + i;
    }
    free(pairs.slots);
    pairs.slots = slots;
    pairs.slot_capacity = capacity;
    return true;
}

ecs_pair_id_t ecs_pair_find(const ecs_pair_desc_t *desc) {
    if (!desc || !pairs.slot_capacity)
        return 0;
    uint32_t slot = (uint32_t)identity_hash(desc) & (pairs.slot_capacity - 1);
    while (pairs.slots[slot]) {
        const pair_column_t *existing = column(pairs.slots[slot]);
        if (same_element(existing->desc.first, desc->first) &&
            same_element(existing->desc.second, desc->second))
            return existing->active ? existing->id : 0;
        slot = (slot + 1) & (pairs.slot_capacity - 1);
    }
    return 0;
}

ecs_pair_id_t ecs_pair_init(const ecs_pair_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("pair registration");
    if (!desc || ecs_is_deferred() || !element_valid(desc->first) || !element_valid(desc->second) ||
        !ecs_component_info(desc->payload))
        return 0;
    /* Builtin relation edges have one authoritative representation. */
    const ecs_pair_element_t elements[] = { desc->first, desc->second };
    for (unsigned i = 0; i < 2; i++)
        if (elements[i].kind == EcsPairRelation &&
            (elements[i].id == ecs_rid(ChildOf) || elements[i].id == ecs_rid(IsA)))
            return 0;
    ecs_pair_id_t existing = ecs_pair_find(desc);
    if (existing)
        return column(existing)->desc.payload == desc->payload ? existing : 0;
    if (!reserve_identity())
        return 0;
    uint32_t slot = (uint32_t)identity_hash(desc) & (pairs.slot_capacity - 1);
    while (pairs.slots[slot])
        slot = (slot + 1) & (pairs.slot_capacity - 1);
    if (pair_sequence == UINT32_MAX ||
        !reserve((void **)&pairs.columns, &pairs.capacity, pairs.count + 1, sizeof(*pairs.columns)))
        return 0;
    pair_column_t *created = calloc(1, sizeof(*created));
    if (!created)
        return 0;
    for (unsigned i = 0; i < 2; i++) {
        if (elements[i].kind != EcsPairEntity || (i && same_element(elements[0], elements[1])))
            continue;
        pair_source_t *target = source(elements[i].id, true);
        if (!target || !reserve(
                           (void **)&target->targets,
                           &target->target_capacity,
                           target->target_count + 1,
                           sizeof(*target->targets)
                       )) {
            free(created);
            return 0;
        }
    }
    ecs_pair_id_t id = ++pair_sequence;
    if (!pairs.count)
        pairs.first = id;
    const ecs_component_record_t *record = ecs_component_index_get(desc->payload);
    *created = (pair_column_t){ .desc = *desc,
                                .id = id,
                                .ops = record->ops,
                                .size = record->size,
                                .active = true };
    pairs.columns[pairs.count++] = created;
    pairs.slots[slot] = id;
    for (unsigned i = 0; i < 2; i++)
        if (elements[i].kind == EcsPairEntity && !(i && same_element(elements[0], elements[1]))) {
            pair_source_t *target = source(elements[i].id, false);
            target->targets[target->target_count++] = id;
        }
    return id;
}

const ecs_pair_desc_t *ecs_pair_info(ecs_pair_id_t pair) {
    const pair_column_t *record = column(pair);
    return record && record->active ? &record->desc : NULL;
}

bool ecs_pair_has_owned(ecs_entity_t entity, ecs_pair_id_t pair) {
    return ecs_is_alive(entity) && ecs_pair_info(pair) && entry(entity, pair);
}

bool ecs_pair_entity_in_use(ecs_entity_t entity) {
    if (!ecs_is_alive(entity))
        return false;
    const pair_source_t *owner = source(entity, false);
    if (!owner)
        return false;
    if (owner->count)
        return true;
    for (uint32_t i = 0; i < owner->target_count; i++) {
        const pair_column_t *record = column(owner->targets[i]);
        if (record && record->active && record->count)
            return true;
    }
    return false;
}

static ecs_entity_t value_owner(ecs_entity_t entity, ecs_pair_id_t pair) {
    const pair_column_t *record = column(pair);
    if (!record || !record->active)
        return 0;
    while (entity && ecs_is_alive(entity)) {
        if (entry(entity, pair))
            return entity;
        entity = ecs_target_id(entity, ecs_rid(IsA));
    }
    return 0;
}

bool ecs_pair_has(ecs_entity_t entity, ecs_pair_id_t pair) {
    return value_owner(entity, pair) != 0;
}

const void *ecs_pair_get(ecs_entity_t entity, ecs_pair_id_t pair) {
    ecs_entity_t owner = value_owner(entity, pair);
    pair_column_t *record = column(pair);
    pair_entry_t *location = owner ? entry(owner, pair) : NULL;
    return location && record->size ? (const char *)record->data + location->row * record->size
                                    : NULL;
}

static bool reserve_rows(pair_column_t *record) {
    if (record->count < record->capacity)
        return true;
    uint32_t capacity = record->capacity ? record->capacity * 2 : 8;
    if (capacity < record->capacity || (record->size && capacity > SIZE_MAX / record->size))
        return false;
    ecs_entity_t *entities = malloc(capacity * sizeof(*entities));
    void *data = record->size ? calloc(capacity, record->size) : NULL;
    if (!entities || (record->size && !data)) {
        free(entities);
        free(data);
        return false;
    }
    if (record->count) {
        memcpy(entities, record->entities, record->count * sizeof(*entities));
        ecs_value_transfer(
            &record->ops,
            record->size,
            data,
            record->data,
            record->count,
            true,
            true
        );
    }
    free(record->entities);
    free(record->data);
    record->entities = entities;
    record->data = data;
    record->capacity = capacity;
    return true;
}

static void
emit_pair(ecs_entity_t entity, ecs_pair_id_t pair, ecs_event_t event, const void *value) {
    if (!ecs_is_alive(entity))
        return;
    const ecs_entity_record_t *location = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(ecs_entity_record_table(location));
    pair_column_t *record = column(pair);
    ecs_pair_event_t data = { pair, record->desc.payload, value };
    ecs_emit(table, entity, event, 0, &data);
}

bool ecs_pair_apply(ecs_entity_t entity, ecs_pair_id_t pair, ecs_pair_op_t op, const void *value) {
    pair_column_t *record = column(pair);
    if (!ecs_is_alive(entity) || !record || !record->active ||
        (record->retiring && op != EcsPairRemove) || (op == EcsPairSet && record->size && !value))
        return false;
    pair_source_t *owner = source(entity, op == EcsPairAdd || op == EcsPairSet);
    if (owner && owner->retiring && op != EcsPairRemove)
        return false;
    pair_entry_t *location = entry(entity, pair);
    _Alignas(ecs_arena_max_align_t) unsigned char temporary[256];
    if (op == EcsPairRemove) {
        if (!location)
            return true;
        uint32_t row = location->row, index = entry_index(owner, pair);
        const ecs_entity_record_t *entity_record = ecs_get_record(entity);
        bool observed = ecs_table_observed(
            ecs_get_table(ecs_entity_record_table(entity_record)),
            EcsOnPairRemove
        );
        void *removed = !observed || !record->size         ? NULL
                        : record->size <= sizeof temporary ? temporary
                                                           : malloc(record->size);
        if (observed && record->size && !removed)
            return false;
        if (removed)
            ecs_value_transfer(
                &record->ops,
                record->size,
                removed,
                (char *)record->data + row * record->size,
                1,
                true,
                true
            );
        else if (record->size)
            ecs_value_fini(&record->ops, (char *)record->data + row * record->size, 1);
        uint32_t last = --record->count;
        memmove(
            owner->entries + index,
            owner->entries + index + 1,
            (--owner->count - index) * sizeof(*owner->entries)
        );
        if (row != last) {
            record->entities[row] = record->entities[last];
            ecs_value_transfer(
                &record->ops,
                record->size,
                (char *)record->data + row * record->size,
                (char *)record->data + last * record->size,
                1,
                true,
                true
            );
            entry(record->entities[row], pair)->row = row;
        }
        record->tick = ecs_change_tick();
        ecs_type_ops_t ops = record->ops;
        if (observed)
            emit_pair(entity, pair, EcsOnPairRemove, removed);
        if (removed)
            ecs_value_fini(&ops, removed, 1);
        if (removed != temporary)
            free(removed);
        return true;
    }
    if (op == EcsPairModified && !location)
        return false;
    if (op == EcsPairAdd && location)
        return true;
    /* Snapshot the RHS before growing a column: self assignment and copying a
     * borrowed row from the same column must survive allocation/relocation. */
    void *copy = NULL;
    if (op == EcsPairSet && record->size) {
        copy = record->size <= sizeof temporary ? temporary : malloc(record->size);
        if (!copy)
            return false;
        ecs_value_copy(&record->ops, record->size, copy, value, 1, true);
    }
    if (!location) {
        if (!owner ||
            !reserve(
                (void **)&owner->entries,
                &owner->capacity,
                owner->count + 1,
                sizeof(*owner->entries)
            ) ||
            !reserve_rows(record)) {
            if (copy)
                ecs_value_fini(&record->ops, copy, 1);
            if (copy != temporary)
                free(copy);
            return false;
        }
        uint32_t index = entry_index(owner, pair), row = record->count++;
        memmove(
            owner->entries + index + 1,
            owner->entries + index,
            (owner->count++ - index) * sizeof(*owner->entries)
        );
        owner->entries[index] = (pair_entry_t){ pair, row };
        location = owner->entries + index;
        record->entities[row] = entity;
        void *destination = record->size ? (char *)record->data + row * record->size : NULL;
        if (record->ops.ctor)
            record->ops.ctor(destination, 1);
        else if (record->size)
            memset(destination, 0, record->size);
    }
    void *destination = record->size ? (char *)record->data + location->row * record->size : NULL;
    if (copy) {
        ecs_value_transfer(&record->ops, record->size, destination, copy, 1, true, false);
        if (copy != temporary)
            free(copy);
    }
    record->tick = ecs_change_tick();
    emit_pair(entity, pair, EcsOnPairSet, destination);
    return true;
}

static bool mutate(ecs_entity_t entity, ecs_pair_id_t pair, ecs_pair_op_t op, const void *value) {
    if (!ecs_is_alive(entity) || !ecs_pair_info(pair))
        return false;
    if (ecs_is_deferred())
        return ecs_command_buffer_pair(entity, pair, op, value);
    ecs_assert_not_scheduler_parallel("immediate pair mutation");
    ecs_sync_begin();
    bool ok = ecs_pair_apply(entity, pair, op, value);
    ecs_sync_end();
    return ok;
}
bool ecs_pair_add(ecs_entity_t entity, ecs_pair_id_t pair) {
    return mutate(entity, pair, EcsPairAdd, NULL);
}
bool ecs_pair_set(ecs_entity_t entity, ecs_pair_id_t pair, const void *value) {
    return mutate(entity, pair, EcsPairSet, value);
}
bool ecs_pair_remove(ecs_entity_t entity, ecs_pair_id_t pair) {
    return mutate(entity, pair, EcsPairRemove, NULL);
}
bool ecs_pair_modified(ecs_entity_t entity, ecs_pair_id_t pair) {
    return mutate(entity, pair, EcsPairModified, NULL);
}

static bool element_matches(ecs_pair_element_t filter, ecs_pair_element_t value) {
    return (filter.kind == EcsPairAny || filter.kind == value.kind) &&
           (!filter.id || filter.id == value.id);
}

ecs_pair_iter_t ecs_pair_query_iter(const ecs_pair_query_desc_t *desc) {
    ecs_pair_iter_t it = { 0 };
    if (desc)
        it.query = *desc;
    it.tick = ecs_change_tick_take();
    return it;
}

static bool source_matches(const ecs_pair_iter_t *it, ecs_entity_t entity) {
    if (!it->query.filter_sources)
        return true;
    ecs_query_cache_t *cache = ecs_query_cache(it->query.sources);
    const ecs_entity_record_t *record = ecs_get_record(entity);
    return ecs_query_table_position(cache, ecs_entity_record_table(record)) != UINT16_MAX;
}

bool ecs_pair_next(ecs_pair_iter_t *it) {
    while (it->pair_cursor < pairs.count) {
        pair_column_t *record = pairs.columns[it->pair_cursor];
        ecs_pair_id_t id = pairs.first + it->pair_cursor;
        if (record->active && (!it->query.pair || it->query.pair == id) &&
            (!it->query.payload || it->query.payload == record->desc.payload) &&
            (!it->query.after_tick || record->tick > it->query.after_tick) &&
            element_matches(it->query.first, record->desc.first) &&
            element_matches(it->query.second, record->desc.second)) {
            while (it->row_cursor < record->count &&
                   !source_matches(it, record->entities[it->row_cursor]))
                it->row_cursor++;
            uint32_t first = it->row_cursor;
            while (it->row_cursor < record->count &&
                   source_matches(it, record->entities[it->row_cursor]))
                it->row_cursor++;
            if (it->row_cursor > first) {
                it->pair = id;
                it->entities = record->entities + first;
                it->values =
                    record->size ? (const char *)record->data + first * record->size : NULL;
                it->count = it->row_cursor - first;
                it->stride = record->size;
                return true;
            }
        }
        it->pair_cursor++;
        it->row_cursor = 0;
    }
    it->count = 0;
    return false;
}

void ecs_pair_inherit(ecs_entity_t entity, ecs_entity_t base) {
    if (!pairs.count)
        return;
    for (ecs_entity_t ancestor = base; ancestor && ecs_is_alive(ancestor) && ecs_is_alive(entity);
         ancestor = ecs_target_id(ancestor, ecs_rid(IsA))) {
        pair_source_t *owner = source(ancestor, false);
        uint32_t count = owner ? owner->count : 0;
        /* A callback can mutate either source; retain identity handles only. */
        ecs_pair_id_t *ids = count ? malloc(count * sizeof(*ids)) : NULL;
        ecs_fatal_if(count && !ids, "out of memory inheriting pairs");
        for (uint32_t i = 0; i < count; i++)
            ids[i] = owner->entries[i].pair;
        for (uint32_t i = 0; i < count; i++) {
            pair_column_t *record = column(ids[i]);
            if (ecs_is_alive(entity) && record->active &&
                ecs_component_info(record->desc.payload)->inheritance == EcsInheritOwned &&
                !ecs_pair_has_owned(entity, ids[i]) && ecs_pair_has_owned(ancestor, ids[i]))
                ecs_fatal_if(
                    !ecs_pair_apply(entity, ids[i], EcsPairSet, ecs_pair_get(ancestor, ids[i])),
                    "could not inherit pair"
                );
        }
        free(ids);
    }
}

void ecs_pair_entity_fini(ecs_entity_t entity) {
    pair_source_t *owner = source(entity, false);
    if (!owner || owner->retiring)
        return;
    owner->retiring = true;
    while (owner->count)
        ecs_fatal_if(
            !ecs_pair_apply(entity, owner->entries[owner->count - 1].pair, EcsPairRemove, NULL),
            "could not remove pair"
        );
    while (owner->target_count) {
        ecs_pair_id_t id = owner->targets[--owner->target_count];
        pair_column_t *record = column(id);
        if (!record || !record->active)
            continue;
        record->retiring = true;
        while (record->count)
            ecs_fatal_if(
                !ecs_pair_apply(record->entities[record->count - 1], id, EcsPairRemove, NULL),
                "could not remove target pair"
            );
        record->active = false;
        free(record->data);
        free(record->entities);
        record->data = NULL;
        record->entities = NULL;
        record->capacity = 0;
    }
}

void ecs_pair_storage_fini(void) {
    for (uint32_t i = 0; i < pairs.count; i++) {
        pair_column_t *record = pairs.columns[i];
        if (record->size)
            ecs_value_fini(&record->ops, record->data, record->count);
        free(record->data);
        free(record->entities);
        free(record);
    }
    for (uint32_t page = 0; page < pairs.page_capacity; page++) {
        if (!pairs.pages[page])
            continue;
        for (unsigned row = 0; row < 256; row++) {
            free(pairs.pages[page][row].entries);
            free(pairs.pages[page][row].targets);
        }
        free(pairs.pages[page]);
    }
    free(pairs.pages);
    free(pairs.columns);
    free(pairs.slots);
    memset(&pairs, 0, sizeof pairs);
}

typedef struct {
    pair_column_t *column;
    void *previous, *data;
    uint32_t count, capacity;
} pair_schema_column_t;
struct ecs_pair_schema_t {
    size_t size;
    ecs_type_ops_t ops;
    uint32_t count;
    uint32_t registry_count;
    bool applied;
    pair_schema_column_t columns[];
};

void ecs_pair_schema_free(ecs_pair_schema_t *plan) {
    if (!plan)
        return;
    for (uint32_t i = 0; i < plan->count; i++)
        free(plan->columns[i].data);
    free(plan);
}

bool ecs_pair_schema_prepare(
    ecs_component_t component,
    sireflect_handle_t type,
    ecs_pair_schema_t **out
) {
    *out = NULL;
    uint32_t count = 0;
    for (uint32_t i = 0; i < pairs.count; i++)
        count += pairs.columns[i]->desc.payload == component;
    size_t allocation_count = count;
    if (allocation_count > (SIZE_MAX - sizeof(ecs_pair_schema_t)) / sizeof(pair_schema_column_t))
        return false;
    ecs_pair_schema_t *plan = calloc(1, sizeof(*plan) + count * sizeof(pair_schema_column_t));
    if (!plan)
        return false;
    plan->registry_count = pairs.count;
    plan->size = sireflect_type_size(type);
    plan->ops = ecs_reflected_ops(type);
    for (uint32_t i = 0; i < pairs.count; i++) {
        pair_column_t *record = pairs.columns[i];
        if (record->desc.payload != component)
            continue;
        pair_schema_column_t *prepared = &plan->columns[plan->count++];
        *prepared = (pair_schema_column_t){ .column = record,
                                            .previous = record->data,
                                            .count = record->count,
                                            .capacity = record->capacity };
        if (plan->size) {
            if (record->capacity > SIZE_MAX / plan->size) {
                ecs_pair_schema_free(plan);
                return false;
            }
            prepared->data = record->capacity ? calloc(record->capacity, plan->size) : NULL;
            if (record->capacity && !prepared->data) {
                ecs_pair_schema_free(plan);
                return false;
            }
        }
    }
    *out = plan;
    return true;
}

bool ecs_pair_schema_validate(const ecs_pair_schema_t *plan) {
    if (!plan)
        return true;
    if (plan->applied || plan->registry_count != pairs.count)
        return false;
    for (uint32_t i = 0; i < plan->count; i++) {
        const pair_schema_column_t *prepared = &plan->columns[i];
        if (prepared->column->data != prepared->previous ||
            prepared->column->count != prepared->count ||
            prepared->column->capacity != prepared->capacity)
            return false;
    }
    return true;
}

void ecs_pair_schema_apply(ecs_pair_schema_t *plan) {
    if (!plan)
        return;
    for (uint32_t i = 0; i < plan->count; i++) {
        pair_schema_column_t *prepared = &plan->columns[i];
        pair_column_t *record = prepared->column;
        if (record->size)
            ecs_value_fini(&record->ops, record->data, record->count);
        free(record->data);
        record->data = prepared->data;
        prepared->data = NULL;
        record->size = plan->size;
        record->ops = plan->ops;
        record->tick = ecs_change_tick();
    }
    plan->applied = true;
}

void ecs_pair_schema_notify(const ecs_pair_schema_t *plan) {
    for (uint32_t i = 0; plan && i < plan->count; i++) {
        pair_column_t *record = plan->columns[i].column;
        for (uint32_t row = 0; row < record->count; row++) {
            emit_pair(
                record->entities[row],
                record->id,
                EcsOnPairSet,
                record->size ? (char *)record->data + row * record->size : NULL
            );
        }
    }
}
