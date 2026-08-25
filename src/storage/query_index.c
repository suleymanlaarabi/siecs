#include "observer_index.h"
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

#define ECS_QUERY_COMPILED_COMPONENT_CAPACITY \
    (ECS_QUERY_TERM_CAPACITY + ECS_QUERY_RELATION_CAPACITY + 2)

typedef struct {
    uint64_t bloom;
    ecs_entity_t is_a;
    ecs_query_order_t order_by;
    ecs_component_term_t fields[ECS_QUERY_TERM_CAPACITY];
    ecs_component_term_t matches[ECS_QUERY_COMPILED_COMPONENT_CAPACITY];
    ecs_resource_term_t resources[ECS_QUERY_RESOURCE_CAPACITY];
    ecs_query_type_filter_t filters[ECS_QUERY_RELATION_CAPACITY];
    uint16_t required_field_mask;
    uint16_t up_mask;
    uint8_t field_count;
    uint8_t match_count;
    uint8_t resource_count;
    uint8_t filter_count;
    ecs_component_t rarest;
    uint32_t rarest_table_count;
} ecs_query_builder_t;

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
        free(cache->query);
        free(cache->tables);
    }
    sicore_vec_fini(&index->active_ids);
    sicore_vec_fini(&index->queries);
    *index = (ecs_query_index_t){ 0 };
}

static inline bool ecs_component_term_requires_owned(ecs_component_term_t term) {
    const ecs_access_t access = ecs_access_term_access(term);
    return access == EcsOut || access == EcsInOut || access == EcsInOutOptional;
}

static bool ecs_query_desc_tracks_tables(const ecs_query_desc_t *desc) {
    return desc->components[0].id || desc->relations[0].id ||
           desc->order_by.func || desc->is_a;
}

static void ecs_query_compile_component_term(ecs_query_builder_t *builder,
                                             ecs_component_term_t term) {
    const ecs_access_t access = ecs_access_term_access(term);
#ifndef NDEBUG
    ecs_assert_id_valid(term.id);
    const ecs_relation_id_t source = ecs_access_term_source_relation(term);
    ecs_assert(access <= EcsInUpOptional, "invalid query term access: %d\n", access);
    ecs_assert((access >= EcsInUp) == (source != 0), "invalid query up relation\n");
    for (uint8_t i = 0; i < builder->field_count; i++)
        ecs_assert(builder->fields[i].id != term.id, "duplicate query term component: %d\n", term.id);
    for (uint8_t i = 0; i < builder->match_count; i++)
        ecs_assert(builder->matches[i].id != term.id, "duplicate query term component: %d\n", term.id);
#endif

    if (access == EcsFilter || access == EcsNot) {
        builder->matches[builder->match_count++] = term;
    } else {
        const uint8_t field_index = builder->field_count++;
        builder->fields[field_index] = term;
        if (access <= EcsInOut)
            builder->required_field_mask |= (uint16_t)(1u << field_index);
        if (access == EcsInUp || access == EcsInUpOptional) {
            builder->up_mask |= (uint16_t)(1u << field_index);
#ifndef NDEBUG
            const ecs_relation_record_t *record =
                ecs_relation_record(ecs_access_term_source_relation(term));
            ecs_assert(record->info.desc.storage == EcsRelationByTarget &&
                           record->info.desc.acyclic,
                       "ecs_up requires acyclic ByTarget\n");
#endif
        }
    }

    if (access == EcsIn || access == EcsOut || access == EcsInOut || access == EcsFilter) {
        builder->bloom |= UINT64_C(1) << (term.id % 64);
        const uint32_t table_count = ecs_component_index_get(term.id)->tables.size;
        if (table_count < builder->rarest_table_count) {
            builder->rarest = term.id;
            builder->rarest_table_count = table_count;
        }
    }
}

static void ecs_query_compile_resources(const ecs_query_desc_t *desc,
                                        ecs_query_builder_t *builder) {
    for (uint8_t i = 0; i < ECS_QUERY_RESOURCE_CAPACITY && desc->resources[i].id; i++) {
        const ecs_resource_term_t term = desc->resources[i];
#ifndef NDEBUG
        const ecs_access_t access = ecs_access_term_access(term);
        ecs_assert(ecs_resource_is_registered_rid((ecs_resource_t)term.id), "invalid resource id: %u\n", term.id);
        ecs_assert(ecs_access_term_source_relation(term) == 0, "resource access cannot have a source relation\n");
        ecs_assert(access == EcsIn || access == EcsOut || access == EcsInOut,
                   "resource access must use ecs_in, ecs_out, or ecs_inout\n");
#endif
        builder->resources[builder->resource_count++] = term;
    }
}

static ecs_query_t *ecs_query_build(
    const ecs_query_desc_t *desc,
    bool tracks_tables,
    ecs_component_t *match_component
) {
    ecs_query_builder_t builder = {
        .is_a = desc->is_a, .order_by = desc->order_by,
        .rarest_table_count = UINT32_MAX,
    };
    ecs_query_compile_resources(desc, &builder);

    if (tracks_tables) {
        const ecs_component_t excludes[] = { ecs_id(Disabled), ecs_id(Abstract) };
        bool has_exclude[2] = { false, false };
        for (uint8_t i = 0; i < ECS_QUERY_TERM_CAPACITY && desc->components[i].id; i++) {
            has_exclude[0] |= desc->components[i].id == excludes[0];
            has_exclude[1] |= desc->components[i].id == excludes[1];
            ecs_query_compile_component_term(&builder, desc->components[i]);
        }
        for (uint8_t i = 0; i < 2; i++) {
            if (excludes[i] && !has_exclude[i]) {
                ecs_query_compile_component_term(
                    &builder, (ecs_component_term_t){ excludes[i], EcsNot });
            }
        }
        for (uint8_t i = 0; i < ECS_QUERY_RELATION_CAPACITY && desc->relations[i].id; i++) {
            const ecs_query_relation_term_t term = desc->relations[i];
            const ecs_relation_record_t *record = ecs_relation_record(term.id);
#ifndef NDEBUG
            ecs_assert(term.kind <= EcsRelationDepth, "invalid relation query kind\n");
            ecs_assert(term.kind != EcsRelationTarget || record->info.desc.storage == EcsRelationByTarget,
                       "ecs_to requires ByTarget\n");
            ecs_assert(term.kind != EcsRelationDepth || record->info.desc.storage == EcsRelationByDepth,
                       "ecs_depth requires ByDepth\n");
            for (uint8_t j = 0; j < i; j++)
                ecs_assert(desc->relations[j].id != term.id, "duplicate relation query term\n");
#endif
            if (record->info.desc.storage != EcsRelationByTarget &&
                (term.kind == EcsRelationRequired || term.kind == EcsRelationExcluded)) {
                ecs_query_compile_component_term(&builder, (ecs_component_term_t){
                        record->component,
                        term.kind == EcsRelationRequired ? EcsFilter : EcsNot,
                    });
            } else if (term.kind != EcsRelationOptional) {
                builder.filters[builder.filter_count++] = (ecs_query_type_filter_t){
                    .value = term.target,
                    .id = term.id,
                    .op = term.kind == EcsRelationRequired   ? EcsQueryFilterRequired
                          : term.kind == EcsRelationExcluded ? EcsQueryFilterExcluded
                                                             : EcsQueryFilterExact,
                };
            }
        }
    }

    *match_component = builder.rarest;
    const size_t size = sizeof(ecs_query_t) +
        (size_t)(builder.field_count + builder.match_count) * sizeof(ecs_component_term_t) +
        (size_t)builder.resource_count * sizeof(ecs_resource_term_t) + (size_t)builder.filter_count * sizeof(ecs_query_type_filter_t);
    ecs_query_t *query = malloc(size);
    *query = (ecs_query_t){
        .bloom = builder.bloom,
        .is_a = builder.is_a,
        .order_by = builder.order_by,
        .required_field_mask = builder.required_field_mask, .up_mask = builder.up_mask,
        .field_count = builder.field_count, .match_count = builder.match_count,
        .filter_count = builder.filter_count, .resource_count = builder.resource_count,
    };
    memcpy((void *)ecs_query_fields(query), builder.fields,
           (size_t)builder.field_count * sizeof(ecs_component_term_t));
    memcpy((void *)ecs_query_match_terms(query), builder.matches,
           (size_t)builder.match_count * sizeof(ecs_component_term_t));
    memcpy((void *)ecs_query_resources(query), builder.resources,
           (size_t)builder.resource_count * sizeof(ecs_resource_term_t));
    memcpy((void *)ecs_query_filters(query), builder.filters,
           (size_t)builder.filter_count * sizeof(ecs_query_type_filter_t));
    return query;
}

static inline bool ecs_query_match_table(const ecs_query_cache_t *cache, const ecs_table_t *table) {
    const ecs_query_t *query = cache->query;
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) return false;
    if (query->is_a && !ecs_table_is_a(table, query->is_a)) return false;

    const ecs_component_term_t *fields = ecs_query_fields(query);
    uint16_t required = query->required_field_mask;
    while (required) {
        const uint8_t field_index = (uint8_t)ECS_CTZ(required);
        required &= (uint16_t)(required - 1);
        const ecs_component_term_t term = fields[field_index];
        const ecs_access_t access = ecs_access_term_access(term);
        if ((access == EcsOut || access == EcsInOut) &&
            ecs_table_column_or_invalid(table, term.id) == UINT16_MAX) return false;
        if (access == EcsIn && !ecs_table_has(table, term.id)) return false;
    }

    const ecs_component_term_t *matches = ecs_query_match_terms(query);
    for (uint8_t i = 0; i < query->match_count; i++) {
        const ecs_component_term_t term = matches[i];
        const ecs_access_t access = ecs_access_term_access(term);
        if ((access == EcsNot && ecs_table_has(table, term.id)) ||
            (access == EcsFilter && !ecs_table_has(table, term.id))) return false;
    }

    const ecs_query_type_filter_t *filters = ecs_query_filters(query);
    for (uint8_t i = 0; i < query->filter_count; i++) {
        const ecs_query_type_filter_t filter = filters[i];
        const uint16_t pair_index = ecs_type_pair_index(&table->type, filter.id);
        const bool present = pair_index != UINT16_MAX;
        if ((filter.op == EcsQueryFilterRequired && !present) ||
            (filter.op == EcsQueryFilterExcluded && present) ||
            (filter.op == EcsQueryFilterExact &&
             (!present || ecs_type_pairs(&table->type)[pair_index].value != filter.value))) return false;
    }
    return true;
}

static void ecs_query_cache_set_table_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    ecs_query_table_t *query_table
) {
    const ecs_component_term_t *fields = ecs_query_fields(cache->query);
    uint32_t field_kind_bits = 0;
    for (uint8_t field_index = 0; field_index < cache->query->field_count; field_index++) {
        const ecs_component_term_t term = fields[field_index];
        const ecs_access_t access = ecs_access_term_access(term);
        void *field_ptr = NULL;
        ecs_field_kind_t field_kind = EcsFieldNone;

        if (ecs_component_term_requires_owned(term)) {
            const uint16_t column = ecs_table_column_or_invalid(table, term.id);
            if (column != UINT16_MAX) {
                field_ptr = table->cls[column].data;
                field_kind = EcsFieldOwned;
            }
        } else if (access != EcsInUp && access != EcsInUpOptional) {
            bool is_shared = false;
            field_ptr = ecs_table_field(table, term.id, &is_shared);
            if (field_ptr || is_shared)
                field_kind = is_shared ? EcsFieldShared : EcsFieldOwned;
        }

        ecs_assert(
            field_kind != EcsFieldNone || access == EcsInOptional || access == EcsInOutOptional ||
                access == EcsInUp || access == EcsInUpOptional,
            "query cache matched table without field component: %d\n",
            term.id
        );
        query_table->fields[field_index] = field_ptr;
        field_kind_bits |= (uint32_t)field_kind << (field_index * 2);
    }
    query_table->field_kind_bits = field_kind_bits;
}

bool ecs_query_resolve_up_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    ecs_query_table_t *query_table
) {
    uint16_t up_fields = cache->query->up_mask;
    while (up_fields) {
        const uint8_t field_index = (uint8_t)ECS_CTZ(up_fields);
        up_fields &= (uint16_t)(up_fields - 1);
        const ecs_component_term_t term = ecs_query_fields(cache->query)[field_index];
        const ecs_access_t access = ecs_access_term_access(term);
        const ecs_relation_id_t source_relation = ecs_access_term_source_relation(term);
        ecs_entity_t target = ecs_relation_target_at_table(table, source_relation, 0);
        void *ptr = NULL;
        while (target) {
            ptr = ecs_try_get_cid(target, term.id);
            if (ptr) break;
            target = ecs_target_id(target, source_relation);
        }
        query_table->fields[field_index] = ptr;
        query_table->field_kind_bits &= ~(3u << (field_index * 2));
        if (ptr)
            query_table->field_kind_bits |= (uint32_t)EcsFieldShared << (field_index * 2);
        else if (access == EcsInUp) return false;
    }
    return true;
}

static inline void ecs_query_cache_reserve_tables(ecs_query_cache_t *cache, uint16_t count) {
    if (count <= cache->table_capacity) return;
    uint32_t new_capacity = cache->table_capacity ? cache->table_capacity : 4;
    while (new_capacity < count) new_capacity *= 2;
    cache->tables = realloc(cache->tables,
        (size_t)new_capacity * ecs_query_table_stride(cache->query));
    cache->table_capacity = new_capacity;
}

static void ecs_query_cache_add_matched_table(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_id
) {
    const uint16_t old_count = cache->table_count;
    ecs_query_cache_reserve_tables(cache, old_count + 1);
    const size_t stride = ecs_query_table_stride(cache->query);
    uint16_t insert = old_count;
    const ecs_query_order_t order_by = cache->query->order_by;
    while (order_by.func && insert && order_by.func(
               ecs_get_table(ecs_query_table_id(cache, insert - 1)), table, order_by.data) > 0) {
        insert--;
    }
    if (insert != old_count) {
        memmove(cache->tables + (size_t)(insert + 1) * stride,
                cache->tables + (size_t)insert * stride,
                (size_t)(old_count - insert) * stride);
    }
    cache->table_count = old_count + 1;
    if (cache->query->field_count == 0) {
        ((uint16_t *)cache->tables)[insert] = table_id;
    } else {
        ecs_query_table_t *entry = ecs_query_table_at(cache, insert);
        entry->id = table_id;
        ecs_query_cache_set_table_fields(cache, table, entry);
    }
}

static void
ecs_query_index_update_matches(ecs_query_cache_t *query_cache, ecs_component_t component);

ecs_query_id_t ecs_query_index_create(const ecs_query_desc_t *desc) {
    ecs_query_index_t *index = &query_index;
    ecs_query_id_t id;
    ecs_query_cache_t *query_cache;

    if (index->first_free != UINT16_MAX) {
        id = index->first_free;
        query_cache = sicore_vec_get_mut(&index->queries, id, ecs_query_cache_t);
        index->first_free = query_cache->next_free;
    } else {
        query_cache = sicore_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
        id = index->queries.size - 1;
    }

    const bool tracks_tables = ecs_query_desc_tracks_tables(desc);
    ecs_component_t match_component = 0;
    ecs_query_t *query = ecs_query_build(desc, tracks_tables, &match_component);
    *query_cache = (ecs_query_cache_t){
        .query = query,
        .tables = NULL,
        .active_index = UINT32_MAX,
        .observer = UINT32_MAX,
        .table_capacity = 0,
        .table_count = 0,
        .next_free = UINT16_MAX,
        .alive = true,
    };
    if (tracks_tables) {
        query_cache->active_index = index->active_ids.size;
        sicore_vec_push_u16(&index->active_ids, id);
        ecs_query_index_update_matches(query_cache, match_component);
    }
    return id;
}

static void
ecs_query_index_update_matches(ecs_query_cache_t *query_cache, ecs_component_t component) {
    const ecs_query_type_filter_t *filters = ecs_query_filters(query_cache->query);
    for (uint8_t i = 0; i < query_cache->query->filter_count; i++) {
        const ecs_query_type_filter_t filter = filters[i];
        if (filter.op == EcsQueryFilterExact) {
            const ecs_pair_tables_t tables = ecs_table_index_pair_tables(filter.id, filter.value);
            if (!tables.count) return;
            for (uint16_t t = 0; t < tables.count; t++) {
                const ecs_table_t *table = ecs_get_table(tables.ids[t]);
                if (ecs_query_match_table(query_cache, table)) {
                    ecs_query_cache_add_matched_table(query_cache, table, tables.ids[t]);
                }
            }
            return;
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
            if (cache->observer != UINT32_MAX) {
                const ecs_observer_t *observer = sicore_vec_get(
                    &observer_index.observers, cache->observer, ecs_observer_t);
                ecs_table_add_observer((ecs_table_t *)table, observer->event, cache->observer);
            }
        }
    }
}

void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = query_index.active_ids.data;
    for (uint32_t i = 0; i < query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&query_index.queries, active_ids[i], ecs_query_cache_t);
        if (cache->query->field_count == 0) {
            continue;
        }
        for (uint16_t table_index = 0; table_index < cache->table_count; table_index++) {
            if (ecs_query_table_id(cache, table_index) == table_id) {
                ecs_query_cache_set_table_fields(
                    cache,
                    table,
                    ecs_query_table_at(cache, table_index)
                );
                break;
            }
        }
    }
}
