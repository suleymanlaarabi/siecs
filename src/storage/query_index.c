#include "query_index.h"
#include "../relation.h"
#include "../table.h"
#include "../utils.h"
#include "../world_internal.h"
#include "component_index.h"
#include "helper.h"
#include "query_index.h"
#include "siecs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

ecs_query_index_t query_index;

static inline int ecs_compare_order_value(uint64_t a, uint64_t b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

static int ecs_query_order_relation(const ecs_table_t *a, const ecs_table_t *b, uint64_t data) {
    const ecs_relation_id_t relation = (ecs_relation_id_t)data;
    return ecs_compare_order_value(
        ecs_type_pair_get(&a->type, relation),
        ecs_type_pair_get(&b->type, relation)
    );
}

ecs_query_order_t ecs_order_by_target_id(ecs_relation_id_t relation) {
    return (ecs_query_order_t){
        .func = ecs_query_order_relation,
        .data = relation,
    };
}

ecs_query_order_t ecs_order_by_depth_id(ecs_relation_id_t relation) {
    return (ecs_query_order_t){
        .func = ecs_query_order_relation,
        .data = relation,
    };
}

void ecs_query_index_init() {
    ecs_query_index_t *index = &query_index;
    sicore_vec_init(&index->queries, sizeof(ecs_query_cache_t));
    sicore_vec_init(&index->active_ids, sizeof(ecs_query_id_t));
    index->first_free = UINT16_MAX;
}

void ecs_query_index_fini() {
    ecs_query_index_t *index = &query_index;
    for (uint32_t i = 0; i < index->queries.size; i++) {
        ecs_query_cache_t *cache = sicore_vec_get_mut(&index->queries, i, ecs_query_cache_t);
        sicore_vec_fini(&cache->table_ids);
        if (cache->alive) {
            free(cache->fields_ptr);
            free(cache->field_kind_bits);
        }
    }
    sicore_vec_fini(&index->active_ids);
    sicore_vec_fini(&index->queries);
    *index = (ecs_query_index_t){ 0 };
}

static bool ecs_query_term_is_field(ecs_query_term_t term) {
    ecs_term_access_t access = ecs_query_term_access(term);
    return access == EcsIn || access == EcsOut || access == EcsInOut || access == EcsInOptional ||
           access == EcsInOutOptional || access == EcsInUp || access == EcsInUpOptional;
}

static bool ecs_query_term_is_positive(ecs_query_term_t term) {
    ecs_term_access_t access = ecs_query_term_access(term);
    return access == EcsIn || access == EcsOut || access == EcsInOut || access == EcsFilter;
}

static ecs_component_t ecs_query_build(const ecs_query_desc_t *desc, ecs_query_cache_t *cache) {
    ecs_query_t *query = &cache->query;
    uint16_t explicit_count = 0;
    while (desc->terms[explicit_count].id) explicit_count++;
    uint16_t relation_count = 0;
    while (desc->relations[relation_count].id) {
        relation_count++;
    }
    uint16_t translated_count = 0;
    uint16_t filter_count = 0;
    for (uint16_t i = 0; i < relation_count; i++) {
        ecs_query_relation_term_t term = desc->relations[i];
        const ecs_relation_record_t *record = ecs_relation_record(term.id);
#ifndef NDEBUG
        ecs_assert(term.kind <= EcsRelationDepth, "invalid relation query kind\n");
        ecs_assert(term.kind != EcsRelationTarget ||
                       record->info.desc.storage == EcsRelationByTarget,
                   "ecs_to requires ByTarget\n");
        ecs_assert(term.kind != EcsRelationDepth ||
                       record->info.desc.storage == EcsRelationByDepth,
                   "ecs_depth requires ByDepth\n");
        for (uint16_t j = 0; j < i; j++)
            ecs_assert(desc->relations[j].id != term.id, "duplicate relation query term\n");
#endif
        if (record->info.desc.storage != EcsRelationByTarget &&
            (term.kind == EcsRelationRequired || term.kind == EcsRelationExcluded)) {
            translated_count++;
        } else {
            filter_count += term.kind != EcsRelationOptional;
        }
    }
    const ecs_component_t excludes[] = { ecs_id(Disabled), ecs_id(Abstract) };
    bool has_exclude[2] = { false, false };
    for (uint16_t i = 0; i < explicit_count; i++) {
        has_exclude[0] |= desc->terms[i].id == excludes[0];
        has_exclude[1] |= desc->terms[i].id == excludes[1];
    }
    uint16_t exclude_count = 0;
    for (uint16_t i = 0; i < 2; i++) exclude_count += excludes[i] && !has_exclude[i];
    query->term_count = explicit_count + exclude_count + translated_count;
    if (explicit_count) {
        memcpy(cache->terms, desc->terms, (size_t)explicit_count * sizeof(ecs_query_term_t));
    }
    uint16_t out = explicit_count;
    for (uint16_t i = 0; i < 2; i++) {
        if (excludes[i] && !has_exclude[i]) {
            cache->terms[out++] = (ecs_query_term_t){ excludes[i], EcsNot };
        }
    }
    for (uint16_t i = 0; i < relation_count; i++) {
        ecs_query_relation_term_t term = desc->relations[i];
        const ecs_relation_record_t *record = ecs_relation_record(term.id);
        if (record->info.desc.storage != EcsRelationByTarget &&
            (term.kind == EcsRelationRequired || term.kind == EcsRelationExcluded)) {
            cache->terms[out++] = (ecs_query_term_t){
                record->component,
                term.kind == EcsRelationRequired ? EcsFilter : EcsNot,
            };
        }
    }
    query->filter_count = filter_count;
    query->order_by = desc->order_by;
    out = 0;
    for (uint16_t i = 0; i < relation_count; i++) {
        ecs_query_relation_term_t term = desc->relations[i];
        const ecs_relation_record_t *record = ecs_relation_record(term.id);
        if ((record->info.desc.storage == EcsRelationByTarget ||
             (term.kind != EcsRelationRequired && term.kind != EcsRelationExcluded)) &&
            term.kind != EcsRelationOptional) {
            cache->filters[out++] = (ecs_query_type_filter_t){
                .value = term.target,
                .id = term.id,
                .op = term.kind == EcsRelationRequired   ? EcsQueryFilterRequired
                      : term.kind == EcsRelationExcluded ? EcsQueryFilterExcluded
                                                         : EcsQueryFilterExact,
            };
        }
    }

    query->is_a = desc->is_a;
    query->field_count = 0;
    query->field_mask = 0;
    query->up_mask = 0;
    query->access_count = 0;
    query->bloom = 0;
    ecs_component_t rarest = 0;
    uint32_t rarest_table_count = UINT32_MAX;
    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_query_term_t term = cache->terms[i];
        ecs_term_access_t access = ecs_query_term_access(term);
#ifndef NDEBUG
        ecs_assert_id_valid(term.id);
        ecs_relation_id_t source = ecs_query_term_source_relation(term);
        ecs_assert(access <= EcsInUpOptional, "invalid query term access: %d\n", access);
        ecs_assert((access >= EcsInUp) == (source != 0), "invalid query up relation\n");
        for (uint16_t j = 0; j < i; j++)
            ecs_assert(cache->terms[j].id != term.id, "duplicate query term component: %d\n", term.id);
#endif
        if (ecs_query_term_is_field(term)) {
            query->field_mask |= (uint16_t)(1u << i);
            query->field_count++;
            uint32_t access_id = term.id;
            if (access == EcsOut || access == EcsInOut || access == EcsInOutOptional) {
                access_id |= UINT32_C(1) << 16;
            }
            uint16_t at = query->access_count;
            while (at && (cache->accesses[at - 1] & UINT16_MAX) > term.id) {
                cache->accesses[at] = cache->accesses[at - 1];
                at--;
            }
            cache->accesses[at] = access_id;
            query->access_count++;
        }
        if (ecs_query_term_is_positive(term)) {
            query->bloom |= UINT64_C(1) << (term.id % 64);
            uint32_t table_count = ecs_component_index_get(term.id)->tables.size;
            if (table_count < rarest_table_count) {
                rarest = term.id;
                rarest_table_count = table_count;
            }
        }
        if (access == EcsInUp || access == EcsInUpOptional) {
            query->up_mask |= (uint16_t)(1u << i);
#ifndef NDEBUG
            const ecs_relation_record_t *record =
                ecs_relation_record(ecs_query_term_source_relation(term));
            ecs_assert(
                record->info.desc.storage == EcsRelationByTarget && record->info.desc.acyclic,
                "ecs_up requires acyclic ByTarget\n"
            );
#endif
        }
    }
    return rarest;
}

static void ecs_query_cache_set_table_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_index
) {
    const uint16_t field_count = cache->query.field_count;
    const uint32_t base = (uint32_t)table_index * field_count;
    uint16_t remaining_fields = cache->query.field_mask;
    uint16_t field_index = 0;
    uint32_t field_kind_bits = 0;

    while (remaining_fields) {
        const uint16_t term_index = (uint16_t)ECS_CTZ(remaining_fields);
        remaining_fields &= (uint16_t)(remaining_fields - 1);
        const ecs_query_term_t term = cache->terms[term_index];
        const ecs_term_access_t access = ecs_query_term_access(term);
        void *field_ptr = NULL;
        ecs_field_kind_t field_kind = EcsFieldNone;

        if (access == EcsInUp || access == EcsInUpOptional) {
            field_kind = EcsFieldNone;
        } else if (ecs_query_term_requires_owned(term)) {
            uint16_t column = ecs_table_column_or_invalid(table, term.id);
            if (column != UINT16_MAX) {
                field_ptr = table->cls[column].data;
                field_kind = EcsFieldOwned;
            }
        } else {
            bool is_shared = false;
            field_ptr = ecs_table_field(table, term.id, &is_shared);
            if (field_ptr || is_shared) {
                field_kind = is_shared ? EcsFieldShared : EcsFieldOwned;
            }
        }

        ecs_assert(
            field_kind != EcsFieldNone || access == EcsInOptional || access == EcsInOutOptional ||
                access == EcsInUp || access == EcsInUpOptional,
            "query cache matched table without field component: %d\n",
            term.id
        );

        cache->fields_ptr[base + field_index] = field_ptr;
        field_kind_bits |= (uint32_t)field_kind << (field_index * 2);
        field_index++;
    }

    cache->field_kind_bits[table_index] = field_kind_bits;
}

bool ecs_query_resolve_up_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_index
) {
    uint16_t remaining_fields = cache->query.field_mask;
    uint16_t field_index = 0;
    uint32_t field_kind_bits = cache->field_kind_bits[table_index];
    uint32_t base = (uint32_t)table_index * cache->query.field_count;
    while (remaining_fields) {
        uint16_t term_index = (uint16_t)ECS_CTZ(remaining_fields);
        remaining_fields &= (uint16_t)(remaining_fields - 1);
        ecs_query_term_t term = cache->terms[term_index];
        ecs_term_access_t access = ecs_query_term_access(term);
        if (access != EcsInUp && access != EcsInUpOptional) {
            field_index++;
            continue;
        }

        ecs_relation_id_t source_relation = ecs_query_term_source_relation(term);
        ecs_entity_t target = ecs_relation_target_at_table(table, source_relation, 0);
        void *ptr = NULL;
        while (target) {
            ptr = ecs_try_get_cid(target, term.id);
            if (ptr) {
                break;
            }
            target = ecs_target_id(target, source_relation);
        }
        cache->fields_ptr[base + field_index] = ptr;
        field_kind_bits &= ~(3u << (field_index * 2));
        if (ptr) {
            field_kind_bits |= (uint32_t)EcsFieldShared << (field_index * 2);
        } else if (access == EcsInUp) {
            cache->field_kind_bits[table_index] = field_kind_bits;
            return false;
        }
        field_index++;
    }
    cache->field_kind_bits[table_index] = field_kind_bits;
    return true;
}

static void ecs_query_cache_reserve_fields(ecs_query_cache_t *cache, uint16_t count) {
    if (!cache->query.field_count || count <= cache->field_table_capacity) {
        return;
    }
    uint16_t capacity = cache->field_table_capacity ? cache->field_table_capacity : 4;
    while (capacity < count) {
        capacity *= 2;
    }
    cache->fields_ptr =
        realloc(cache->fields_ptr, sizeof(void *) * (uint32_t)capacity * cache->query.field_count);
    cache->field_kind_bits = realloc(cache->field_kind_bits, sizeof(uint32_t) * capacity);
    cache->field_table_capacity = capacity;
}

static void ecs_query_cache_add_matched_table(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_id
) {
    sicore_vec_push_u16(&cache->table_ids, table_id);
    const uint16_t table_count = cache->table_ids.size;
    const uint16_t old_count = table_count - 1;
    const uint16_t field_count = cache->query.field_count;
    ecs_query_cache_reserve_fields(cache, table_count);

    uint16_t insert = old_count;
    const ecs_query_order_t order_by = cache->query.order_by;
    uint16_t *ids = cache->table_ids.data;
    while (order_by.func && insert &&
           order_by.func(ecs_get_table(ids[insert - 1]), table, order_by.data) > 0) {
        ids[insert] = ids[insert - 1];
        insert--;
    }
    ids[insert] = table_id;
    if (field_count && insert != old_count) {
        memmove(
            &cache->fields_ptr[(uint32_t)(insert + 1) * field_count],
            &cache->fields_ptr[(uint32_t)insert * field_count],
            (size_t)(old_count - insert) * field_count * sizeof(void *)
        );
        memmove(
            &cache->field_kind_bits[insert + 1],
            &cache->field_kind_bits[insert],
            (size_t)(old_count - insert) * sizeof(uint32_t)
        );
    }

    if (field_count) ecs_query_cache_set_table_fields(cache, table, insert);
}

static void
ecs_query_index_update_matches(ecs_query_cache_t *query_cache, ecs_component_t component);

ecs_query_id_t ecs_query_index_create(const ecs_query_desc_t *desc) {
    ecs_query_index_t *index = &query_index;
    ecs_query_id_t id;
    ecs_query_cache_t *query_cache;
    bool reused;

    if (index->first_free != UINT16_MAX) {
        reused = true;
        id = index->first_free;
        query_cache = sicore_vec_get_mut(&index->queries, id, ecs_query_cache_t);
        index->first_free = query_cache->next_free;
    } else {
        reused = false;
        query_cache = sicore_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
        id = index->queries.size - 1;
    }

    const ecs_component_t match_component = ecs_query_build(desc, query_cache);
    if (reused) {
        query_cache->table_ids.size = 0;
    } else {
        sicore_vec_init(&query_cache->table_ids, sizeof(uint16_t));
    }
    query_cache->fields_ptr = NULL;
    query_cache->field_kind_bits = NULL;
    query_cache->field_table_capacity = 0;
    query_cache->active_index = index->active_ids.size;
    query_cache->next_free = UINT16_MAX;
    query_cache->alive = true;
    sicore_vec_push_u16(&index->active_ids, id);

    ecs_query_index_update_matches(query_cache, match_component);

    return id;
}

static void
ecs_query_index_update_matches(ecs_query_cache_t *query_cache, ecs_component_t component) {
    if (query_cache->query.filter_count) {
        for (uint16_t i = 0; i < query_cache->query.filter_count; i++) {
            ecs_query_type_filter_t filter = query_cache->filters[i];
            if (filter.op == EcsQueryFilterExact) {
                ecs_pair_tables_t tables = ecs_table_index_pair_tables(filter.id, filter.value);
                if (!tables.count) {
                    return;
                }
                for (uint16_t t = 0; t < tables.count; t++) {
                    const ecs_table_t *table = ecs_get_table(tables.ids[t]);
                    if (ecs_query_match_table(query_cache, table)) {
                        ecs_query_cache_add_matched_table(query_cache, table, tables.ids[t]);
                    }
                }
                return;
            }
        }
    }
    if (component) {
        const sicore_vec_t *tables_vec = &ecs_component_index_get(component)->tables;
        sicore_vec_iter(tables_vec, uint16_t, table_id, {
            const ecs_table_t *table = &table_index.tables[*table_id];

            if (ecs_query_match_table(query_cache, table)) {
                    ecs_query_cache_add_matched_table(query_cache, table, *table_id);
            }
        });
    } else {
        const uint16_t table_count = table_index.table_count;
        const ecs_table_t *tables = table_index.tables;
        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(query_cache, &tables[i])) {
                ecs_query_cache_add_matched_table(query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = query_index.active_ids.data;
    for (uint32_t i = 0; i < query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&query_index.queries, active_ids[i], ecs_query_cache_t);
        if (ecs_query_match_table(cache, table)) {
            ecs_query_cache_add_matched_table(cache, table, table_id);
        }
    }
}

void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = query_index.active_ids.data;

    for (uint32_t i = 0; i < query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&query_index.queries, active_ids[i], ecs_query_cache_t);
        if (cache->query.field_count == 0) {
            continue;
        }

        const uint16_t *table_ids = cache->table_ids.data;
        for (uint32_t table_index = 0; table_index < cache->table_ids.size; table_index++) {
            if (table_ids[table_index] == table_id) {
                ecs_query_cache_set_table_fields(cache, table, table_index);
                break;
            }
        }
    }
}
