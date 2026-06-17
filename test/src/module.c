#include "siecs.h"
#include <siecs_test.h>

ECS_COMPONENT_DECLARE(ModulePosition, { int value; });
ECS_COMPONENT_DECLARE(ModuleVelocity, { int value; });

ECS_COMPONENT_DEFINE(ModulePosition);
ECS_COMPONENT_DEFINE(ModuleVelocity);

ECS_MODULE_DECLARE(module_physics, {
    int velocity;
});

ECS_MODULE_DECLARE(module_render, {
    int value;
});

ECS_MODULE_DEFINE(module_physics);
ECS_MODULE_DEFINE(module_render);

static uint32_t module_system_calls;
static uint32_t module_observer_calls;
static uint32_t module_physics_import_calls;
static uint32_t module_render_import_calls;
static int module_last_velocity;

static void module_reset(void) {
    module_system_calls = 0;
    module_observer_calls = 0;
    module_physics_import_calls = 0;
    module_render_import_calls = 0;
    module_last_velocity = 0;
}

static void module_move(ecs_iter_t *it) {
    ModulePosition *positions = ecs_field(it, 0);
    const ModuleVelocity *velocities = ecs_field(it, 1);

    for (uint32_t i = 0; i < it->count; i++) {
        positions[i].value += velocities[i].value;
    }

    module_system_calls++;
}

static void module_on_position_set(ecs_observer_event_t *event) {
    (void)event;
    module_observer_calls++;
}

static void module_render_system(ecs_iter_t *it) {
    (void)it;
}

void module_render_import(ecs_world_t *world, const module_render_props_t *props) {
    (void)props;
    module_render_import_calls++;

    ecs_system(world, {
        .name = "Render",
        .query = { .terms = { ecs_in(ModulePosition) } },
        .callback = module_render_system,
        .phase = EcsOnRender,
    });
}

void module_physics_import(ecs_world_t *world, const module_physics_props_t *props) {
    module_physics_import_calls++;
    module_last_velocity = props->velocity;

    ECS_COMPONENT_REGISTER(world, ModulePosition);
    ECS_COMPONENT_REGISTER(world, ModuleVelocity);

    ecs_system(world, {
        .name = "Move",
        .query = { .terms = { ecs_inout(ModulePosition), ecs_in(ModuleVelocity) } },
        .callback = module_move,
        .phase = EcsOnUpdate,
    });

    ECS_MODULE_IMPORT(world, module_render, { .value = 1 });

    ecs_observer(world, {
        .on = OnSet,
        .query = { .terms = { ecs_in(ModulePosition) } },
        .callback = module_on_position_set,
    });
}

static ecs_entity_t module_entity(ecs_world_t *world, int position, int velocity) {
    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, ModulePosition, { position });
    ecs_set(world, entity, ModuleVelocity, { velocity });
    return entity;
}

void module_import_registers_runtime(void) {
    module_reset();

    ecs_world_t *world = ecs_init();
    ecs_module_id_t module = ECS_MODULE_IMPORT(world, module_physics, { .velocity = 4 });
    ecs_entity_t entity = module_entity(world, 10, 2);

    ecs_progress(world);

    test_true(ecs_module_is_enabled(world, module));
    test_int(1, module_physics_import_calls);
    test_int(1, module_render_import_calls);
    test_int(4, module_last_velocity);
    test_int(1, module_system_calls);
    test_int(2, module_observer_calls);
    test_int(12, ecs_get(world, entity, ModulePosition)->value);

    ecs_fini(world);
}

void module_enable(void) {
    module_reset();

    ecs_world_t *world = ecs_init();
    ecs_module_id_t module = ECS_MODULE_IMPORT(world, module_physics, { .velocity = 1 });
    ecs_entity_t entity = module_entity(world, 10, 2);

    ecs_module_disable(world, module);
    test_false(ecs_module_is_enabled(world, module));

    ecs_progress(world);
    ecs_set(world, entity, ModulePosition, { 20 });
    test_int(0, module_system_calls);
    test_int(2, module_observer_calls);
    test_int(20, ecs_get(world, entity, ModulePosition)->value);

    ecs_module_enable(world, module);
    test_true(ecs_module_is_enabled(world, module));

    ecs_progress(world);
    ecs_set(world, entity, ModulePosition, { 30 });
    test_int(1, module_system_calls);
    test_int(3, module_observer_calls);

    ecs_fini(world);
}

void module_disabled_import(void) {
    module_reset();

    ecs_world_t *world = ecs_init();
    module_physics_props_t props = { .velocity = 3 };
    ecs_module_id_t module = ecs_module(world, {
        .name = "module_physics",
        .id = &ecs_id(module_physics),
        .import = ecs_id(module_physics_import_wrapper),
        .desc = &props,
        .desc_size = sizeof(props),
        .disabled = true,
    });
    ecs_entity_t entity = module_entity(world, 1, 2);

    test_false(ecs_module_is_enabled(world, module));
    ecs_progress(world);
    ecs_set(world, entity, ModulePosition, { 7 });

    test_int(0, module_system_calls);
    test_int(0, module_observer_calls);
    test_int(7, ecs_get(world, entity, ModulePosition)->value);

    ecs_module_enable(world, module);
    ecs_progress(world);
    test_int(1, module_system_calls);

    ecs_fini(world);
}

void module_double_import_is_noop(void) {
    module_reset();

    ecs_world_t *world = ecs_init();
    ecs_module_id_t first = ECS_MODULE_IMPORT(world, module_physics, { .velocity = 5 });
    ecs_module_id_t second = ECS_MODULE_IMPORT(world, module_physics, { .velocity = 9 });

    test_int(first, second);
    test_int(1, module_physics_import_calls);
    test_int(5, module_last_velocity);

    ecs_fini(world);
}

void module_reimport_after_world_fini(void) {
    module_reset();

    ecs_world_t *first_world = ecs_init();
    ecs_module_id_t first = ECS_MODULE_IMPORT(first_world, module_physics, { .velocity = 1 });

    test_true(first != 0);
    test_true(ecs_module_is_enabled(first_world, first));
    ecs_fini(first_world);

    test_int(0, ecs_id(module_physics));

    ecs_world_t *second_world = ecs_init();
    ecs_module_id_t second = ECS_MODULE_IMPORT(second_world, module_physics, { .velocity = 2 });

    test_true(second != 0);
    test_int(2, module_physics_import_calls);
    test_true(ecs_module_is_enabled(second_world, second));

    ecs_fini(second_world);
}
