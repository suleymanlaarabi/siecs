#include "query_index.h"
#include "../compiler.h"
#include "../datastructure/vec.h"
#include "../table.h"
#include "../utils.h"
#include "../world_internal.h"
#include "component_index.h"
#include "siecs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ecs_query_index_init(ecs_query_index_t *index) {
    ecs_vec_init(&index->queries, sizeof(ecs_query_cache_t));
    ecs_vec_init(&index->active_ids, sizeof(ecs_query_id_t));
    index->first_free = UINT16_MAX;
}

void ecs_query_index_fini(ecs_query_index_t *index) {
    const ecs_query_id_t *active_ids = index->active_ids.data;
    for (uint32_t i = 0; i < index->active_ids.size; i++) {
        ecs_query_cache_t *cache =
            ecs_vec_get_mut(&index->queries, active_ids[i], ecs_query_cache_t);
        ecs_vec_fini(&cache->table_ids);
        free(cache->fields_ptr);
        free(cache->fields_kind);
        ecs_query_index_destroy(&cache->query);
    }
    ecs_vec_fini(&index->active_ids);
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
ecs_query_copy_terms_with_implicit_excludes(const ecs_query_term_t *terms, uint16_t *count) {
    const ecs_component_t excludes[] = {
        ecs_id(Disabled),
        ecs_id(Abstract),
    };
    const uint16_t explicit_count = *count;
    uint16_t exclude_count = 0;

    for (uint32_t i = 0; i < sizeof(excludes) / sizeof(excludes[0]); i++) {
        if (excludes[i] && !ecs_query_has_term(terms, explicit_count, excludes[i])) {
            exclude_count++;
        }
    }

    if (exclude_count == 0) {
        return ecs_query_copy_terms(terms, *count);
    }

    ecs_assert(*count + exclude_count < 16, "query has no room for implicit exclude terms\n");

    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * (*count + exclude_count));
    if (*count != 0) {
        memcpy(copy, terms, sizeof(ecs_query_term_t) * *count);
    }

    for (uint32_t i = 0; i < sizeof(excludes) / sizeof(excludes[0]); i++) {
        if (excludes[i] && !ecs_query_has_term(terms, explicit_count, excludes[i])) {
            copy[*count] = (ecs_query_term_t){ .id = excludes[i], .access = EcsNot };
            *count += 1;
        }
    }

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
            terms[i].access == EcsIn || terms[i].access == EcsOut || terms[i].access == EcsInOut ||
                terms[i].access == EcsInOptional || terms[i].access == EcsInOutOptional ||
                terms[i].access == EcsFilter || terms[i].access == EcsNot,
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

    query->terms = ecs_query_copy_terms_with_implicit_excludes(desc->terms, &query->term_count);
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

static void ecs_query_cache_add_table(
    ecs_world_t *world,
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_id
) {
    ecs_vec_push_u16(&cache->table_ids, table_id);
    const uint16_t table_count = cache->table_ids.size;
    const uint16_t field_count = cache->query.field_count;

    if (table_count > cache->field_table_capacity) {
        uint16_t capacity = cache->field_table_capacity ? cache->field_table_capacity : 4;
        while (capacity < table_count) {
            capacity *= 2;
        }

        const uint32_t slot_count = (uint32_t)capacity * field_count;
        cache->fields_ptr = realloc(cache->fields_ptr, sizeof(void *) * slot_count);
        cache->fields_kind = realloc(cache->fields_kind, sizeof(ecs_field_kind_t) * slot_count);
        cache->field_table_capacity = capacity;
    }

    const uint32_t base = (uint32_t)(table_count - 1) * field_count;
    for (uint16_t i = 0; i < cache->query.field_count; i++) {
        const ecs_query_term_t term = cache->query.fields[i];
        void *field = NULL;
        ecs_field_kind_t field_kind = EcsFieldNone;

        if (ecs_query_term_requires_owned(term)) {
            uint16_t column = ecs_table_column_or_invalid(table, term.id);
            if (column != UINT16_MAX) {
                field = &table->cls[column].data;
                field_kind = EcsFieldOwned;
            }
        } else {
            bool is_shared = false;
            field = ecs_table_field(world, table, term.id, &is_shared);
            if (field || is_shared) {
                field_kind = is_shared ? EcsFieldShared : EcsFieldOwned;
            }
        }

        ecs_assert(
            field_kind != EcsFieldNone || term.access == EcsInOptional ||
                term.access == EcsInOutOptional,
            "query cache matched table without field component: %d\n",
            term.id
        );

        cache->fields_ptr[base + i] = field;
        cache->fields_kind[base + i] = field_kind;
    }
}

ecs_query_id_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc) {
    ecs_query_id_t id;
    ecs_query_cache_t *query_cache;

    if (index->first_free != UINT16_MAX) {
        id = index->first_free;
        query_cache = ecs_vec_get_mut(&index->queries, id, ecs_query_cache_t);
        index->first_free = query_cache->next_free;
    } else {
        query_cache = ecs_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
        id = index->queries.size - 1;
    }

    ecs_query_from_desc(desc, &query_cache->query);
    ecs_vec_init(&query_cache->table_ids, sizeof(uint16_t));
    query_cache->fields_ptr = NULL;
    query_cache->fields_kind = NULL;
    query_cache->field_table_capacity = 0;
    query_cache->active_index = index->active_ids.size;
    query_cache->next_free = UINT16_MAX;
    query_cache->alive = true;
    ecs_vec_push_u16(&index->active_ids, id);

    return id;
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

            if (ecs_query_match_table(world, &query_cache->query, table)) {
                ecs_query_cache_add_table(world, query_cache, table, *table_index);
            }
        });
    } else {
        const uint16_t table_count = world->table_index.table_count;
        const ecs_table_t *tables = world->table_index.tables;

        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(world, &query_cache->query, &tables[i])) {
                ecs_query_cache_add_table(world, query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(ecs_world_t *world, const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = world->query_index.active_ids.data;
    for (uint32_t i = 0; i < world->query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            ecs_vec_get_mut(&world->query_index.queries, active_ids[i], ecs_query_cache_t);
        if (ecs_query_match_table(world, &cache->query, table)) {
            ecs_query_cache_add_table(world, cache, table, table_id);
        }
    }
}
