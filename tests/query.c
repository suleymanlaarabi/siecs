#include "ecs/storage/query_index.h"
#include "ecs/table.h"
#include "ecs/world.h"
#include "ecs/world_internal.h"
#include <criterion/criterion.h>
#include <stdint.h>

typedef ecs_world_t ecs_world_t_internal;

ECS_COMPONENT_DECLARE(Pos, { float x, y; });
ECS_COMPONENT_DECLARE(Vel, { float x, y; });
ECS_COMPONENT_DECLARE(Acc, { float x, y; });

ECS_COMPONENT_DEFINE(Pos);
ECS_COMPONENT_DEFINE(Vel);
ECS_COMPONENT_DEFINE(Acc);

Test(query, bloom_filter_subset_match) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Pos);
    ECS_COMPONENT_REGISTER(world, Vel);
    ECS_COMPONENT_REGISTER(world, Acc);

    // Entity 1: Pos, Vel
    ecs_entity_t e1 = ecs_new(world);
    ecs_add(world, e1, Pos);
    ecs_add(world, e1, Vel);

    ecs_entity_t e2 = ecs_new(world);
    ecs_add(world, e2, Pos);
    ecs_add(world, e2, Vel);
    ecs_add(world, e2, Acc);

    uint32_t q = ecs_query(
        world,
        {
            .read = { ecs_id(Pos), ecs_id(Vel) },
            .required = { ecs_id(Pos), ecs_id(Vel) },
        }
    );

    ecs_world_t_internal *wi = (ecs_world_t_internal *)world;
    ecs_query_index_t *qi = &wi->query_index;

    ecs_query_cache_t *cache = ecs_vec_get_mut(&qi->queries, q, ecs_query_cache_t);
    ecs_vec_clear(&cache->table_ids);
    ecs_vec_clear(&cache->fields);

    ecs_query_index_update_matches(qi, wi->table_index.tables, wi->table_index.table_count, q);

    cache = ecs_vec_get_mut(&qi->queries, q, ecs_query_cache_t);

    cr_assert_eq(cache->table_ids.size, 2, "Query should match 2 tables (Pos+Vel and Pos+Vel+Acc)");

    ecs_fini(world);
}

Test(query, iter_count) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Pos);
    ECS_COMPONENT_REGISTER(world, Vel);

    for (int i = 0; i < 3; i++) {
        ecs_entity_t e = ecs_new(world);
        ecs_add(world, e, Pos);
        ecs_add(world, e, Vel);
    }
    ecs_entity_t lone = ecs_new(world);
    ecs_add(world, lone, Pos);

    uint32_t q = ecs_query(
        world,
        {
            .read = { ecs_id(Pos), ecs_id(Vel) },
            .required = { ecs_id(Pos), ecs_id(Vel) },
        }
    );

    uint32_t count = 0;
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_iter_next(&it)) {
        count += ecs_iter_table(&it)->entity_count;
    }

    cr_assert_eq(count, 3u, "Should iterate exactly 3 entities with Pos+Vel");
    ecs_fini(world);
}

Test(query, iter_data_write) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Pos);
    ECS_COMPONENT_REGISTER(world, Vel);

    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, Pos);
    ecs_add(world, e, Vel);

    Pos *p = ecs_get(world, e, Pos);
    p->x = 1.0f;
    p->y = 2.0f;

    uint32_t q = ecs_query(
        world,
        {
            .read = { ecs_id(Pos), ecs_id(Vel) },
            .required = { ecs_id(Pos), ecs_id(Vel), 0 },
        }
    );

    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_iter_next(&it)) {
        Pos *positions = ecs_field(&it, 0);
        uint32_t n = ecs_iter_table(&it)->entity_count;
        for (uint32_t i = 0; i < n; i++) {
            positions[i].x += 10.0f;
        }
    }

    Pos *result = ecs_get(world, e, Pos);
    cr_assert_float_eq(result->x, 11.0f, 1e-6f, "ecs_field write should be visible via ecs_get");
    ecs_fini(world);
}

Test(query, cache_auto_update) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Pos);
    ECS_COMPONENT_REGISTER(world, Vel);

    uint32_t q = ecs_query(
        world,
        {
            .read = { ecs_id(Pos), ecs_id(Vel) },
            .required = { ecs_id(Pos), ecs_id(Vel), 0 },
        }
    );

    ecs_entity_t e1 = ecs_new(world);
    ecs_add(world, e1, Pos);
    ecs_add(world, e1, Vel);

    ecs_entity_t e2 = ecs_new(world);
    ecs_add(world, e2, Pos);
    ecs_add(world, e2, Vel);

    uint32_t count = 0;
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_iter_next(&it)) {
        count += ecs_iter_table(&it)->entity_count;
    }

    cr_assert_eq(count, 2u, "Tables created after query init must appear in iteration");
    ecs_fini(world);
}

Test(query, iter_excluded) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Pos);
    ECS_COMPONENT_REGISTER(world, Vel);
    ECS_COMPONENT_REGISTER(world, Acc);

    for (int i = 0; i < 2; i++) {
        ecs_entity_t e = ecs_new(world);
        ecs_add(world, e, Pos);
        ecs_add(world, e, Vel);
    }
    ecs_entity_t e3 = ecs_new(world);
    ecs_add(world, e3, Pos);
    ecs_add(world, e3, Vel);
    ecs_add(world, e3, Acc);

    uint32_t q = ecs_query(
        world,
        {
            .read = { ecs_id(Pos), ecs_id(Vel) },
            .required = { ecs_id(Pos), ecs_id(Vel) },
            .excluded = { ecs_id(Acc) },
        }
    );

    uint32_t count = 0;
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_iter_next(&it)) {
        count += ecs_iter_table(&it)->entity_count;
    }

    cr_assert_eq(count, 2u, "Entity with excluded component must not appear in iteration");
    ecs_fini(world);
}
