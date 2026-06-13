#include "siecs.h"
#include "sijson.h"
#include <siecs_test.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

ECS_COMPONENT_DECLARE(Position, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(Position);

ECS_COMPONENT_DECLARE(HookComponent, { int value; });

static uint32_t hook_add_calls;
static uint32_t hook_set_calls;
static uint32_t hook_order;
static uint32_t hook_last_add_order;
static uint32_t hook_last_set_order;
static bool hook_add_saw_zero;
static bool hook_add_saw_component;
static int hook_last_set_old;
static int hook_last_set_new;

static void reset_hook_state(void) {
    hook_add_calls = 0;
    hook_set_calls = 0;
    hook_order = 0;
    hook_last_add_order = 0;
    hook_last_set_order = 0;
    hook_add_saw_zero = false;
    hook_add_saw_component = false;
    hook_last_set_old = 0;
    hook_last_set_new = 0;
}

static void hook_component_on_add(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *ptr
) {
    (void)world;
    (void)entity;

    const HookComponent *value = ptr;
    hook_add_calls++;
    hook_last_add_order = ++hook_order;
    hook_add_saw_zero = value->value == 0;
    hook_add_saw_component = component == ecs_id(HookComponent);
}

static void hook_component_on_set(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *ptr
) {
    (void)component;

    const HookComponent *old_value = ecs_get(world, entity, HookComponent);
    const HookComponent *new_value = ptr;

    hook_set_calls++;
    hook_last_set_order = ++hook_order;
    hook_last_set_old = old_value->value;
    hook_last_set_new = new_value->value;
}

ECS_COMPONENT_DEFINE(HookComponent, .on_add = hook_component_on_add, .on_set = hook_component_on_set);

void component_reflection(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, Position, { 10, 20 });

    void *data = ecs_get(world, entity, Position);
    char *result = sijson_to_json_ptr(Position, data);

    test_str("{\"x\":10,\"y\":20}", result);
    puts(result);
    free(result);
    ecs_fini(world);
}

void component_on_add(void) {
    reset_hook_state();

    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, HookComponent);

    ecs_entity_t entity = ecs_new(world);
    ecs_add(world, entity, HookComponent);

    test_assert(hook_add_calls == 1);
    test_assert(hook_set_calls == 0);
    test_true(hook_add_saw_zero);
    test_true(hook_add_saw_component);
    test_int(0, ecs_get(world, entity, HookComponent)->value);

    ecs_add(world, entity, HookComponent);
    test_assert(hook_add_calls == 1);

    ecs_set(world, entity, HookComponent, { 5 });
    test_assert(hook_add_calls == 1);
    test_assert(hook_set_calls == 1);
    test_assert(hook_last_set_old == 0);
    test_assert(hook_last_set_new == 5);
    test_int(5, ecs_get(world, entity, HookComponent)->value);

    ecs_entity_t implicit_entity = ecs_new(world);
    ecs_set(world, implicit_entity, HookComponent, { 9 });

    test_assert(hook_add_calls == 2);
    test_assert(hook_set_calls == 2);
    test_assert(hook_last_add_order < hook_last_set_order);
    test_true(hook_add_saw_zero);
    test_assert(hook_last_set_old == 0);
    test_assert(hook_last_set_new == 9);
    test_int(9, ecs_get(world, implicit_entity, HookComponent)->value);

    ecs_fini(world);
}
