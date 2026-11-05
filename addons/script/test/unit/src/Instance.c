#include <stdio.h>
#include <stdlib.h>
#include <test.h>

static ecs_component_t position;
static ecs_entity_t source;

static void setup(void) {
    ecs_init();
    position = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Position", .fields = "{ float x; float y; }" }
    );
    source = ecs_new_named(0, "Source");
    float value[] = { 2, 3 };
    ecs_set_cid(source, position, value);
}

static float output(void) {
    const float *value = ecs_get_cid(ecs_lookup("Output"), position);
    test_true(value != NULL);
    return value[0];
}

void Instance_automatic_reactivity(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "reactive.si",
        "Output { Position: {Source[Position].x * 2, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_flt(output(), 4);
    float value[] = { 7, 3 };
    ecs_set_cid(source, position, value);
    test_flt(output(), 14);
    test_uint(ecs_script_instance_stats(instance).evaluations, 2);
    test_int(ecs_script_instance_state(instance), EcsScriptActive);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_unrelated_and_grouped_changes(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "reactive.si",
        "Output { Position: {Source[Position].x, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_entity_t unrelated = ecs_new();
    float value[] = { 100, 20 };
    ecs_set_cid(unrelated, position, value);
    test_uint(ecs_script_instance_stats(instance).evaluations, 1);
    ecs_sync_begin();
    value[0] = 3;
    ecs_set_cid(source, position, value);
    value[0] = 4;
    ecs_set_cid(source, position, value);
    test_flt(output(), 2);
    ecs_sync_end();
    test_flt(output(), 4);
    test_uint(ecs_script_instance_stats(instance).evaluations, 2);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_failed_reload_keeps_active_graph(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "reactive.si",
        "Output { Position: {Source[Position].x * 2, 0} }",
        NULL,
        NULL
    );
    ecs_entity_t retained = ecs_lookup("Output");
    ecs_script_t *candidate = ecs_script_parse(
        "reload.si",
        "Output { Position: {99, 0} }\nBroken { Missing }",
        NULL,
        NULL
    );
    ecs_script_eval_result_t result = { 0 };
    test_int(ecs_script_instance_reload(instance, candidate, &result), -1);
    test_true(result.error != NULL);
    free(result.error);
    ecs_script_free(candidate);
    test_flt(output(), 4);
    test_uint(ecs_lookup("Output"), retained);
    test_uint(ecs_lookup("Broken"), 0);
    float value[] = { 8, 0 };
    ecs_set_cid(source, position, value);
    test_flt(output(), 16);
    test_int(ecs_script_instance_state(instance), EcsScriptActive);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_reload_replaces_dependencies(void) {
    setup();
    ecs_entity_t other = ecs_new_named(0, "Other");
    float value[] = { 9, 0 };
    ecs_set_cid(other, position, value);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "reactive.si",
        "Output { Position: {Source[Position].x, 0} }",
        NULL,
        NULL
    );
    ecs_entity_t retained = ecs_lookup("Output");
    ecs_script_t *candidate =
        ecs_script_parse("reload.si", "Output { Position: {Other[Position].x, 0} }", NULL, NULL);
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_uint(ecs_lookup("Output"), retained);
    test_flt(output(), 9);
    uint64_t before = ecs_script_instance_stats(instance).evaluations;
    value[0] = 20;
    ecs_set_cid(source, position, value);
    test_flt(output(), 9);
    test_uint(ecs_script_instance_stats(instance).evaluations, before);
    ecs_set_cid(other, position, value);
    test_flt(output(), 20);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_shared_inputs_and_conflicting_outputs(void) {
    setup();
    ecs_script_instance_t *first = ecs_script_instance_run(
        "first.si",
        "Output { Position: {Source[Position].x, 0} }",
        NULL,
        NULL
    );
    ecs_script_eval_result_t result = { 0 };
    ecs_script_instance_t *conflicting =
        ecs_script_instance_run("second.si", "Output { Position: {99, 0} }", NULL, &result);
    test_true(conflicting == NULL);
    test_true(result.error != NULL);
    free(result.error);
    test_flt(output(), 2);
    ecs_script_instance_t *second = ecs_script_instance_run(
        "second.si",
        "Second { Position: {Source[Position].x + 1, 0} }",
        NULL,
        NULL
    );
    test_true(second != NULL);
    ecs_script_instance_free(first);
    float value[] = { 10, 0 };
    ecs_set_cid(source, position, value);
    const float *data = ecs_get_cid(ecs_lookup("Second"), position);
    test_flt(data[0], 11);
    ecs_script_instance_free(second);
    ecs_fini();
}

void Instance_restore_and_preserve_host_changes(void) {
    setup();
    ecs_entity_t target = ecs_new_named(0, "Output");
    float baseline[] = { 30, 40 };
    ecs_set_cid(target, position, baseline);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "restore.si",
        "Output { Position: {Source[Position].x, 0} }",
        NULL,
        NULL
    );
    test_flt(output(), 2);
    ecs_script_instance_free(instance);
    test_flt(output(), 30);
    instance = ecs_script_instance_run("restore.si", "Output { Position: {1, 2} }", NULL, NULL);
    baseline[0] = 77;
    ecs_set_cid(target, position, baseline);
    ecs_script_instance_free(instance);
    test_flt(output(), 77);
    ecs_fini();
}

void Instance_missing_reference_recovers(void) {
    ecs_init();
    position = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Position", .fields = "{ float x; float y; }" }
    );
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "pending.si",
        "Output { Position: {Missing[Position].x, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_int(ecs_script_instance_state(instance), EcsScriptPending);
    test_uint(ecs_lookup("Output"), 0);
    ecs_entity_t missing = ecs_new_named(0, "Missing");
    float value[] = { 12, 0 };
    ecs_set_cid(missing, position, value);
    test_int(ecs_script_instance_state(instance), EcsScriptActive);
    test_flt(output(), 12);
    ecs_script_instance_free(instance);
    ecs_fini();
}

static void write_script(const char *path, const char *code) {
    FILE *file = fopen(path, "wb");
    test_true(file != NULL);
    fputs(code, file);
    test_int(fclose(file), 0);
}

void Instance_watched_files_and_includes(void) {
    setup();
    const char *main_file = "instance-watch.si";
    const char *include = "instance-watch-include.si";
    write_script(include, "const number = 5\n");
    write_script(main_file, "include instance-watch-include.si\nOutput { Position: {$number, 0} }");
    ecs_script_instance_desc_t desc = { .watch = true, .watch_interval = 1e-12 };
    ecs_script_instance_t *instance = ecs_script_instance_load(main_file, &desc, NULL);
    test_true(instance != NULL);
    test_flt(output(), 5);
    uint64_t before = ecs_script_instance_stats(instance).evaluations;
    write_script(include, "const number = 5\n");
    test_int(ecs_script_flush(NULL), 0);
    test_uint(ecs_script_instance_stats(instance).evaluations, before);
    write_script(include, "const number = 99\n");
    test_int(ecs_script_flush(NULL), 0);
    test_flt(output(), 99);
    write_script(include, "const number =\n");
    test_int(ecs_script_flush(NULL), 0);
    test_flt(output(), 99);
    test_true(ecs_script_instance_diagnostic(instance)->error != NULL);
    write_script(include, "const number = 123\n");
    test_int(ecs_script_flush(NULL), 0);
    test_flt(output(), 123);
    ecs_script_instance_free(instance);
    test_int(remove(main_file), 0);
    test_int(remove(include), 0);
    ecs_fini();
}

void Instance_member_ownership_and_restore(void) {
    setup();
    ecs_entity_t target = ecs_new_named(0, "Target");
    float initial[] = { 10, 20 };
    ecs_set_cid(target, position, initial);
    ecs_script_instance_t *x = ecs_script_instance_run(
        "x.si",
        "Target { Position = {x: Source[Position].x} }",
        NULL,
        NULL
    );
    ecs_script_instance_t *y = ecs_script_instance_run(
        "y.si",
        "Target { Position = {y: Source[Position].y} }",
        NULL,
        NULL
    );
    test_true(x != NULL);
    test_true(y != NULL);
    const float *value = ecs_get_cid(target, position);
    test_flt(value[0], 2);
    test_flt(value[1], 3);
    ecs_script_eval_result_t result = { 0 };
    ecs_script_instance_t *conflict =
        ecs_script_instance_run("conflict.si", "Target { Position = {x: 99} }", NULL, &result);
    test_true(conflict == NULL);
    test_true(result.error != NULL);
    free(result.error);
    float changed[] = { 4, 5 };
    ecs_set_cid(source, position, changed);
    value = ecs_get_cid(target, position);
    test_flt(value[0], 4);
    test_flt(value[1], 5);
    ecs_script_instance_free(x);
    value = ecs_get_cid(target, position);
    test_flt(value[0], 10);
    test_flt(value[1], 5);
    changed[0] = 10;
    changed[1] = 77;
    ecs_set_cid(target, position, changed);
    ecs_script_instance_free(y);
    value = ecs_get_cid(target, position);
    test_flt(value[0], 10);
    test_flt(value[1], 77);
    ecs_fini();
}

void Instance_member_dependencies(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "member.si",
        "Output { Position: {Source[Position].x, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    float changed[] = { 2, 999 };
    ecs_set_cid(source, position, changed);
    test_uint(ecs_script_instance_stats(instance).evaluations, 1);
    changed[0] = 7;
    ecs_set_cid(source, position, changed);
    test_uint(ecs_script_instance_stats(instance).evaluations, 2);
    test_flt(output(), 7);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_anonymous_identity(void) {
    setup();
    ecs_component_t link = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Link", .fields = "{ ecs_entity_t target; }" }
    );
    test_true(link != 0);
    const char *code = "const child = new { Position: {Source[Position].x, 0} }\n"
                       "Output { Link: {$child} }";
    ecs_script_instance_t *instance = ecs_script_instance_run("anonymous.si", code, NULL, NULL);
    test_true(instance != NULL);
    ecs_entity_t child = *(const ecs_entity_t *)ecs_get_cid(ecs_lookup("Output"), link);
    test_true(ecs_is_alive(child));
    float changed[] = { 9, 0 };
    ecs_set_cid(source, position, changed);
    test_uint(*(const ecs_entity_t *)ecs_get_cid(ecs_lookup("Output"), link), child);
    test_flt(*(const float *)ecs_get_cid(child, position), 9);
    ecs_script_t *candidate = ecs_script_parse(
        "anonymous.si",
        "// retained identity\nconst child = new { Position: {Source[Position].x + 1, 0} }\n"
        "Output { Link: {$child} }",
        NULL,
        NULL
    );
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_uint(*(const ecs_entity_t *)ecs_get_cid(ecs_lookup("Output"), link), child);
    test_flt(*(const float *)ecs_get_cid(child, position), 10);
    ecs_script_instance_free(instance);
    test_false(ecs_is_alive(child));
    ecs_fini();
}

static unsigned region_calls[2];
static int probe_region(
    const ecs_script_value_t *arguments,
    uint32_t count,
    void *result,
    void *context,
    const char **error
) {
    (void)context;
    (void)error;
    test_uint(count, 2);
    int64_t index = *(const int64_t *)arguments[1].data;
    test_true(index == 0 || index == 1);
    region_calls[index]++;
    *(double *)result = *(const double *)arguments[0].data;
    return 0;
}

void Instance_reuses_unchanged_regions(void) {
    setup();
    region_calls[0] = region_calls[1] = 0;
    sireflect_handle_t parameters[] = { sireflect_type_by_name("double"),
                                        sireflect_type_by_name("int64_t") };
    test_int(
        ecs_script_function_register(&(ecs_script_function_desc_t){ .name = "probe",
                                                                    .parameters = parameters,
                                                                    .parameter_count = 2,
                                                                    .result_type = parameters[0],
                                                                    .callback = probe_region }),
        0
    );
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "regions.si",
        "Output { Position: {probe(Source[Position].x, 0), 0} }\n"
        "Second { Position: {probe(Source[Position].y, 1), 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(region_calls[0], 1);
    test_uint(region_calls[1], 1);
    ecs_entity_t second = ecs_lookup("Second");
    float host[] = { 77, 0 };
    ecs_set_cid(second, position, host);
    float changed[] = { 9, 3 };
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_uint(region_calls[1], 1);
    test_flt(output(), 9);
    test_flt(*(const float *)ecs_get_cid(second, position), 77);
    changed[1] = 8;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_uint(region_calls[1], 2);
    test_flt(*(const float *)ecs_get_cid(second, position), 8);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_captured_regions(void) {
    setup();
    region_calls[0] = region_calls[1] = 0;
    sireflect_handle_t parameters[] = { sireflect_type_by_name("double"),
                                        sireflect_type_by_name("int64_t") };
    test_int(
        ecs_script_function_register(&(ecs_script_function_desc_t){ .name = "probe",
                                                                    .parameters = parameters,
                                                                    .parameter_count = 2,
                                                                    .result_type = parameters[0],
                                                                    .callback = probe_region }),
        0
    );
    ecs_component_t text =
        ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "Text",
                                                                    .fields = "{ char *value; }" });
    ecs_entity_t input_text = ecs_new_named(0, "InputText");
    const char *message = "captured";
    ecs_set_cid(input_text, text, &message);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "captured-regions.si",
        "const numbers = [Source[Position].x]\nconst y = Source[Position].y\n"
        "const word = InputText[Text].value\n"
        "Output { Position: {probe($numbers[0], 0), 0} }\n"
        "Second { Position: {probe($y, 1), 0} }\n"
        "Label { Text: {\"{$word:s}\"} }\n"
        "try { Discarded { Text: {$word} }\nBroken { Missing } } catch { Recovered {} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(region_calls[0], 1);
    test_uint(region_calls[1], 1);
    float changed[] = { 9, 3 };
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_uint(region_calls[1], 1);
    test_flt(output(), 9);
    changed[1] = 8;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_uint(region_calls[1], 2);
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Second"), position), 8);
    test_uint(ecs_lookup("Discarded"), 0);
    test_true(ecs_lookup("Recovered") != 0);
    test_str(*(const char *const *)ecs_get_cid(ecs_lookup("Label"), text), "captured");
    message = "updated";
    ecs_set_cid(input_text, text, &message);
    test_uint(region_calls[0], 2);
    test_uint(region_calls[1], 2);
    test_str(*(const char *const *)ecs_get_cid(ecs_lookup("Label"), text), "updated");
    ecs_script_instance_free(instance);
    test_str(*(const char *const *)ecs_get_cid(input_text, text), "updated");
    ecs_fini();
}

void Instance_loop_regions(void) {
    setup();
    region_calls[0] = region_calls[1] = 0;
    sireflect_handle_t parameters[] = { sireflect_type_by_name("double"),
                                        sireflect_type_by_name("int64_t") };
    test_int(
        ecs_script_function_register(&(ecs_script_function_desc_t){ .name = "probe",
                                                                    .parameters = parameters,
                                                                    .parameter_count = 2,
                                                                    .result_type = parameters[0],
                                                                    .callback = probe_region }),
        0
    );
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "loop-regions.si",
        "for (i, n) in [Source[Position].x, Source[Position].y] {\n"
        "\"Item_$i\" { Position: {probe($n, $i), 0} }\n}",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(region_calls[0], 1);
    test_uint(region_calls[1], 1);
    ecs_entity_t first = ecs_lookup("Item_0"), second = ecs_lookup("Item_1");
    test_true(first != 0);
    test_true(second != 0);
    float changed[] = { 9, 3 };
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_uint(region_calls[1], 1);
    test_flt(*(const float *)ecs_get_cid(first, position), 9);
    test_flt(*(const float *)ecs_get_cid(second, position), 3);
    changed[1] = 8;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_uint(region_calls[1], 2);
    test_flt(*(const float *)ecs_get_cid(second, position), 8);
    test_uint(ecs_lookup("Item_0"), first);
    test_uint(ecs_lookup("Item_1"), second);
    ecs_script_instance_free(instance);
    test_false(ecs_is_alive(first));
    test_false(ecs_is_alive(second));
    ecs_fini();
}

void Instance_disjoint_self_dependency(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "self.si",
        "Source { Position = {x: Source[Position].y * 2} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    const float *data = ecs_get_cid(source, position);
    test_flt(data[0], 6);
    float changed[] = { 6, 8 };
    ecs_set_cid(source, position, changed);
    data = ecs_get_cid(source, position);
    test_flt(data[0], 16);
    test_flt(data[1], 8);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_rejects_incompatible_schema(void) {
    ecs_init();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "schema.si",
        "struct Shape(x: f32)\nOutput { Shape: {3} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_component_t cid = ecs_component_lookup("Shape");
    uint32_t count = ecs_component_count();
    ecs_entity_t output_entity = ecs_lookup("Output");
    ecs_script_t *candidate = ecs_script_parse(
        "schema.si",
        "struct Shape(x: f32, y: i64)\nOutput { Shape: {9, 10} }",
        NULL,
        NULL
    );
    ecs_script_eval_result_t result = { 0 };
    test_int(ecs_script_instance_reload(instance, candidate, &result), -1);
    test_true(result.error != NULL);
    free(result.error);
    ecs_script_free(candidate);
    test_uint(ecs_component_count(), count);
    test_uint(ecs_component_lookup("Shape"), cid);
    test_uint(ecs_lookup("Output"), output_entity);
    test_flt(*(const float *)ecs_get_cid(output_entity, cid), 3);
    test_uint(ecs_component_info(cid)->size, sizeof(float));
    candidate =
        ecs_script_parse("schema.si", "struct Shape(x: f32)\nOutput { Shape: {7} }", NULL, NULL);
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_flt(*(const float *)ecs_get_cid(output_entity, cid), 7);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_inherited_member_dependencies(void) {
    setup();
    ecs_component_t shared = ecs_component_dynamic_init(&(
        ecs_dynamic_component_desc_t
    ){ .name = "Shared", .fields = "{ float x; float y; }", .inheritance = EcsInheritShared });
    ecs_entity_t base = ecs_new_named(0, "Base");
    ecs_entity_t other = ecs_new_named(0, "OtherBase");
    float first[] = { 4, 5 }, second[] = { 20, 30 };
    ecs_set_cid(base, shared, first);
    ecs_set_cid(other, shared, second);
    ecs_is_a(source, base);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "shared.si",
        "Output { Position: {Source[Shared].x, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_flt(output(), 4);
    first[1] = 99;
    ecs_set_cid(base, shared, first);
    test_uint(ecs_script_instance_stats(instance).evaluations, 1);
    first[0] = 9;
    ecs_set_cid(base, shared, first);
    test_flt(output(), 9);
    ecs_is_a(source, other);
    test_flt(output(), 20);
    first[0] = 50;
    uint64_t before = ecs_script_instance_stats(instance).evaluations;
    ecs_set_cid(base, shared, first);
    test_uint(ecs_script_instance_stats(instance).evaluations, before);
    second[0] = 21;
    ecs_set_cid(other, shared, second);
    test_flt(output(), 21);
    ecs_script_instance_free(instance);
    ecs_fini();
}

static unsigned reentrant_calls;
static void update_input_during_publish(ecs_observer_event_t *event) {
    test_uint(event->entity, ecs_lookup("Output"));
    test_uint(event->component, position);
    test_int(event->event, EcsOnSet);
    const float *data = event->trigger_data;
    reentrant_calls++;
    if (data[0] == 6) {
        float next[] = { 7, 3 };
        ecs_set_cid(source, position, next);
    } else {
        test_flt(data[0], 14);
    }
}

void Instance_preserves_reentrant_invalidation(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "reentrant.si",
        "Output { Position: {Source[Position].x * 2, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    reentrant_calls = 0;
    ecs_observer_id_t observer =
        ecs_observer_init(&(ecs_observer_desc_t){ .on = EcsOnSet,
                                                  .entity = ecs_lookup("Output"),
                                                  .query.components = { { position, EcsIn } },
                                                  .callback = update_input_during_publish });
    float changed[] = { 3, 3 };
    ecs_set_cid(source, position, changed);
    test_uint(reentrant_calls, 2);
    test_flt(output(), 14);
    ecs_observer_fini(observer);
    ecs_script_instance_free(instance);
    ecs_fini();
}
