#include "siecs.h"
#include <siecs_test.h>

ECS_COMPONENT_DECLARE(ModulePosition, { int value; });
ECS_COMPONENT_DECLARE(ModuleVelocity, { int value; });

ECS_COMPONENT_DEFINE(ModulePosition);
ECS_COMPONENT_DEFINE(ModuleVelocity);

ECS_MODULE_DECLARE(module_physics, { int velocity; });

ECS_MODULE_DECLARE(module_render, { int value; });

ECS_MODULE_DECLARE(module_query, { int unused; });

ECS_MODULE_DEFINE(module_physics);
ECS_MODULE_DEFINE(module_render);
ECS_MODULE_DEFINE(module_query);

static uint32_t module_system_calls;
static uint32_t module_observer_calls;
static uint32_t module_physics_import_calls;
static uint32_t module_render_import_calls;
static int module_last_velocity;
static ecs_query_id_t module_import_query;

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

static void module_render_system(ecs_iter_t *it) { (void)it; }

void module_render_import(const module_render_props_t *props) {
    (void)props;
    module_render_import_calls++;

    ecs_system(
        {
            .name = "Render",
            .query = { .terms = { ecs_in(ModulePosition) } },
            .callback = module_render_system,
            .phase = EcsOnRender,
        }
    );
}

void module_physics_import(const module_physics_props_t *props) {
    module_physics_import_calls++;
    module_last_velocity = props->velocity;

    ECS_COMPONENT_REGISTER(ModulePosition);
    ECS_COMPONENT_REGISTER(ModuleVelocity);

    ecs_system(
        {
            .name = "Move",
            .query = { .terms = { ecs_inout(ModulePosition), ecs_in(ModuleVelocity) } },
            .callback = module_move,
            .phase = EcsOnUpdate,
        }
    );

    ECS_MODULE_IMPORT(module_render, { .value = 1 });

    ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .terms = { ecs_in(ModulePosition) } },
            .callback = module_on_position_set,
        }
    );
}

void module_query_import(const module_query_props_t *props) {
    (void)props;
    module_import_query = ecs_query({ .terms = { ecs_in(ModulePosition) } });
}

static ecs_entity_t module_entity(int position, int velocity) {
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ModulePosition, { position });
    ecs_set(entity, ModuleVelocity, { velocity });
    return entity;
}

void module_import_registers_runtime(void) {
    module_reset();

    ecs_init();
    ecs_module_id_t module = ECS_MODULE_IMPORT(module_physics, { .velocity = 4 });
    ecs_entity_t entity = module_entity(10, 2);

    ecs_progress();

    test_true(ecs_module_is_enabled(module));
    test_int(1, module_physics_import_calls);
    test_int(1, module_render_import_calls);
    test_int(4, module_last_velocity);
    test_int(1, module_system_calls);
    test_int(2, module_observer_calls);
    test_int(12, ecs_get(entity, ModulePosition)->value);

    ecs_fini();
}

void module_name_returns_imported_name(void) {
    module_reset();

    ecs_init();
    ecs_module_id_t module = ECS_MODULE_IMPORT(module_physics, { .velocity = 4 });

    test_str("module_physics", ecs_module_name(module));

    ecs_fini();
}

void module_enable(void) {
    module_reset();

    ecs_init();
    ecs_module_id_t module = ECS_MODULE_IMPORT(module_physics, { .velocity = 1 });
    ecs_entity_t entity = module_entity(10, 2);

    ecs_module_disable(module);
    test_false(ecs_module_is_enabled(module));

    ecs_progress();
    ecs_set(entity, ModulePosition, { 20 });
    test_int(0, module_system_calls);
    test_int(2, module_observer_calls);
    test_int(20, ecs_get(entity, ModulePosition)->value);

    ecs_module_enable(module);
    test_true(ecs_module_is_enabled(module));

    ecs_progress();
    ecs_set(entity, ModulePosition, { 30 });
    test_int(1, module_system_calls);
    test_int(3, module_observer_calls);

    ecs_fini();
}

void module_disabled_import(void) {
    module_reset();

    ecs_init();
    module_physics_props_t props = { .velocity = 3 };
    ecs_module_id_t module = ecs_module(
        {
            .name = "module_physics",
            .id = &ecs_id(module_physics),
            .import = ecs_id(module_physics_import_wrapper),
            .desc = &props,
            .desc_size = sizeof(props),
            .disabled = true,
        }
    );
    ecs_entity_t entity = module_entity(1, 2);

    test_false(ecs_module_is_enabled(module));
    ecs_progress();
    ecs_set(entity, ModulePosition, { 7 });

    test_int(0, module_system_calls);
    test_int(0, module_observer_calls);
    test_int(7, ecs_get(entity, ModulePosition)->value);

    ecs_module_enable(module);
    ecs_progress();
    test_int(1, module_system_calls);

    ecs_fini();
}

void module_double_import_is_noop(void) {
    module_reset();

    ecs_init();
    ecs_module_id_t first = ECS_MODULE_IMPORT(module_physics, { .velocity = 5 });
    ecs_module_id_t second = ECS_MODULE_IMPORT(module_physics, { .velocity = 9 });

    test_int(first, second);
    test_int(1, module_physics_import_calls);
    test_int(5, module_last_velocity);

    ecs_fini();
}

void module_owns_queries_created_during_import(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ModulePosition);
    ecs_module_id_t module = ECS_MODULE_IMPORT(module_query, { .unused = 0 });
    ecs_entity_t entity = ecs_new();
    ecs_add(entity, ModulePosition);

    test_true(module != 0);
    test_int(1, ecs_query_count(module_import_query));
    ecs_fini();
}

void module_forgets_manually_destroyed_query(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ModulePosition);
    ECS_MODULE_IMPORT(module_query, { .unused = 0 });

    ecs_query_fini(module_import_query);
    test_true(true);
    ecs_fini();
}

void module_query_id_reuse_does_not_keep_old_owner(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ModulePosition);
    ECS_MODULE_IMPORT(module_query, { .unused = 0 });
    ecs_query_id_t old_query = module_import_query;
    ecs_query_fini(old_query);

    ecs_query_id_t outside_query = ecs_query({ .terms = { ecs_in(ModulePosition) } });
    test_int(old_query, outside_query);
    ecs_fini();
}

void module_queries_survive_disable_enable(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ModulePosition);
    ecs_module_id_t module = ECS_MODULE_IMPORT(module_query, { .unused = 0 });

    ecs_module_disable(module);
    ecs_module_enable(module);

    ecs_entity_t entity = ecs_new();
    ecs_add(entity, ModulePosition);
    test_int(1, ecs_query_count(module_import_query));
    ecs_fini();
}
