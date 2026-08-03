#include "query_index.h"
#include "../table.h"
#include "../relation.h"
#include "../utils.h"
#include "../world_internal.h"
#include "component_index.h"
#include "helper.h"
#include "siecs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ecs_query_index_init() {
    ecs_query_index_t *index = &ecs_world.query_index;
    sicore_vec_init(&index->queries, sizeof(ecs_query_cache_t));
    sicore_vec_init(&index->active_ids, sizeof(ecs_query_id_t));
    index->first_free = UINT16_MAX;
}

void ecs_query_index_fini() {
    ecs_query_index_t *index = &ecs_world.query_index;
    for (uint32_t i = 0; i < index->queries.size; i++) {
        ecs_query_cache_t *cache = sicore_vec_get_mut(&index->queries, i, ecs_query_cache_t);
        sicore_vec_fini(&cache->table_ids);
        if (cache->alive) {
            free(cache->fields_ptr);
            free(cache->field_kind_bits);
        }
        ecs_query_index_destroy(&cache->query);
    }
    sicore_vec_fini(&index->active_ids);
    sicore_vec_fini(&index->queries);
}

void ecs_query_index_destroy(ecs_query_t *query) {
    free(query->terms);
}

static uint16_t ecs_query_count_terms(const ecs_query_term_t *terms, uint32_t *access_bits) {
    uint16_t i = 0;
    uint32_t bits = 0;
    while (terms[i].id) {
        bits |= terms[i].access;
        i++;
    }
    *access_bits = bits;
    return i;
}

static uint16_t ecs_query_count_relation_terms(const ecs_query_relation_term_t *terms) {
    uint16_t i = 0;
    while (terms[i].id) {
        i++;
    }
    return i;
}

static bool ecs_query_has_component(
    const ecs_query_term_t *terms,
    uint16_t count,
    ecs_component_t component
) {
    for (uint16_t i = 0; i < count; i++) {
        if (terms[i].id == component) {
            return true;
        }
    }
    return false;
}

static bool ecs_query_term_is_field(ecs_query_term_t term) {
    ecs_term_access_t access = ecs_query_term_access(term);
    return access == EcsIn || access == EcsOut || access == EcsInOut ||
           access == EcsInOptional || access == EcsInOutOptional || access == EcsInUp ||
           access == EcsInUpOptional;
}

static bool ecs_query_term_is_positive(ecs_query_term_t term) {
    ecs_term_access_t access = ecs_query_term_access(term);
    return access == EcsIn || access == EcsOut || access == EcsInOut || access == EcsFilter;
}

static ecs_component_t ecs_query_rarest_positive_term(const ecs_query_t *query) {
    ecs_component_t rarest = 0;
    uint32_t rarest_table_count = UINT32_MAX;

    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            const ecs_component_t component = query->terms[i].id;
            const uint32_t table_count = ecs_component_index_get(component)->tables.size;
            if (table_count < rarest_table_count) {
                rarest = component;
                rarest_table_count = table_count;
                if (table_count == 0) {
                    break;
                }
            }
        }
    }
    return rarest;
}

#ifndef NDEBUG
static void ecs_query_validate_terms(const ecs_query_term_t *terms, uint16_t term_count) {
    for (uint16_t i = 0; i < term_count; i++) {
        ecs_term_access_t access = ecs_query_term_access(terms[i]);
        ecs_relation_id_t source_relation = ecs_query_term_source_relation(terms[i]);
        ecs_assert_id_valid(terms[i].id);
        ecs_assert(
            access == EcsIn || access == EcsOut || access == EcsInOut ||
                access == EcsInOptional || access == EcsInOutOptional ||
                access == EcsFilter || access == EcsNot || access == EcsInUp ||
                access == EcsInUpOptional,
            "invalid query term access: %d\n",
            access
        );
        ecs_assert(
            (access == EcsInUp || access == EcsInUpOptional) == (source_relation != 0),
            "invalid query up relation: term=%u access=%u relation=%u\n",
            terms[i].id,
            access,
            source_relation
        );

        for (uint16_t j = i + 1; j < term_count; j++) {
            ecs_assert(
                terms[i].id != terms[j].id,
                "duplicate query term component: %d\n",
                terms[i].id
            );
        }
    }
}

static void ecs_query_validate_relations(
    const ecs_query_relation_term_t *terms,
    uint16_t count
) {
    for (uint16_t i = 0; i < count; i++) {
        const ecs_relation_record_t *record = ecs_relation_record(terms[i].id);
        ecs_assert(
            terms[i].kind == EcsRelationRequired || terms[i].kind == EcsRelationOptional ||
                terms[i].kind == EcsRelationExcluded || terms[i].kind == EcsRelationTarget ||
                terms[i].kind == EcsRelationDepth,
            "invalid relation query kind\n"
        );
        ecs_assert(
            terms[i].kind != EcsRelationTarget || record->storage == EcsRelationByTarget,
            "ecs_to requires ByTarget\n"
        );
        ecs_assert(
            terms[i].kind != EcsRelationDepth || record->storage == EcsRelationByDepth,
            "ecs_depth requires ByDepth\n"
        );
        for (uint16_t j = i + 1; j < count; j++) {
            ecs_assert(terms[i].id != terms[j].id, "duplicate relation query term\n");
        }
    }
}
#endif

static ecs_component_t ecs_query_build(
    const ecs_query_desc_t *desc,
    ecs_query_t *query,
    uint16_t *terms_capacity
) {
    uint32_t access_bits;
    uint16_t explicit_count = ecs_query_count_terms(desc->terms, &access_bits);
    const bool has_up = access_bits >> 8;
    const uint16_t relation_count = desc->relations[0].id
                                        ? ecs_query_count_relation_terms(desc->relations)
                                        : 0;
    uint16_t translated_count = 0;
    uint16_t special_count = 0;
    for (uint16_t i = 0; i < relation_count; i++) {
        ecs_query_relation_term_t term = desc->relations[i];
        const ecs_relation_record_t *record = ecs_relation_record(term.id);
        if (record->storage != EcsRelationByTarget &&
            (term.kind == EcsRelationRequired || term.kind == EcsRelationExcluded)) {
            translated_count++;
        } else if (term.kind != EcsRelationOptional) {
            special_count++;
        }
    }
    const bool has_relation_meta = special_count || desc->order.relation;

    const ecs_component_t excludes[] = { ecs_id(Disabled), ecs_id(Abstract) };
    uint16_t exclude_count = 0;
    for (uint16_t i = 0; i < 2; i++) {
        exclude_count += excludes[i] &&
                         !ecs_query_has_component(desc->terms, explicit_count, excludes[i]);
    }
    query->term_count = explicit_count + exclude_count + translated_count;
    size_t bytes = (size_t)query->term_count * sizeof(ecs_query_term_t);
    if (has_relation_meta) {
        bytes = (bytes + _Alignof(ecs_query_relation_meta_t) - 1) &
                ~(size_t)(_Alignof(ecs_query_relation_meta_t) - 1);
        bytes += sizeof(ecs_query_relation_meta_t) +
                 (size_t)special_count * sizeof(ecs_query_relation_term_t);
    }
    if (*terms_capacity < bytes) {
        query->terms = realloc(query->terms, bytes);
        *terms_capacity = (uint16_t)bytes;
    }
    if (explicit_count) {
        memcpy(query->terms, desc->terms, (size_t)explicit_count * sizeof(ecs_query_term_t));
    }
    uint16_t out = explicit_count;
    for (uint16_t i = 0; i < 2; i++) {
        if (excludes[i] && !ecs_query_has_component(desc->terms, explicit_count, excludes[i])) {
            query->terms[out++] = (ecs_query_term_t){ excludes[i], EcsNot };
        }
    }
    for (uint16_t i = 0; i < relation_count; i++) {
        ecs_query_relation_term_t term = desc->relations[i];
        const ecs_relation_record_t *record = ecs_relation_record(term.id);
        if (record->storage != EcsRelationByTarget &&
            (term.kind == EcsRelationRequired || term.kind == EcsRelationExcluded)) {
            query->terms[out++] = (ecs_query_term_t){
                record->component,
                term.kind == EcsRelationRequired ? EcsFilter : EcsNot,
            };
        }
    }
#ifndef NDEBUG
    ecs_query_validate_terms(query->terms, query->term_count);
#endif

    query->is_a = desc->is_a;
    query->field_count = 0;
    query->field_mask = 0;

    if (ECS_LIKELY(!has_relation_meta && !has_up)) {
        query->up_mask = 0;
        for (uint16_t i = 0; i < query->term_count; i++) {
            if (ecs_query_term_is_field(query->terms[i])) {
                query->field_mask |= (uint16_t)(1u << i);
                query->field_count++;
            }
        }

        query->bloom = 0;
        for (uint16_t i = 0; i < query->term_count; i++) {
            if (ecs_query_term_is_positive(query->terms[i])) {
                const ecs_component_t component = query->terms[i].id;
                query->bloom |= 1ull << (component % 64);
            }
        }
        return ecs_query_rarest_positive_term(query);
    }

    ecs_query_relation_term_t *relations = NULL;
    ecs_query_relation_meta_t *meta = NULL;
    if (has_relation_meta) {
        query->up_mask = ECS_QUERY_HAS_RELATIONS;
        meta = ecs_query_relation_meta(query);
        *meta = (ecs_query_relation_meta_t){
            .relation_count = special_count,
            .cascade = desc->order.relation,
        };
        relations = ecs_query_relations(query);
    } else {
        query->up_mask = 0;
    }
    if (special_count) {
        uint16_t out = 0;
        for (uint16_t i = 0; i < relation_count; i++) {
            ecs_query_relation_term_t term = desc->relations[i];
            const ecs_relation_record_t *record = ecs_relation_record(term.id);
            if ((record->storage == EcsRelationByTarget ||
                 (term.kind != EcsRelationRequired && term.kind != EcsRelationExcluded)) &&
                term.kind != EcsRelationOptional) {
                relations[out++] = term;
            }
        }
    }
#ifndef NDEBUG
    ecs_query_validate_relations(desc->relations, relation_count);
#endif
    if (desc->order.relation) {
        ecs_assert(
            ecs_relation_record(desc->order.relation)->storage == EcsRelationByDepth,
            "ecs_cascade requires ByDepth\n"
        );
    }

    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_term_access_t access = ecs_query_term_access(query->terms[i]);
        if (ecs_query_term_is_field(query->terms[i])) {
            query->field_mask |= (uint16_t)(1u << i);
            query->field_count++;
        }
        if (access == EcsInUp || access == EcsInUpOptional) {
            query->up_mask |= (uint16_t)(1u << i);
#ifndef NDEBUG
            ecs_relation_id_t source_relation =
                ecs_query_term_source_relation(query->terms[i]);
            const ecs_relation_record_t *record =
                ecs_relation_record(source_relation);
#endif
            ecs_assert(
                record->storage == EcsRelationByTarget && record->acyclic,
                "ecs_up requires acyclic ByTarget\n"
            );
        }
    }

    query->bloom = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            const ecs_component_t component = query->terms[i].id;
            query->bloom |= 1ull << (component % 64);
        }
    }
    return ecs_query_rarest_positive_term(query);
}

ecs_component_t ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query) {
    uint16_t capacity = 0;
    query->terms = NULL;
    return ecs_query_build(desc, query, &capacity);
}

static bool ecs_query_match_up_component_table(
    const ecs_query_t *query,
    const ecs_table_t *table
) {
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) {
        return false;
    }
    if (query->is_a && !ecs_table_is_a(table, query->is_a)) {
        return false;
    }
    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_query_term_t term = query->terms[i];
        ecs_term_access_t access = ecs_query_term_access(term);
        if (access == EcsInOptional || access == EcsInOutOptional || access == EcsInUp ||
            access == EcsInUpOptional) {
            continue;
        }
        if (access == EcsNot) {
            if (ecs_table_has(table, term.id)) {
                return false;
            }
        } else if (access == EcsOut || access == EcsInOut || access == EcsInOutOptional) {
            if (ecs_table_column_or_invalid(table, term.id) == UINT16_MAX) {
                return false;
            }
        } else if (!ecs_table_has(table, term.id)) {
            return false;
        }
    }
    return true;
}

bool ecs_query_match_table(const ecs_query_t *query, const ecs_table_t *table) {
    const uint16_t up_mask = query->up_mask & ECS_QUERY_UP_MASK;
    if (!(up_mask ? ecs_query_match_up_component_table(query, table)
                  : ecs_query_match_component_table(query, table))) {
        return false;
    }
    if (!(query->up_mask & ECS_QUERY_HAS_RELATIONS)) {
        return true;
    }
    const ecs_query_relation_meta_t *meta = ecs_query_relation_meta(query);
    const ecs_query_relation_term_t *relations = ecs_query_relations(query);
    for (uint16_t i = 0; i < meta->relation_count; i++) {
        ecs_query_relation_term_t term = relations[i];
        uint16_t relation_index = ecs_type_relation_index(&table->type, term.id);
        if (term.kind == EcsRelationExcluded) {
            if (relation_index != UINT16_MAX) {
                return false;
            }
            continue;
        }
        if (term.kind == EcsRelationRequired) {
            if (relation_index == UINT16_MAX) {
                return false;
            }
            continue;
        }
        if (relation_index == UINT16_MAX ||
            ecs_relation_key_target(&ecs_type_relations(&table->type)[relation_index]) !=
                term.target) {
            return false;
        }
        if (term.kind == EcsRelationTarget) {
            continue;
        }
        ecs_entity_t value = ecs_relation_table_depth(table, term.id);
        if (value != term.target) {
            return false;
        }
    }
    return true;
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
        const uint16_t term_index = (uint16_t)__builtin_ctz((unsigned)remaining_fields);
        remaining_fields &= (uint16_t)(remaining_fields - 1);
        const ecs_query_term_t term = cache->query.terms[term_index];
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
            field_kind != EcsFieldNone || access == EcsInOptional ||
                access == EcsInOutOptional || access == EcsInUp ||
                access == EcsInUpOptional,
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
        uint16_t term_index = (uint16_t)__builtin_ctz((unsigned)remaining_fields);
        remaining_fields &= (uint16_t)(remaining_fields - 1);
        ecs_query_term_t term = cache->query.terms[term_index];
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
    cache->fields_ptr = realloc(
        cache->fields_ptr,
        sizeof(void *) * (uint32_t)capacity * cache->query.field_count
    );
    cache->field_kind_bits = realloc(cache->field_kind_bits, sizeof(uint32_t) * capacity);
    cache->field_table_capacity = capacity;
}

static void
ecs_query_cache_append_table(ecs_query_cache_t *cache, const ecs_table_t *table, uint16_t table_id) {
    sicore_vec_push_u16(&cache->table_ids, table_id);
    const uint16_t table_count = cache->table_ids.size;
    const uint16_t field_count = cache->query.field_count;
    ecs_query_cache_reserve_fields(cache, table_count);

    if (field_count) {
        ecs_query_cache_set_table_fields(cache, table, table_count - 1);
    }
}

static void ecs_query_cache_insert_cascade(
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
    const ecs_relation_id_t cascade = ecs_query_relation_meta(&cache->query)->cascade;
    uint32_t depth = ecs_relation_table_depth(table, cascade);
    uint16_t *ids = cache->table_ids.data;
    while (insert &&
           ecs_relation_table_depth(ecs_get_table(ids[insert - 1]), cascade) > depth) {
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

    if (field_count != 0) {
        ecs_query_cache_set_table_fields(cache, table, insert);
    }
}

static inline void ecs_query_cache_add_relation_table(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_id
) {
    if ((cache->query.up_mask & ECS_QUERY_HAS_RELATIONS) &&
        ecs_query_relation_meta(&cache->query)->cascade) {
        ecs_query_cache_insert_cascade(cache, table, table_id);
    } else {
        ecs_query_cache_append_table(cache, table, table_id);
    }
}

static void ecs_query_index_update_matches(
    ecs_query_cache_t *query_cache,
    ecs_component_t component
);

ecs_query_id_t ecs_query_index_create(const ecs_query_desc_t *desc) {
    ecs_query_index_t *index = &ecs_world.query_index;
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
        query_cache->query.terms = NULL;
        query_cache->terms_capacity = 0;
    }

    const ecs_component_t match_component =
        ecs_query_build(desc, &query_cache->query, &query_cache->terms_capacity);
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

static void ecs_query_index_update_matches(
    ecs_query_cache_t *query_cache,
    ecs_component_t component
) {
    if (ECS_LIKELY(query_cache->query.up_mask == 0)) {
        if (ECS_LIKELY(component)) {
            const sicore_vec_t *tables_vec = &ecs_component_index_get(component)->tables;
            sicore_vec_iter(tables_vec, uint16_t, table_index, {
                const ecs_table_t *table = &ecs_world.table_index.tables[*table_index];

                if (ecs_query_match_component_table(&query_cache->query, table)) {
                    ecs_query_cache_append_table(query_cache, table, *table_index);
                }
            });
        } else {
            const uint16_t table_count = ecs_world.table_index.table_count;
            const ecs_table_t *tables = ecs_world.table_index.tables;
            for (uint16_t i = 0; i < table_count; i++) {
                if (ecs_query_match_component_table(&query_cache->query, &tables[i])) {
                    ecs_query_cache_append_table(query_cache, &tables[i], i);
                }
            }
        }
        return;
    }

    if (query_cache->query.up_mask & ECS_QUERY_HAS_RELATIONS) {
        const ecs_query_relation_meta_t *meta = ecs_query_relation_meta(&query_cache->query);
        const ecs_query_relation_term_t *relations = ecs_query_relations(&query_cache->query);
        for (uint16_t i = 0; i < meta->relation_count; i++) {
            ecs_query_relation_term_t term = relations[i];
            if (term.kind == EcsRelationTarget) {
                ecs_relation_tables_t tables =
                    ecs_table_index_relation_tables(term.id, term.target);
                if (!tables.count) {
                    return;
                }
                for (uint16_t t = 0; t < tables.count; t++) {
                    const ecs_table_t *table = ecs_get_table(tables.ids[t]);
                    if (ecs_query_match_table(&query_cache->query, table)) {
                        ecs_query_cache_add_relation_table(query_cache, table, tables.ids[t]);
                    }
                }
                return;
            }
        }
    }
    if (ECS_LIKELY(component)) {
        const sicore_vec_t *tables_vec = &ecs_component_index_get(component)->tables;
        sicore_vec_iter(tables_vec, uint16_t, table_index, {
            const ecs_table_t *table = &ecs_world.table_index.tables[*table_index];

            if (ecs_query_match_table(&query_cache->query, table)) {
                ecs_query_cache_add_relation_table(query_cache, table, *table_index);
            }
        });
    } else {
        const uint16_t table_count = ecs_world.table_index.table_count;
        const ecs_table_t *tables = ecs_world.table_index.tables;
        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(&query_cache->query, &tables[i])) {
                ecs_query_cache_add_relation_table(query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = ecs_world.query_index.active_ids.data;
    for (uint32_t i = 0; i < ecs_world.query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&ecs_world.query_index.queries, active_ids[i], ecs_query_cache_t);
        if (ecs_query_match_table(&cache->query, table)) {
            ecs_query_cache_add_relation_table(cache, table, table_id);
        }
    }
}

void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = ecs_world.query_index.active_ids.data;

    for (uint32_t i = 0; i < ecs_world.query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&ecs_world.query_index.queries, active_ids[i], ecs_query_cache_t);
        if (cache->query.field_count == 0) {
            continue;
        }

        const uint16_t *table_ids = cache->table_ids.data;
        for (uint16_t table_index = 0; table_index < cache->table_ids.size; table_index++) {
            if (table_ids[table_index] == table_id) {
                ecs_query_cache_set_table_fields(cache, table, table_index);
                break;
            }
        }
    }
}
