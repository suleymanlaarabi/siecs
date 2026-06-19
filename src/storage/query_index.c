#include "query_index.h"
#include "../compiler.h"
#include "../datastructure/vec.h"
#include "../table.h"
#include "../utils.h"
#include "../world_internal.h"
#include "component_index.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void *ecs_query_null_field = NULL;

void ecs_query_index_init(ecs_query_index_t *index) {
    ecs_vec_init(&index->queries, sizeof(ecs_query_cache_t));
}

void ecs_query_index_fini(ecs_query_index_t *index) {
    for (uint32_t i = 0; i < index->queries.size; i++) {
        ecs_query_cache_t *cache = ecs_vec_get_mut(&index->queries, i, ecs_query_cache_t);
        ecs_vec_fini(&cache->table_ids);
        ecs_vec_fini(&cache->fields);
        ecs_query_index_destroy(&cache->query);
    }
    ecs_vec_fini(&index->queries);
}

void ecs_query_index_destroy(ecs_query_t *query) {
    free(query->terms);
    free(query->fields);
}

static uint16_t ecs_query_count_terms(const ecs_query_term_t *terms) {
    uint16_t i = 0;
    while (terms[i].id) {
        i++;
    }
    return i;
}

static ecs_query_term_t *ecs_query_copy_terms(const ecs_query_term_t *terms, uint16_t count) {
    if (count == 0) {
        return NULL;
    }
    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * count);
    memcpy(copy, terms, sizeof(ecs_query_term_t) * count);
    return copy;
}

static bool
ecs_query_has_term(const ecs_query_term_t *terms, uint16_t term_count, ecs_component_t id) {
    for (uint16_t i = 0; i < term_count; i++) {
        if (terms[i].id == id) {
            return true;
        }
    }
    return false;
}

static ecs_query_term_t *
ecs_query_copy_terms_with_implicit_disabled(const ecs_query_term_t *terms, uint16_t *count) {
    const ecs_component_t disabled = ecs_id(Disabled);

    if (!disabled || ecs_query_has_term(terms, *count, disabled)) {
        return ecs_query_copy_terms(terms, *count);
    }

    const uint16_t query_term_capacity = 16;
    ecs_assert(
        *count + 1 < query_term_capacity,
        "query has no room for implicit Disabled term\n"
    );

    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * (*count + 1));
    if (*count != 0) {
        memcpy(copy, terms, sizeof(ecs_query_term_t) * *count);
    }
    copy[*count] = (ecs_query_term_t){ .id = disabled, .access = EcsNot };
    *count += 1;

    return copy;
}

static bool ecs_query_term_is_field(ecs_query_term_t term) {
    return term.access == EcsIn || term.access == EcsOut || term.access == EcsInOut ||
           term.access == EcsInOptional || term.access == EcsInOutOptional;
}

static bool ecs_query_term_is_positive(ecs_query_term_t term) {
    return term.access == EcsIn || term.access == EcsOut || term.access == EcsInOut ||
           term.access == EcsFilter;
}

static void ecs_query_validate_terms(const ecs_query_term_t *terms, uint16_t term_count) {
    for (uint16_t i = 0; i < term_count; i++) {
        ecs_assert_id_valid(terms[i].id);
        ecs_assert(
            terms[i].access == EcsIn || terms[i].access == EcsOut ||
                terms[i].access == EcsInOut || terms[i].access == EcsInOptional ||
                terms[i].access == EcsInOutOptional || terms[i].access == EcsFilter ||
                terms[i].access == EcsNot,
            "invalid query term access: %d\n",
            terms[i].access
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

void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query) {
    query->term_count = ecs_query_count_terms(desc->terms);
    ecs_assert(query->term_count != 0, "query must contain at least one term\n");

    query->terms = ecs_query_copy_terms_with_implicit_disabled(desc->terms, &query->term_count);
    ecs_query_validate_terms(query->terms, query->term_count);

    query->field_count = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_field(query->terms[i])) {
            query->field_count++;
        }
    }

    query->fields = NULL;
    if (query->field_count != 0) {
        query->fields = malloc(sizeof(ecs_query_term_t) * query->field_count);

        uint16_t field = 0;
        for (uint16_t i = 0; i < query->term_count; i++) {
            if (ecs_query_term_is_field(query->terms[i])) {
                query->fields[field++] = query->terms[i];
            }
        }
    }

    query->bloom = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            query->bloom |= 1ull << (query->terms[i].id % 64);
        }
    }
}

static void
ecs_query_cache_add_table(ecs_query_cache_t *cache, const ecs_table_t *table, uint16_t table_id) {
    ecs_vec_push_u16(&cache->table_ids, table_id);
    for (uint16_t i = 0; i < cache->query.field_count; i++) {
        const ecs_query_term_t term = cache->query.fields[i];
        uint16_t col = ecs_table_get_column_index(table, term.id);
        const bool has_field = col < table->type.count && table->type.ids[col] == term.id;
        const bool optional = term.access == EcsInOptional || term.access == EcsInOutOptional;

        ecs_assert(
            has_field || optional,
            "query cache matched table without field component: %d\n",
            term.id
        );

        void **slot = has_field ? &table->cls[col].data : &ecs_query_null_field;
        ecs_vec_push(&cache->fields, &slot, sizeof(void **));
    }
}

ecs_query_id_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc) {
    ecs_query_cache_t *query_cache = ecs_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
    ecs_query_from_desc(desc, &query_cache->query);
    ecs_vec_init(&query_cache->table_ids, sizeof(uint16_t));
    ecs_vec_init(&query_cache->fields, sizeof(void **));

    return index->queries.size - 1;
}

static ecs_component_t ecs_query_first_positive_term(const ecs_query_t *query) {
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            return query->terms[i].id;
        }
    }
    return 0;
}

void ecs_query_index_update_matches(ecs_world_t *world, ecs_query_cache_t *query_cache) {
    uint16_t component = ecs_query_first_positive_term(&query_cache->query);

    if (ECS_LIKELY(component)) {
        const ecs_vec_t *tables_vec =
            &ecs_component_index_get(&world->component_index, component)->tables;

        ecs_vec_iter(tables_vec, uint16_t, table_index, {
            const ecs_table_t *table = &world->table_index.tables[*table_index];

            if (ecs_query_match_table(&query_cache->query, table)) {
                ecs_query_cache_add_table(query_cache, table, *table_index);
            }
        });
    } else {
        const uint16_t table_count = world->table_index.table_count;
        const ecs_table_t *tables = world->table_index.tables;

        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(&query_cache->query, &tables[i])) {
                ecs_query_cache_add_table(query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(
    ecs_query_index_t *index,
    const ecs_table_t *table,
    uint16_t table_id
) {
    for (uint32_t i = 0; i < index->queries.size; i++) {
        ecs_query_cache_t *cache = ecs_vec_get_mut(&index->queries, i, ecs_query_cache_t);
        if (ecs_query_match_table(&cache->query, table)) {
            ecs_query_cache_add_table(cache, table, table_id);
        }
    }
}
