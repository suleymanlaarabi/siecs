#include "../ecs/table.h"
#include "../ecs/storage/table_index.h"
#include "../ecs/world.h"
#include "bench.h"
#include "ecs/world_internal.h"
#include <stdio.h>
#include <stdlib.h>

ECS_COMPONENT_DECLARE(Position, { float x, y, z; });                        /* 3  */
ECS_COMPONENT_DECLARE(Velocity, { float x, y, z; });                        /* 3  */
ECS_COMPONENT_DECLARE(Rotation, { float x, y, z, w; });                     /* 4  */
ECS_COMPONENT_DECLARE(Scale, { float x, y, z; });                           /* 3  */
ECS_COMPONENT_DECLARE(Color, { float r, g, b, a; });                        /* 4  */
ECS_COMPONENT_DECLARE(Health, { float hp, max_hp, regen; });                /* 3  */
ECS_COMPONENT_DECLARE(Stats, { float atk, def, spd, mass; });               /* 4 */
ECS_COMPONENT_DECLARE(Meta, { int active, layer, tag, flags; });            /* 4 */
ECS_COMPONENT_DECLARE(Camera, { float near, far, fov, aspect; });           /* 4 */
ECS_COMPONENT_DECLARE(Light, { float lx, ly, lz, lr, lg, lb, intensity; }); /* 7 */

ECS_COMPONENT_DEFINE(Position);
ECS_COMPONENT_DEFINE(Velocity);
ECS_COMPONENT_DEFINE(Rotation);
ECS_COMPONENT_DEFINE(Scale);
ECS_COMPONENT_DEFINE(Color);
ECS_COMPONENT_DEFINE(Health);
ECS_COMPONENT_DEFINE(Stats);
ECS_COMPONENT_DEFINE(Meta);
ECS_COMPONENT_DEFINE(Camera);
ECS_COMPONENT_DEFINE(Light);

#define N_ENTITIES 100000
#define QUERY_ITER_REPS 10000
#define QUERY_CREATE_TABLES 2200
#define QUERY_CREATE_COMPONENT_POOL 2048
#define QUERY_CREATE_COMPONENTS_PER_TABLE 24
#define QUERY_CREATE_TERMS 7

static ecs_world_t *g_world;
static ecs_entity_t *g_entities;
static ecs_query_id_t g_query;
static ecs_component_t g_query_create_terms[QUERY_CREATE_TERMS];
static uint64_t g_observer_count;

static void register_components(ecs_world_t *w) {
    ECS_COMPONENT_REGISTER(w, Position);
    ECS_COMPONENT_REGISTER(w, Velocity);
    ECS_COMPONENT_REGISTER(w, Rotation);
    ECS_COMPONENT_REGISTER(w, Scale);
    ECS_COMPONENT_REGISTER(w, Color);
    ECS_COMPONENT_REGISTER(w, Health);
    ECS_COMPONENT_REGISTER(w, Stats);
    ECS_COMPONENT_REGISTER(w, Meta);
    ECS_COMPONENT_REGISTER(w, Camera);
    ECS_COMPONENT_REGISTER(w, Light);
}

static void setup_create(void *ctx) {
    (void)ctx;
    g_world = ecs_init();
    register_components(g_world);
}

static void bench_create(void *ctx) {
    (void)ctx;
    for (int i = 0; i < N_ENTITIES; i++)
        ecs_new(g_world);
}

static void teardown_create(void *ctx) {
    (void)ctx;
    ecs_fini(g_world);
    g_world = NULL;
}

static void setup_add(void *ctx) {
    (void)ctx;
    g_world = ecs_init();
    register_components(g_world);
}

static void bench_add_components(void *ctx) {
    (void)ctx;
    for (int i = 0; i < N_ENTITIES; i++) {
        ecs_entity_t e = ecs_new(g_world);
        ecs_add(g_world, e, Position);
        ecs_add(g_world, e, Velocity);
        ecs_add(g_world, e, Rotation);
        ecs_add(g_world, e, Scale);
        ecs_add(g_world, e, Color);
        ecs_add(g_world, e, Health);
        ecs_add(g_world, e, Stats);
        ecs_add(g_world, e, Meta);
        ecs_add(g_world, e, Camera);
        ecs_add(g_world, e, Light);
    }
}

static void teardown_add(void *ctx) {
    (void)ctx;
    ecs_fini(g_world);
    g_world = NULL;
}

static void setup_add_remove(void *ctx) {
    (void)ctx;
    g_world = ecs_init();
    g_entities = malloc(N_ENTITIES * sizeof(ecs_entity_t));
    register_components(g_world);
    for (int i = 0; i < N_ENTITIES; i++) {
        g_entities[i] = ecs_new(g_world);
    }
}

static void bench_add_remove(void *ctx) {
    (void)ctx;
    for (int i = 0; i < N_ENTITIES; i++) {
        ecs_add(g_world, g_entities[i], Position);
        ecs_remove(g_world, g_entities[i], Position);
    }
}

static void teardown_add_remove(void *ctx) {
    (void)ctx;
    free(g_entities);
    g_entities = NULL;
    ecs_fini(g_world);
    g_world = NULL;
}

static void setup_kill(void *ctx) {
    (void)ctx;
    g_world = ecs_init();
    g_entities = malloc(N_ENTITIES * sizeof(ecs_entity_t));
    register_components(g_world);
    for (int i = 0; i < N_ENTITIES; i++) {
        ecs_entity_t e = ecs_new(g_world);
        ecs_add(g_world, e, Position);
        ecs_add(g_world, e, Velocity);
        ecs_add(g_world, e, Health);
        g_entities[i] = e;
    }
}

static void bench_kill(void *ctx) {
    (void)ctx;
    for (int i = 0; i < N_ENTITIES; i++)
        ecs_kill(g_world, g_entities[i]);
}

static void teardown_kill(void *ctx) {
    (void)ctx;
    free(g_entities);
    g_entities = NULL;
    ecs_fini(g_world);
    g_world = NULL;
}

static void setup_has(void *ctx) {
    (void)ctx;
    g_world = ecs_init();
    g_entities = malloc(N_ENTITIES * sizeof(ecs_entity_t));
    register_components(g_world);
    for (int i = 0; i < N_ENTITIES; i++) {
        ecs_entity_t e = ecs_new(g_world);
        ecs_add(g_world, e, Position);
        ecs_add(g_world, e, Health);
        g_entities[i] = e;
    }
}

static void bench_has(void *ctx) {
    (void)ctx;
    volatile int sink = 0;
    for (int i = 0; i < N_ENTITIES; i++)
        sink += ecs_has(g_world, g_entities[i], Position);
    (void)sink;
}

static void teardown_has(void *ctx) {
    (void)ctx;
    free(g_entities);
    g_entities = NULL;
    ecs_fini(g_world);
    g_world = NULL;
}

static void bench_get(void *ctx) {
    (void)ctx;
    volatile float sink = 0.0f;
    for (int i = 0; i < N_ENTITIES; i++) {
        Position *p = ecs_get(g_world, g_entities[i], Position);
        sink += p->x;
    }
    (void)sink;
}

static void setup_query_iter(void *ctx) {
    (void)ctx;
    g_world = ecs_init();
    register_components(g_world);
    for (int i = 0; i < N_ENTITIES; i++) {
        ecs_entity_t e = ecs_new(g_world);
        ecs_add(g_world, e, Position);
        ecs_add(g_world, e, Velocity);
    }
    g_query = ecs_query(
        g_world,
        {
            .read = { ecs_id(Position), ecs_id(Velocity) },
            .required = { ecs_id(Position), ecs_id(Velocity) },
        }
    );
}

static void bench_query_iter(void *ctx) {
    (void)ctx;
    volatile uint64_t sink = 0;
    for (int i = 0; i < QUERY_ITER_REPS; i++) {
        ecs_iter_t it = ecs_query_iter(g_world, g_query);
        while (ecs_iter_next(&it)) {
            ecs_table_t *table = ecs_iter_table(&it);
            sink += table->entity_count;
        }
    }
    (void)sink;
}

static void bench_query_fields(void *ctx) {
    (void)ctx;
    volatile uintptr_t sink = 0;
    for (int i = 0; i < QUERY_ITER_REPS; i++) {
        ecs_iter_t it = ecs_query_iter(g_world, g_query);
        while (ecs_iter_next(&it)) {
            Position *positions = ecs_field(&it, 0);
            Velocity *velocities = ecs_field(&it, 1);
            sink += (uintptr_t)positions + (uintptr_t)velocities;
        }
    }
    (void)sink;
}

static void teardown_query_iter(void *ctx) {
    (void)ctx;
    ecs_fini(g_world);
    g_world = NULL;
}

static int cmp_component_id(const void *a, const void *b) {
    const ecs_component_t lhs = *(const ecs_component_t *)a;
    const ecs_component_t rhs = *(const ecs_component_t *)b;
    return (lhs > rhs) - (lhs < rhs);
}

static void setup_query_create(void *ctx) {
    (void)ctx;
    g_world = ecs_init();
    const uint32_t target_table = QUERY_CREATE_TABLES / 2;
    ecs_component_t components[QUERY_CREATE_COMPONENT_POOL];

    for (uint32_t i = 0; i < QUERY_CREATE_COMPONENT_POOL; i++) {
        components[i] = ecs_component(g_world, { .name = NULL, .size = 8 });
    }

    for (uint32_t table_i = 0; table_i < QUERY_CREATE_TABLES; table_i++) {
        ecs_component_t *ids =
            malloc(sizeof(ecs_component_t) * QUERY_CREATE_COMPONENTS_PER_TABLE);

        for (uint32_t col = 0; col < QUERY_CREATE_COMPONENTS_PER_TABLE; col++) {
            ids[col] = components[(table_i * 37 + col * 11) % QUERY_CREATE_COMPONENT_POOL];
        }

        qsort(ids, QUERY_CREATE_COMPONENTS_PER_TABLE, sizeof(ecs_component_t), cmp_component_id);
        if (table_i == target_table) {
            for (uint32_t i = 0; i < QUERY_CREATE_TERMS; i++) {
                g_query_create_terms[i] = ids[i];
            }
        }

        ecs_table_index_get_or_create(
            g_world,
            (ecs_type_t){
                .ids = ids,
                .count = QUERY_CREATE_COMPONENTS_PER_TABLE,
            }
        );
    }
}

static void bench_query_create(void *ctx) {
    (void)ctx;
    g_query = ecs_query(
        g_world,
        {
            .read = {
                g_query_create_terms[0],
                g_query_create_terms[1],
                g_query_create_terms[2],
                g_query_create_terms[3],
                g_query_create_terms[4],
                g_query_create_terms[5],
                g_query_create_terms[6],
            },
        }
    );
}

static void teardown_query_create(void *ctx) {
    (void)ctx;
    ecs_fini(g_world);
    g_world = NULL;
}

static void on_observer_emit(ecs_observer_event_t *event) {
    (void)event;
    g_observer_count++;
}

static void setup_observer_emit(void *ctx) {
    (void)ctx;
    g_world = ecs_init();
    g_entities = malloc(N_ENTITIES * sizeof(ecs_entity_t));
    register_components(g_world);
    ecs_observer(
        g_world,
        {
            .on = OnSet,
            .query = { .required = { ecs_id(Position) } },
            .callback = on_observer_emit,
        }
    );
    for (int i = 0; i < N_ENTITIES; i++) {
        ecs_entity_t e = ecs_new(g_world);
        ecs_add(g_world, e, Position);
        g_entities[i] = e;
    }
    g_observer_count = 0;
}

static void bench_observer_emit(void *ctx) {
    (void)ctx;
    Position p = { 1.0f, 2.0f, 3.0f };
    for (int i = 0; i < N_ENTITIES; i++) {
        ecs_set_cid(g_world, g_entities[i], ecs_id(Position), &p);
    }
}

static void teardown_observer_emit(void *ctx) {
    (void)ctx;
    free(g_entities);
    g_entities = NULL;
    ecs_fini(g_world);
    g_world = NULL;
}

int main(void) {
    printf("ECS benchmark — %d entities per run\n\n", N_ENTITIES);

    BENCH("ecs_new x100000", 5, NULL, bench_create, setup_create, teardown_create);

    BENCH(
        "ecs_new + add 10 components x100000",
        5,
        NULL,
        bench_add_components,
        setup_add,
        teardown_add
    );

    BENCH("ecs_kill x100000", 5, NULL, bench_kill, setup_kill, teardown_kill);

    BENCH(
        "ecs_add + ecs_remove x100000",
        5,
        NULL,
        bench_add_remove,
        setup_add_remove,
        teardown_add_remove
    );

    BENCH("ecs_has x100000", 5, NULL, bench_has, setup_has, teardown_has);

    BENCH("ecs_get x100000", 5, NULL, bench_get, setup_has, teardown_has);

    BENCH(
        "query iter x100000 x10000",
        5,
        NULL,
        bench_query_iter,
        setup_query_iter,
        teardown_query_iter
    );

    BENCH(
        "query fields x100000 x10000",
        100,
        NULL,
        bench_query_fields,
        setup_query_iter,
        teardown_query_iter
    );

    BENCH(
        "query create 2200 tables x24 comps",
        1,
        NULL,
        bench_query_create,
        setup_query_create,
        teardown_query_create
    );

    BENCH(
        "observer emit OnSet x100000",
        5,
        NULL,
        bench_observer_emit,
        setup_observer_emit,
        teardown_observer_emit
    );

    return 0;
}
