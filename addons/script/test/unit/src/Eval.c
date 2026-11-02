#include <test.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int run(const char *source) {
    ecs_script_eval_result_t result = {0};
    int status = ecs_script_run("eval-test", source, &result);
    if (result.error) fprintf(stderr, "%d:%d %s\n", result.line, result.column, result.error);
    free(result.error);
    return status;
}

void Eval_native_entities(void) {
    ecs_init();
    test_int(run("Root {\n Child {}\n Other {}\n}\n"), 0);
    ecs_entity_t root = ecs_lookup("Root"), child = ecs_lookup("Root::Child");
    test_true(root != 0);
    test_true(child != 0);
    test_uint(ecs_target_id(child, ecs_rid(ChildOf)), root);
    test_true(ecs_lookup("Root::Other") != 0);
    ecs_fini();
}

void Eval_native_component_values(void) {
    ecs_init();
    ecs_component_t position = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "Position", .fields = "{ float x; float y; }",
    });
    test_int(run("const x = 2 + 3 * 4\nShip { Position: {x: $x, y: 20} }\n"), 0);
    ecs_entity_t entity = ecs_lookup("Ship");
    test_true(entity != 0);
    test_true(ecs_has_cid(entity, position));
    const float *data = ecs_get_cid(entity, position);
    test_flt(data[0], 14);
    test_flt(data[1], 20);
    ecs_fini();
}

void Eval_script_struct(void) {
    ecs_init();
    test_int(run("using flecs.meta\nstruct Position(x: f32, y: f32)\nShip { Position: {10, 20} }\n"), 0);
    ecs_component_t position = ecs_component_lookup("Position");
    test_true(position != 0);
    test_true(ecs_component_info(position)->type != 0);
    test_true(ecs_lookup("Position::x") != 0);
    const float *data = ecs_get_cid(ecs_lookup("Ship"), position);
    test_true(data != NULL);
    test_flt(data[0], 10);
    test_flt(data[1], 20);
    ecs_fini();
}

void Eval_native_tags(void) {
    ecs_init();
    test_int(run("Active {}\nShip { Active }\n"), 0);
    ecs_component_t active = ecs_component_lookup("Active");
    test_true(active != 0);
    test_uint(ecs_component_info(active)->size, 0);
    test_true(ecs_has_cid(ecs_lookup("Ship"), active));
    ecs_fini();
}

void Eval_conditions_loops(void) {
    ecs_init();
    test_int(run("const x = 4\nif $x > 3 { Yes {} } else { No {} }\nfor i in 0..3 { _ {} }\n"), 0);
    test_true(ecs_lookup("Yes") != 0);
    test_uint(ecs_lookup("No"), 0);
    ecs_fini();
}

void Eval_registered_relation(void) {
    ecs_init();
    ecs_relation_id_t follows = ecs_relation_init("Follows", &(ecs_relation_desc_t){ .storage = EcsRelationDense });
    test_int(run("Target {}\nSource { (Follows, Target) }\n"), 0);
    test_uint(ecs_target_id(ecs_lookup("Source"), follows), ecs_lookup("Target"));
    ecs_fini();
}

void Eval_interpolated_ids(void) {
    ecs_init();
    ecs_component_t position = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "Position", .fields = "{ float x; float y; }",
    });
    test_int(run(
        "for i in 0..2 { prefab \"Shape_$i\" { Position: {$i, 20} } }\n"
        "for i in 0..2 { \"Item_$i\" { (IsA, \"Shape_$i\") } }\n"), 0);
    for (int i = 0; i < 2; i++) {
        char name[32], base[32];
        snprintf(name, sizeof name, "Item_%d", i);
        snprintf(base, sizeof base, "Shape_%d", i);
        ecs_entity_t entity = ecs_lookup(name);
        test_uint(ecs_target_id(entity, ecs_rid(IsA)), ecs_lookup(base));
        const float *value = ecs_get_cid(entity, position);
        test_true(value != NULL);
        test_flt(value[0], i);
        test_flt(value[1], 20);
    }
    test_int(run("Bad { (IsA, \"Missing_{42}\") }"), -1);
    test_uint(ecs_target_id(ecs_lookup("Bad"), ecs_rid(IsA)), 0);
    ecs_relation_id_t follows = ecs_relation_init("Follows", &(ecs_relation_desc_t){.storage = EcsRelationDense});
    test_int(run("const relation = \"Follows\"\nTarget {}\nSource { (\"$relation\", Target) }"), 0);
    test_uint(ecs_target_id(ecs_lookup("Source"), follows), ecs_lookup("Target"));
    ecs_fini();
}

void Eval_parse_error_does_not_mutate(void) {
    ecs_init();
    ecs_script_eval_result_t result = {0};
    test_int(ecs_script_run("broken", "Root {}\nBroken {", &result), -1);
    test_true(result.error != NULL);
    test_uint(ecs_lookup("Root"), 0);
    free(result.error);
    ecs_fini();
}

void Eval_unknown_component_is_error(void) {
    ecs_init();
    ecs_script_eval_result_t result = {0};
    test_int(ecs_script_run("unknown", "Ship { Missing: {10} }", &result), -1);
    test_true(result.error != NULL);
    test_true(strstr(result.error, "Missing") != NULL);
    free(result.error);
    ecs_fini();
}

void Eval_interpolated_names(void) {
    ecs_init();
    test_int(run("for i in 0..4 {\n \"Entity_{$i * 2}\" {}\n}\n"), 0);
    test_true(ecs_lookup("Entity_0") != 0);
    test_true(ecs_lookup("Entity_2") != 0);
    test_true(ecs_lookup("Entity_4") != 0);
    test_true(ecs_lookup("Entity_6") != 0);
    test_uint(ecs_lookup("Entity_1"), 0);
    ecs_fini();
}

void Eval_reusable_runtime(void) {
    ecs_init();
    ecs_script_eval_result_t result = {0};
    ecs_script_t *script = ecs_script_parse("reusable",
        "struct Position(x: f32, y: f32)\nconst x = 3\nShip { Position: {$x, 7} }", NULL, &result);
    test_true(script != NULL);
    ecs_script_runtime_t *runtime = ecs_script_runtime_new();
    test_true(runtime != NULL);
    ecs_script_eval_desc_t desc = {.runtime = runtime};
    ecs_entity_t handle = 0;
    for (int i = 0; i < 100; i++) {
        test_int(ecs_script_eval(script, &desc, &result), 0);
        test_true(result.error == NULL);
        ecs_entity_t entity = ecs_lookup("Ship");
        if (!i) handle = entity;
        test_uint(entity, handle);
        const float *value = ecs_get_cid(entity, ecs_component_lookup("Position"));
        test_flt(value[0], 3);
        test_flt(value[1], 7);
    }
    ecs_script_runtime_free(runtime);
    ecs_script_free(script);
    ecs_fini();
}

void Eval_component_member_reads(void) {
    ecs_init();
    test_int(run("struct Position(x: f32, y: f32)\n"
        "Source { Position: {10, 20} }\n"
        "const p = Source[Position]\n"
        "Target { Position: {$p.y, $p.x + 5} }"), 0);
    ecs_component_t position = ecs_component_lookup("Position");
    const float *data = ecs_get_cid(ecs_lookup("Target"), position);
    test_true(data != NULL);
    test_flt(data[0], 20);
    test_flt(data[1], 15);
    ecs_fini();
}

ECS_COMPONENT_DECLARE(ScriptTracked, { int value; });

static int tracked_live;
static void tracked_ctor(void *ptr, uint32_t count) {
    tracked_live += (int)count;
    ScriptTracked *values = ptr;
    for (uint32_t i = 0; i < count; i++) values[i].value = 11;
}
static void tracked_copy_ctor(void *dst, const void *src, uint32_t count) {
    tracked_live += (int)count;
    memcpy(dst, src, sizeof(ScriptTracked) * count);
}
static void tracked_copy(void *dst, const void *src, uint32_t count) {
    memcpy(dst, src, sizeof(ScriptTracked) * count);
}
static void tracked_dtor(void *ptr, uint32_t count) {
    (void)ptr;
    tracked_live -= (int)count;
}
ECS_COMPONENT_DEFINE(ScriptTracked, .ops = {
    .ctor = tracked_ctor, .copy_ctor = tracked_copy_ctor,
    .copy = tracked_copy, .dtor = tracked_dtor,
});

typedef struct EventState {
    unsigned calls;
    ecs_entity_t entity;
    ecs_component_t component;
    int value;
} EventState;

static void capture_set(ecs_observer_event_t *event) {
    EventState *state = (EventState*)event->user_data;
    state->calls++;
    state->entity = event->entity;
    state->component = event->component;
    state->value = ((const ScriptTracked*)event->trigger_data)->value;
}

void Eval_native_lifecycle_and_observers(void) {
    ecs_init();
    tracked_live = 0;
    ECS_COMPONENT_REGISTER(ScriptTracked);
    EventState state = {0};
    ecs_observer({
        .on = EcsOnSet,
        .query.components = {ecs_filter(ScriptTracked)},
        .callback = capture_set,
        .user_data = (uintptr_t)&state,
    });
    test_int(run("Thing { ScriptTracked: {value: 40} }"), 0);
    ecs_entity_t entity = ecs_lookup("Thing");
    test_uint(state.calls, 1);
    test_uint(state.entity, entity);
    test_uint(state.component, ecs_id(ScriptTracked));
    test_int(state.value, 40);
    test_int(tracked_live, 1);
    ecs_script_eval_result_t result = {0};
    test_int(ecs_script_run("bad", "Thing { ScriptTracked: {missing: 50} }", &result), -1);
    test_true(result.error != NULL);
    free(result.error);
    test_uint(state.calls, 1);
    test_int(ecs_get(entity, ScriptTracked)->value, 40);
    test_int(tracked_live, 1);
    ecs_fini();
    test_int(tracked_live, 0);
}

void Eval_value_snapshot_and_runtime_error_recovery(void) {
    ecs_init();
    ecs_script_eval_result_t result = {0};
    ecs_script_runtime_t *runtime = ecs_script_runtime_new();
    ecs_script_eval_desc_t desc = {.runtime = runtime};
    test_int(ecs_script_run_w_desc("values",
        "struct Position(x: f32, y: f32)\n"
        "Source { Position: {10, 20} }\n"
        "const p = Source[Position]\n"
        "Source { Position: {30, 40} }\n"
        "Target { Position: {$p.x, $p.y} }", &desc, &result), 0);
    const float *value = ecs_get_cid(ecs_lookup("Target"), ecs_component_lookup("Position"));
    test_flt(value[0], 10);
    test_flt(value[1], 20);
    test_int(ecs_script_run_w_desc("bad", "const p = Source[Position]\nconst x = 3 / 0", &desc, &result), -1);
    test_true(result.error != NULL);
    free(result.error);
    test_int(ecs_script_run_w_desc("ok", "Recovered {}", &desc, &result), 0);
    test_true(result.error == NULL);
    test_true(ecs_lookup("Recovered") != 0);
    ecs_script_runtime_free(runtime);
    ecs_fini();
}

ECS_COMPONENT_DECLARE(ScriptOwned, { int value; int *payload; });
static int owned_payloads;
static void owned_ctor(void *ptr, uint32_t count) {
    ScriptOwned *values = ptr;
    for (uint32_t i = 0; i < count; i++) {
        values[i] = (ScriptOwned){.payload = malloc(sizeof(int))};
        test_true(values[i].payload != NULL);
        *values[i].payload = 123;
        owned_payloads++;
    }
}
static void owned_copy_ctor(void *dst, const void *src, uint32_t count) {
    ScriptOwned *values = dst;
    const ScriptOwned *source = src;
    owned_ctor(dst, count);
    for (uint32_t i = 0; i < count; i++) {
        values[i].value = source[i].value;
        *values[i].payload = *source[i].payload;
    }
}
static void owned_copy(void *dst, const void *src, uint32_t count) {
    ScriptOwned *values = dst;
    const ScriptOwned *source = src;
    for (uint32_t i = 0; i < count; i++) {
        values[i].value = source[i].value;
        *values[i].payload = *source[i].payload;
    }
}
static void owned_dtor(void *ptr, uint32_t count) {
    ScriptOwned *values = ptr;
    for (uint32_t i = 0; i < count; i++) {
        free(values[i].payload);
        values[i].payload = NULL;
        owned_payloads--;
    }
}
ECS_COMPONENT_DEFINE(ScriptOwned, .ops = {
    .ctor = owned_ctor, .copy_ctor = owned_copy_ctor,
    .copy = owned_copy, .dtor = owned_dtor,
});

void Eval_owned_component_copy(void) {
    ecs_init();
    owned_payloads = 0;
    ECS_COMPONENT_REGISTER(ScriptOwned);
    test_int(run("Source { ScriptOwned: {value: 40} }\n"
        "const value = Source[ScriptOwned]\n"
        "Target { ScriptOwned: $value }"), 0);
    const ScriptOwned *source = ecs_get(ecs_lookup("Source"), ScriptOwned);
    const ScriptOwned *target = ecs_get(ecs_lookup("Target"), ScriptOwned);
    test_int(source->value, 40);
    test_int(target->value, 40);
    test_int(*target->payload, 123);
    test_true(source->payload != target->payload);
    test_int(owned_payloads, 2);
    ecs_fini();
    test_int(owned_payloads, 0);
}
