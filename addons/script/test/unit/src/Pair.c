#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <test.h>

static ecs_component_t position;
static ecs_entity_t source, target;
static ecs_pair_id_t pair;
static int notifications;

static int run(const char *code) {
    ecs_script_eval_result_t result = { 0 };
    int status = ecs_script_run("pair.si", code, &result);
    if (status)
        fprintf(stderr, "%s\n", result.error);
    free(result.error);
    return status;
}

static ecs_pair_id_t find(ecs_pair_element_t first, ecs_pair_element_t second) {
    return ecs_pair_find(&(ecs_pair_desc_t){ .first = first, .second = second });
}

static void setup(void) {
    ecs_init();
    position = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Position", .fields = "{ float x; float y; }" }
    );
    source = ecs_new_named(0, "Source");
    target = ecs_new_named(0, "Target");
    pair = ecs_pair_init(&(ecs_pair_desc_t){ .first = { position, EcsPairComponent },
                                             .second = { target, EcsPairEntity },
                                             .payload = position });
    float initial[] = { 9, 11 };
    test_true(ecs_pair_set(source, pair, initial));
}

static float output(void) {
    const float *value = ecs_get_cid(ecs_lookup("Output"), position);
    test_true(value != NULL);
    return value[0];
}

void Pair_typed_positions_and_reads(void) {
    ecs_init();
    test_int(
        run("struct Position(x: f32, y: f32)\nTarget {}\nOther {}\n"
            "Source { (Position, Target): {1, 2}\n(Target, Position): {3, 4}\n"
            "(Position, Other): {5, 6}\n(Position, Position): {7, 8} }\n"
            "const p = Source[Position, Target]\n"
            "if Source?[Target, Position] { Copy { Position: {$p.x, $p.y} } }"),
        0
    );
    position = ecs_component_lookup("Position");
    source = ecs_lookup("Source");
    target = ecs_lookup("Target");
    ecs_pair_element_t component = { position, EcsPairComponent };
    ecs_pair_element_t entity = { target, EcsPairEntity };
    const float *value = ecs_pair_get(source, find(component, entity));
    test_true(value != NULL);
    test_flt(value[0], 1);
    test_flt(value[1], 2);
    test_flt(((const float *)ecs_pair_get(source, find(entity, component)))[0], 3);
    test_flt(((const float *)ecs_pair_get(source, find(component, component)))[0], 7);
    test_flt(((const float *)ecs_get_cid(ecs_lookup("Copy"), position))[1], 2);
    test_int(
        run("const target = Target\nSource { (Position, $target): {12, 13} }\n"
            "Read { Position: {Source[Position, $target].x, 0} }\n"
            "Source { ($target, Position): {14, 15} }\n"
            "Reverse { Position: {Source[$target, Position].x, 0} }"),
        0
    );
    test_flt(((const float *)ecs_get_cid(ecs_lookup("Read"), position))[0], 12);
    test_flt(((const float *)ecs_get_cid(ecs_lookup("Reverse"), position))[0], 14);
    ecs_fini();
}

static void changed(ecs_observer_event_t *event) {
    const ecs_pair_event_t *data = event->trigger_data;
    test_uint(event->entity, source);
    test_int(event->event, EcsOnPairSet);
    test_uint(data->pair, pair);
    test_uint(data->payload, position);
    test_flt(((const float *)data->value)[0], 42);
    notifications++;
}

void Pair_global_rollback_and_notifications(void) {
    setup();
    notifications = 0;
    ecs_observer_init(
        &(ecs_observer_desc_t){ .entity = source, .on = EcsOnPairSet, .callback = changed }
    );
    uint32_t components = ecs_component_count();
    test_int(
        run("Source { (Position, Target): {42, 43}\nPosition: {1, 2} }\n"
            "struct Temporary(text: string)\nNewTarget {}\n"
            "NewSource { (Temporary, NewTarget): {\"owned\"} }\nBad { Missing }"),
        -1
    );
    test_uint(notifications, 0);
    test_flt(((const float *)ecs_pair_get(source, pair))[0], 9);
    test_false(ecs_has_cid(source, position));
    test_uint(ecs_lookup("NewSource"), 0);
    test_uint(ecs_lookup("NewTarget"), 0);
    test_uint(ecs_component_lookup("Temporary"), 0);
    test_uint(sireflect_type_by_name("Temporary"), 0);
    test_uint(ecs_component_count(), components);
    test_int(run("Source { (Position, Target): {42, 43} }"), 0);
    test_uint(notifications, 1);
    ecs_fini();
}

void Pair_caught_rollback_and_owning_values(void) {
    ecs_init();
    test_int(
        run("struct Label(text: string)\nTarget {}\n"
            "Source { (Label, Target): {\"initial\"} }\n"
            "try { Source { (Label, Target): {\"discarded\"} }\n"
            "Discarded {}\nBad { Missing } } catch { Recovered {} }\n"
            "Copy { Label: {Source[Label, Target].text} }"),
        0
    );
    test_uint(ecs_lookup("Discarded"), 0);
    test_true(ecs_lookup("Recovered") != 0);
    ecs_component_t label = ecs_component_lookup("Label");
    ecs_pair_id_t id = find(
        (ecs_pair_element_t){ label, EcsPairComponent },
        (ecs_pair_element_t){ ecs_lookup("Target"), EcsPairEntity }
    );
    test_str(*(const char *const *)ecs_pair_get(ecs_lookup("Source"), id), "initial");
    test_str(*(const char *const *)ecs_get_cid(ecs_lookup("Copy"), label), "initial");
    ecs_fini();
}

void Pair_reactive_reads_and_absence(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "reader.si",
        "Output { Position: {Source[Position, Target].x * 2, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_flt(output(), 18);
    float updated[] = { 4, 11 };
    test_true(ecs_pair_set(source, pair, updated));
    test_flt(output(), 8);
    uint64_t evaluations = ecs_script_instance_stats(instance).evaluations;
    ecs_entity_t other = ecs_new_named(0, "Other");
    ecs_pair_id_t unrelated =
        ecs_pair_init(&(ecs_pair_desc_t){ .first = { position, EcsPairComponent },
                                          .second = { other, EcsPairEntity },
                                          .payload = position });
    test_true(ecs_pair_set(source, unrelated, updated));
    test_uint(ecs_script_instance_stats(instance).evaluations, evaluations);
    ecs_script_instance_free(instance);
    test_true(ecs_pair_remove(source, pair));
    instance = ecs_script_instance_run(
        "presence.si",
        "if Source?[Position, Target] { Output { Position: {1, 0} } } "
        "else { Output { Position: {0, 0} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_flt(output(), 0);
    test_true(ecs_pair_set(source, pair, updated));
    test_flt(output(), 1);
    test_true(ecs_pair_remove(source, pair));
    test_flt(output(), 0);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Pair_pending_presence_dependency(void) {
    ecs_init();
    position = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Position", .fields = "{ float x; float y; }" }
    );
    source = ecs_new_named(0, "Source");
    target = ecs_new_named(0, "Target");
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "presence.si",
        "if Source?[Position, Target] { Output { Position: {1, 0} } } "
        "else { Output { Position: {0, 0} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_flt(output(), 0);
    pair = find(
        (ecs_pair_element_t){ position, EcsPairComponent },
        (ecs_pair_element_t){ target, EcsPairEntity }
    );
    test_true(pair != 0);
    float value[] = { 2, 3 };
    test_true(ecs_pair_set(source, pair, value));
    test_flt(output(), 1);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Pair_output_ownership_and_restore(void) {
    setup();
    ecs_script_instance_t *instance =
        ecs_script_instance_run("writer.si", "Source { (Position, Target) = {x: 2} }", NULL, NULL);
    test_true(instance != NULL);
    const float *live = ecs_pair_get(source, pair);
    test_flt(live[0], 2);
    test_flt(live[1], 11);
    ecs_script_eval_result_t result = { 0 };
    ecs_script_instance_t *conflict = ecs_script_instance_run(
        "conflict.si",
        "Source { (Position, Target): {8, 8} }",
        NULL,
        &result
    );
    test_true(conflict == NULL);
    test_true(result.error != NULL);
    free(result.error);
    float host[] = { 2, 25 };
    test_true(ecs_pair_set(source, pair, host));
    ecs_script_instance_free(instance);
    live = ecs_pair_get(source, pair);
    test_flt(live[0], 9);
    test_flt(live[1], 25);
    ecs_fini();
}

void Pair_async_wait_and_outputs(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "wait.si",
        "async { await Source[Position, Target].x > 10\n"
        "Source { (Target, Position): {42, 43} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_pair_id_t output_pair = find(
        (ecs_pair_element_t){ target, EcsPairEntity },
        (ecs_pair_element_t){ position, EcsPairComponent }
    );
    test_false(output_pair && ecs_pair_has(source, output_pair));
    float ready[] = { 12, 11 };
    test_true(ecs_pair_set(source, pair, ready));
    output_pair = find(
        (ecs_pair_element_t){ target, EcsPairEntity },
        (ecs_pair_element_t){ position, EcsPairComponent }
    );
    test_true(output_pair != 0);
    test_flt(((const float *)ecs_pair_get(source, output_pair))[0], 42);
    ecs_script_instance_free(instance);
    test_false(ecs_pair_has(source, output_pair));
    ecs_fini();
}

void Pair_inherited_reads(void) {
    ecs_init();
    position = ecs_component_dynamic_init(&(
        ecs_dynamic_component_desc_t
    ){ .name = "Position", .fields = "{ float x; float y; }", .inheritance = EcsInheritShared });
    source = ecs_new_named(0, "Source");
    target = ecs_new_named(0, "Target");
    ecs_entity_t base = ecs_new_named(0, "Base");
    pair = ecs_pair_init(&(ecs_pair_desc_t){ .first = { position, EcsPairComponent },
                                             .second = { target, EcsPairEntity },
                                             .payload = position });
    float initial[] = { 3, 4 };
    test_true(ecs_pair_set(base, pair, initial));
    ecs_is_a(source, base);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "inherited.si",
        "Output { Position: {Source[Position, Target].x, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_flt(output(), 3);
    float update[] = { 7, 8 };
    test_true(ecs_pair_set(base, pair, update));
    test_flt(output(), 7);
    test_false(ecs_pair_has_owned(source, pair));
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Pair_caught_pending_dependency(void) {
    ecs_init();
    position = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Position", .fields = "{ float x; float y; }" }
    );
    source = ecs_new_named(0, "Source");
    target = ecs_new_named(0, "Target");
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "caught.si",
        "try { const p = Source[Position, Target]\nOutput { Position: {$p.x, 0} } } "
        "catch { Output { Position: {0, 0} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_flt(output(), 0);
    pair = find(
        (ecs_pair_element_t){ position, EcsPairComponent },
        (ecs_pair_element_t){ target, EcsPairEntity }
    );
    test_true(pair != 0);
    float value[] = { 8, 9 };
    test_true(ecs_pair_set(source, pair, value));
    test_flt(output(), 8);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Pair_reload_and_target_deletion(void) {
    setup();
    ecs_script_instance_t *instance =
        ecs_script_instance_run("writer.si", "Source { (Position, Target): {2, 3} }", NULL, NULL);
    test_true(instance != NULL);
    ecs_script_eval_result_t result = { 0 };
    ecs_script_t *program = ecs_script_parse(
        "reload.si",
        "Source { (Position, Target): {7, 8} }\nDiscarded { Missing }",
        NULL,
        &result
    );
    test_true(program != NULL);
    test_int(ecs_script_instance_reload(instance, program, &result), -1);
    test_true(result.error != NULL);
    free(result.error);
    ecs_script_free(program);
    test_flt(((const float *)ecs_pair_get(source, pair))[0], 2);
    test_uint(ecs_lookup("Discarded"), 0);
    ecs_kill(target);
    test_true(ecs_pair_info(pair) == NULL);
    ecs_script_instance_free(instance);
    test_true(ecs_is_alive(source));
    ecs_fini();
}

void Pair_tags_relations_and_errors(void) {
    ecs_init();
    ecs_relation_id_t relation =
        ecs_relation_init("Follows", &(ecs_relation_desc_t){ .storage = EcsRelationDense });
    test_int(
        run("struct Position(x: f32, y: f32)\nLeft {}\nRight {}\n"
            "Source { (Left, Right)\n(Follows, Position): {4, 5} }"),
        0
    );
    position = ecs_component_lookup("Position");
    source = ecs_lookup("Source");
    ecs_pair_id_t tag = find(
        (ecs_pair_element_t){ ecs_lookup("Left"), EcsPairEntity },
        (ecs_pair_element_t){ ecs_lookup("Right"), EcsPairEntity }
    );
    test_true(tag != 0);
    test_true(ecs_pair_has(source, tag));
    ecs_pair_id_t typed = find(
        (ecs_pair_element_t){ relation, EcsPairRelation },
        (ecs_pair_element_t){ position, EcsPairComponent }
    );
    test_true(typed != 0);
    test_flt(((const float *)ecs_pair_get(source, typed))[1], 5);
    test_uint(ecs_target_id(source, relation), 0);
    test_int(run("Bad { (IsA, Right): {1} }"), -1);
    test_uint(ecs_lookup("Bad"), 0);
    test_int(run("Bad { (Missing, Right): {1} }"), -1);
    test_uint(ecs_lookup("Bad"), 0);
    ecs_component_t other = ecs_tag_init("OtherPayload");
    ecs_pair_init(&(ecs_pair_desc_t){ .first = { position, EcsPairComponent },
                                      .second = { ecs_lookup("Right"), EcsPairEntity },
                                      .payload = other });
    test_int(run("Bad { (Position, Right): {1, 2} }"), -1);
    test_uint(ecs_lookup("Bad"), 0);
    ecs_fini();
}

void Pair_schema_reload(void) {
    ecs_init();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "schema.si",
        "struct Payload(text: string)\nTarget {}\nSource { (Payload, Target): {\"owned\"} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_component_t payload = ecs_component_lookup("Payload");
    ecs_entity_t saved_source = ecs_lookup("Source");
    ecs_pair_id_t id = find(
        (ecs_pair_element_t){ payload, EcsPairComponent },
        (ecs_pair_element_t){ ecs_lookup("Target"), EcsPairEntity }
    );
    test_str(*(const char *const *)ecs_pair_get(saved_source, id), "owned");
    ecs_script_eval_result_t result = { 0 };
    ecs_script_t *program = ecs_script_parse(
        "schema.si",
        "struct Payload(count: i64)\nTarget {}\nSource { (Payload, Target): {42} }",
        NULL,
        &result
    );
    test_true(program != NULL);
    test_int(ecs_script_instance_reload(instance, program, &result), 0);
    test_true(result.error == NULL);
    test_uint(ecs_component_lookup("Payload"), payload);
    test_uint(ecs_lookup("Source"), saved_source);
    test_int(*(const int64_t *)ecs_pair_get(saved_source, id), 42);
    ecs_script_free(program);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Pair_pending_self_dependency_is_rejected(void) {
    ecs_init();
    position = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Position", .fields = "{ float x; float y; }" }
    );
    source = ecs_new_named(0, "Source");
    target = ecs_new_named(0, "Target");
    ecs_script_eval_result_t result = { 0 };
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "cycle.si",
        "if Source?[Position, Target] { Existing {} }\n"
        "Source { (Position, Target) = {x: 2} }",
        NULL,
        &result
    );
    test_true(instance == NULL);
    test_true(result.error != NULL);
    test_true(strstr(result.error, "cyclic script output dependency") != NULL);
    free(result.error);
    test_uint(
        find(
            (ecs_pair_element_t){ position, EcsPairComponent },
            (ecs_pair_element_t){ target, EcsPairEntity }
        ),
        0
    );
    test_uint(ecs_lookup("Existing"), 0);
    test_true(ecs_is_alive(source));
    test_true(ecs_is_alive(target));
    ecs_fini();
}
