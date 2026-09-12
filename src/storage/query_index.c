#include "query_index.h"
#include "observer_index.h"
#include "../relation.h"
#include "../world_internal.h"
#include <stdlib.h>
#include <string.h>
#define ECS_COMPILED_TERMS (ECS_QUERY_TERM_CAPACITY + ECS_QUERY_RELATION_CAPACITY + 2)
typedef struct {
    ecs_query_t query;
    ecs_component_term_t terms[ECS_COMPILED_TERMS];
    ecs_query_relation_term_t filters[ECS_QUERY_RELATION_CAPACITY];
    ecs_pair_tables_t candidates;
} ecs_query_builder_t;
ecs_query_index_t query_index;
static int ecs_query_order_relation(const ecs_table_t *a, const ecs_table_t *b, uint64_t data) {
    uint64_t av = ecs_type_pair_get(&a->type, data), bv = ecs_type_pair_get(&b->type, data);
    return (av > bv) - (av < bv);
}
ecs_query_order_t ecs_order_by_target_id(ecs_relation_id_t relation) {
    return (ecs_query_order_t){ ecs_query_order_relation, relation };
}
ecs_query_order_t ecs_order_by_depth_id(ecs_relation_id_t relation) {
    return ecs_order_by_target_id(relation);
}

void ecs_query_index_init(void) {
    sicore_vec_init(&query_index.queries, sizeof(ecs_query_cache_t));
    sicore_vec_init(&query_index.active_ids, sizeof(ecs_query_id_t));
    query_index.first_free = UINT16_MAX;
}
void ecs_query_index_fini(void) {
    for (uint32_t i = 0; i < query_index.queries.size; i++) {
        ecs_query_cache_t *cache = sicore_vec_get_mut(&query_index.queries, i, ecs_query_cache_t);
        if (cache->alive) ecs_query_fini(i);
    }
    sicore_vec_fini(&query_index.active_ids);
    sicore_vec_fini(&query_index.queries);
    query_index = (ecs_query_index_t){ 0 };
}
static void ecs_query_compile_term(ecs_query_builder_t *b, ecs_component_term_t term) {
    ecs_query_t *q = &b->query;
    ecs_access_t access = ecs_access_term_access(term);
    ecs_relation_id_t source = ecs_access_term_source_relation(term);
    ecs_assert_id_valid(term.id);
    ecs_assert(access <= EcsInUpOptional && (access >= EcsInUp) == (source != 0),
               "invalid query access or up relation\n");
#ifndef NDEBUG
    for (uint8_t i = 0; i < q->field_count + q->match_count; i++) {
        uint8_t at = i < q->field_count ? i : ECS_COMPILED_TERMS - 1 - (i - q->field_count);
        ecs_assert(b->terms[at].id != term.id, "duplicate query component: %u\n", term.id);
    }
    if (source) {
        const ecs_relation_desc_t *desc = &ecs_relation_record(source)->info.desc;
        ecs_assert(desc->storage == EcsRelationByTarget && desc->acyclic,
                   "ecs_up requires acyclic ByTarget\n");
    }
#endif
    if (access == EcsFilter || access == EcsNot) {
        b->terms[ECS_COMPILED_TERMS - ++q->match_count] = term;
    } else {
        uint16_t bit = (uint16_t)(1u << q->field_count);
        if (source) q->up_mask |= bit;
        b->terms[q->field_count++] = term;
    }
    if (access <= EcsInOut || access == EcsFilter) {
        q->bloom |= UINT64_C(1) << (term.id % 64);
        const sicore_vec_t *tables = &ecs_component_index_get(term.id)->tables;
        if (tables->size < b->candidates.count)
            b->candidates = (ecs_pair_tables_t){ tables->data, tables->size };
    }
}
ecs_query_id_t ecs_query_index_create(const ecs_query_desc_t *desc) {
    ecs_query_builder_t b = { .query = { .is_a = desc->is_a, .order_by = desc->order_by },
        .candidates = { .count = table_index.table_count } };
    ecs_query_t *q = &b.query;
    bool tracks = desc->components[0].id || desc->relations[0].id || desc->is_a || desc->order_by.func;
    ecs_component_t excludes[] = { ecs_id(Disabled), ecs_id(Abstract) };
    for (uint8_t i = 0; i < ECS_QUERY_TERM_CAPACITY && desc->components[i].id; i++) {
        ecs_component_term_t term = desc->components[i];
        for (uint8_t j = 0; j < 2; j++)
            if (term.id == excludes[j]) excludes[j] = 0;
        ecs_query_compile_term(&b, term);
    }
    for (uint8_t i = 0; tracks && i < 2; i++)
        if (excludes[i]) ecs_query_compile_term(&b, (ecs_component_term_t){ excludes[i], EcsNot });
    for (uint8_t i = 0; i < ECS_QUERY_RELATION_CAPACITY && desc->relations[i].id; i++) {
        ecs_query_relation_term_t term = desc->relations[i];
        const ecs_relation_record_t *r = ecs_relation_record(term.id);
        ecs_assert(term.kind <= EcsRelationDepth, "invalid relation query kind\n");
        ecs_assert(term.kind != EcsRelationTarget || r->info.desc.storage == EcsRelationByTarget,
                   "ecs_to requires ByTarget\n");
        ecs_assert(term.kind != EcsRelationDepth || r->info.desc.storage == EcsRelationByDepth,
                   "ecs_depth requires ByDepth\n");
#ifndef NDEBUG
        for (uint8_t j = 0; j < i; j++)
            ecs_assert(desc->relations[j].id != term.id, "duplicate relation query term\n");
#endif
        if (term.kind == EcsRelationOptional) continue;
        if (r->info.desc.storage != EcsRelationByTarget && term.kind <= EcsRelationExcluded)
            ecs_query_compile_term(&b, (ecs_component_term_t){ r->component,
                term.kind == EcsRelationRequired ? EcsFilter : EcsNot });
        else b.filters[q->filter_count++] = term;
        if (term.kind >= EcsRelationTarget) {
            ecs_pair_tables_t tables = ecs_table_index_pair_tables(term.id, term.target);
            if (tables.count < b.candidates.count) b.candidates = tables;
        }
    }
    while (q->resource_count < ECS_QUERY_RESOURCE_CAPACITY && desc->resources[q->resource_count].id) {
        ecs_resource_term_t term = desc->resources[q->resource_count];
        ecs_assert(ecs_resource_is_registered_rid(term.id), "invalid resource id: %u\n", term.id);
        ecs_assert(ecs_access_term_source_relation(term) == 0,
                   "resource access cannot have a source relation\n");
        ecs_assert(term.access <= EcsInOut, "invalid resource access\n");
        (void)term;
        q->resource_count++;
    }
    q->stride = q->field_count ? sizeof(ecs_query_table_t) + q->field_count * sizeof(void *) : 2;
    ecs_query_t *compiled = malloc(ecs_query_size(q));
    *compiled = *q;
    memcpy((void *)ecs_query_fields(compiled), b.terms, q->field_count * sizeof(*b.terms));
    memcpy((void *)ecs_query_match_terms(compiled), b.terms + ECS_COMPILED_TERMS - q->match_count,
           q->match_count * sizeof(*b.terms));
    memcpy((void *)ecs_query_resources(compiled), desc->resources, q->resource_count * sizeof(*desc->resources));
    memcpy((void *)ecs_query_filters(compiled), b.filters, q->filter_count * sizeof(*b.filters));
    ecs_query_id_t id = query_index.first_free;
    if (id == UINT16_MAX) {
        id = query_index.queries.size;
        sicore_vec_push_empty(&query_index.queries, sizeof(ecs_query_cache_t));
    } else query_index.first_free = sicore_vec_get(&query_index.queries, id, ecs_query_cache_t)->next_free;
    ecs_query_cache_t *cache = sicore_vec_get_mut(&query_index.queries, id, ecs_query_cache_t);
    *cache = (ecs_query_cache_t){ .query = compiled, .alive = true,
        .active_index = UINT32_MAX, .observer = UINT32_MAX, .next_free = UINT16_MAX };
    if (tracks) ecs_query_index_activate(id, b.candidates.ids, b.candidates.count);
    return id;
}
static bool ecs_query_bind(const ecs_query_t *q, const ecs_table_t *table, ecs_query_table_t *entry) {
    entry->field_kind_bits = 0;
    for (uint8_t i = 0; i < q->field_count; i++) {
        ecs_component_t id = ecs_query_fields(q)[i].id;
        ecs_access_t access = ecs_access_term_access(ecs_query_fields(q)[i]);
        ecs_field_kind_t kind = EcsFieldNone;
        void *ptr = NULL;
        if (access < EcsInUp) {
            uint16_t column = ecs_table_column_or_invalid(table, id);
            if (column != UINT16_MAX) {
                ptr = table->cls[column].data;
                kind = EcsFieldOwned;
            } else if ((access == EcsIn || access == EcsInOptional) && table->type.base &&
                       (access == EcsInOptional || id != ecs_id(Abstract))) {
                bool shared = false;
                ptr = ecs_table_field(table, id, &shared);
                if (shared) kind = EcsFieldShared;
            }
        }
        if (access <= EcsInOut && kind == EcsFieldNone) return false;
        entry->fields[i] = ptr;
        entry->field_kind_bits |= (uint32_t)kind << (i * 2);
    }
    return true;
}
bool ecs_query_resolve_up_fields(ecs_query_cache_t *cache, const ecs_table_t *table,
                                 ecs_query_table_t *entry) {
    uint16_t mask = cache->query->up_mask;
    while (mask) {
        uint8_t i = (uint8_t)ECS_CTZ(mask);
        mask &= (uint16_t)(mask - 1);
        ecs_component_term_t term = ecs_query_fields(cache->query)[i];
        ecs_relation_id_t relation = ecs_access_term_source_relation(term);
        ecs_entity_t target = ecs_relation_target_at_table(table, relation, 0);
        void *ptr = NULL;
        while (target && !(ptr = ecs_try_get_cid(target, term.id))) target = ecs_target_id(target, relation);
        entry->fields[i] = ptr;
        entry->field_kind_bits = (entry->field_kind_bits & ~(3u << (i * 2))) |
            (uint32_t)(ptr ? EcsFieldShared : EcsFieldNone) << (i * 2);
        if (!ptr && ecs_access_term_access(term) == EcsInUp) return false;
    }
    return true;
}

static void ecs_query_positions(ecs_query_cache_t *cache, uint16_t first) {
    if (cache->table_count < 16) return;
    ecs_id_map_t *map = &cache->positions;
    if (!map->ids) first = 0;
    if (map->capacity < table_index.table_count) {
        uint32_t capacity = map->capacity ? map->capacity : 16;
        while (capacity < table_index.table_count) capacity *= 2;
        size_t bytes = ecs_query_size(cache->query);
        cache->query = realloc(cache->query, bytes + capacity * sizeof(uint16_t));
        map->ids = (uint16_t *)((uint8_t *)cache->query + bytes);
        memset(map->ids + map->capacity, 0xff, (capacity - map->capacity) * sizeof(uint16_t));
        map->capacity = capacity;
    }
    for (uint16_t i = first; i < cache->table_count; i++)
        map->ids[ecs_query_table_id(cache, i)] = i;
}

static uint16_t ecs_query_insert(ecs_query_cache_t *cache, const ecs_table_t *table,
                                 uint16_t lo, uint16_t end) {
    uint16_t hi = end;
    ecs_query_order_t order = cache->query->order_by;
    if (lo == end || order.func(ecs_get_table(ecs_query_table_id(cache, end - 1)), table, order.data) <= 0)
        return end;
    while (lo < hi) {
        uint16_t mid = lo + (hi - lo) / 2;
        if (order.func(ecs_get_table(ecs_query_table_id(cache, mid)), table, order.data) <= 0) lo = mid + 1;
        else hi = mid;
    }
    size_t stride = cache->query->stride;
    memmove(cache->tables + (lo + 1) * stride, cache->tables + lo * stride, (end - lo) * stride);
    return lo;
}

static bool ecs_query_add(ecs_query_cache_t *cache, const ecs_table_t *table, uint16_t id, bool ordered) {
    const ecs_query_t *q = cache->query;
    if ((q->bloom & table->bloom) != q->bloom || (q->is_a && !ecs_table_is_a(table, q->is_a))) return false;
    for (uint8_t i = 0; i < q->match_count; i++) {
        ecs_component_term_t term = ecs_query_match_terms(q)[i];
        if (ecs_table_has(table, term.id) == (term.access == EcsNot)) return false;
    }
    for (uint8_t i = 0; i < q->filter_count; i++) {
        ecs_query_relation_term_t term = ecs_query_filters(q)[i];
        uint16_t pair = ecs_type_pair_index(&table->type, term.id);
        if (term.kind == EcsRelationExcluded ? pair != UINT16_MAX : pair == UINT16_MAX) return false;
        if (term.kind >= EcsRelationTarget && ecs_type_pairs(&table->type)[pair].value != term.target) return false;
    }
    if (!q->field_count) {
        if (cache->table_count == cache->table_capacity) {
            cache->table_capacity = cache->table_capacity ? cache->table_capacity * 2 : 4;
            cache->tables = realloc(cache->tables, cache->table_capacity * q->stride);
        }
        uint16_t at = cache->table_count;
        if (ordered && q->order_by.func) at = ecs_query_insert(cache, table, 0, at);
        ((uint16_t *)cache->tables)[at] = id;
        cache->table_count++;
        if (ordered) ecs_query_positions(cache, at);
        return true;
    }
    union {
        ecs_query_table_t entry;
        uint8_t bytes[sizeof(ecs_query_table_t) + ECS_QUERY_TERM_CAPACITY * sizeof(void *)];
    } result;
    result.entry.id = id;
    if (!ecs_query_bind(q, table, &result.entry)) return false;
    if (cache->table_count == cache->table_capacity) {
        cache->table_capacity = cache->table_capacity ? cache->table_capacity * 2 : 4;
        cache->tables = realloc(cache->tables, cache->table_capacity * q->stride);
    }
    uint16_t at = cache->table_count;
    if (ordered && q->order_by.func) at = ecs_query_insert(cache, table, 0, at);
    memcpy(ecs_query_table_at(cache, at), &result.entry, q->stride);
    cache->table_count++;
    if (ordered) ecs_query_positions(cache, at);
    return true;
}

static void ecs_query_sort(ecs_query_cache_t *cache, uint8_t *scratch, uint16_t begin, uint16_t end) {
    size_t stride = cache->query->stride;
    ecs_query_order_t order = cache->query->order_by;
    if (end - begin <= 16) {
        for (uint16_t i = begin + 1; i < end; i++) {
            memcpy(scratch, ecs_query_table_bytes_at(cache, i), stride);
            uint16_t at = ecs_query_insert(cache, ecs_get_table(*(uint16_t *)scratch), begin, i);
            memcpy(ecs_query_table_bytes_at(cache, at), scratch, stride);
        }
        return;
    }
    uint16_t mid = begin + (end - begin) / 2, a = begin, b = mid;
    ecs_query_sort(cache, scratch, begin, mid);
    ecs_query_sort(cache, scratch, mid, end);
    if (order.func(ecs_get_table(ecs_query_table_id(cache, mid - 1)),
                   ecs_get_table(ecs_query_table_id(cache, mid)), order.data) <= 0) return;
    for (uint16_t i = begin; i < end; i++) {
        bool left = b == end || (a < mid && order.func(ecs_get_table(ecs_query_table_id(cache, a)),
            ecs_get_table(ecs_query_table_id(cache, b)), order.data) <= 0);
        memcpy(scratch + i * stride,
               ecs_query_table_bytes_at(cache, left ? a++ : b++), stride);
    }
    memcpy(cache->tables + begin * stride, scratch + begin * stride, (end - begin) * stride);
}

void ecs_query_index_activate(ecs_query_id_t id, const uint16_t *tables, uint16_t count) {
    ecs_query_cache_t *cache = ecs_query_cache(id);
    const ecs_query_t *q = cache->query;
    cache->active_index = query_index.active_ids.size;
    sicore_vec_push_u16(&query_index.active_ids, id);
    for (uint16_t i = 0; i < count; i++) {
        uint16_t table = tables ? tables[i] : i;
        ecs_query_add(cache, ecs_get_table(table), table, false);
    }
    if (q->order_by.func && cache->table_count > 1) {
        uint8_t *scratch = malloc(cache->table_count * q->stride);
        ecs_query_sort(cache, scratch, 0, cache->table_count);
        free(scratch);
    }
    ecs_query_positions(cache, 0);
}

void ecs_query_index_add_table(const ecs_table_t *table, uint16_t id) {
    const ecs_query_id_t *ids = query_index.active_ids.data;
    for (uint32_t i = 0; i < query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache = ecs_query_cache(ids[i]);
        if (ecs_query_add(cache, table, id, true) && cache->observer != UINT32_MAX) {
            const ecs_observer_t *o = sicore_vec_get(&observer_index.observers, cache->observer, ecs_observer_t);
            ecs_table_add_observer((ecs_table_t *)table, o->event, cache->observer);
        }
    }
}
void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t id) {
    const ecs_query_id_t *ids = query_index.active_ids.data;
    for (uint32_t i = 0; i < query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache = ecs_query_cache(ids[i]);
        if (!cache->query->field_count) continue;
        uint16_t at = ecs_query_table_position(cache, id);
        if (at != UINT16_MAX) ecs_query_bind(cache->query, table, ecs_query_table_at(cache, at));
    }
}
