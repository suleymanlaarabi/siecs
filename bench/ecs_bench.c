#include "bench.h"
#include "../ecs/world.h"
#include <stdlib.h>
#include <stdio.h>

/* ── Components (39 variables total) ───────────────────────────── */

ECS_COMPONENT_DECLARE(Position,    { float x, y, z; })           /* 3  */
ECS_COMPONENT_DECLARE(Velocity,    { float x, y, z; })           /* 3  */
ECS_COMPONENT_DECLARE(Rotation,    { float x, y, z, w; })        /* 4  */
ECS_COMPONENT_DECLARE(Scale,       { float x, y, z; })           /* 3  */
ECS_COMPONENT_DECLARE(Color,       { float r, g, b, a; })        /* 4  */
ECS_COMPONENT_DECLARE(Health,      { float hp, max_hp, regen; }) /* 3  */
ECS_COMPONENT_DECLARE(Stats,       { float atk, def, spd, mass; }) /* 4 */
ECS_COMPONENT_DECLARE(Meta,        { int active, layer, tag, flags; }) /* 4 */
ECS_COMPONENT_DECLARE(Camera,      { float near, far, fov, aspect; }) /* 4 */
ECS_COMPONENT_DECLARE(Light,       { float lx, ly, lz, lr, lg, lb, intensity; }) /* 7 */

ECS_COMPONENT_DEFINE(Position)
ECS_COMPONENT_DEFINE(Velocity)
ECS_COMPONENT_DEFINE(Rotation)
ECS_COMPONENT_DEFINE(Scale)
ECS_COMPONENT_DEFINE(Color)
ECS_COMPONENT_DEFINE(Health)
ECS_COMPONENT_DEFINE(Stats)
ECS_COMPONENT_DEFINE(Meta)
ECS_COMPONENT_DEFINE(Camera)
ECS_COMPONENT_DEFINE(Light)

#define N_ENTITIES 100000

/* ── Shared world ────────────────────────────────────────────────── */

static ecs_world_t *g_world;
static ecs_entity_t *g_entities;

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

/* ── bench: create N entities (no components) ───────────────────── */

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

/* ── bench: create + add all 10 components ──────────────────────── */

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

/* ── bench: kill N pre-created entities ─────────────────────────── */

static void setup_kill(void *ctx) {
    (void)ctx;
    g_world    = ecs_init();
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

/* ── bench: has_component on alive entities ─────────────────────── */

static void setup_has(void *ctx) {
    (void)ctx;
    g_world    = ecs_init();
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

/* ── main ────────────────────────────────────────────────────────── */

int main(void) {
    printf("ECS benchmark — %d entities per run\n\n", N_ENTITIES);

    BENCH("ecs_new x100000",
          5, NULL, bench_create, setup_create, teardown_create);

    BENCH("ecs_new + add 10 components x100000",
          5, NULL, bench_add_components, setup_add, teardown_add);

    BENCH("ecs_kill x100000",
          5, NULL, bench_kill, setup_kill, teardown_kill);

    BENCH("ecs_has x100000",
          5, NULL, bench_has, setup_has, teardown_has);

    return 0;
}
