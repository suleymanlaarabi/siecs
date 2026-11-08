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

void Instance_native_pairs_preserve_host_state(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "host-pairs.si",
        "KeptSource { Position: {1, 2} }\nKeptTarget {}\nTransient {}",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_entity_t kept_source = ecs_lookup("KeptSource");
    ecs_entity_t kept_target = ecs_lookup("KeptTarget");
    ecs_entity_t external_source = ecs_new(), external_target = ecs_new();
    ecs_pair_id_t outgoing =
        ecs_pair_init(&(ecs_pair_desc_t){ .first = { position, EcsPairComponent },
                                          .second = { external_target, EcsPairEntity },
                                          .payload = position });
    ecs_pair_id_t incoming =
        ecs_pair_init(&(ecs_pair_desc_t){ .first = { kept_target, EcsPairEntity },
                                          .second = { position, EcsPairComponent },
                                          .payload = position });
    test_true(outgoing != 0 && incoming != 0);
    float value[] = { 9, 12 };
    test_true(ecs_pair_set(kept_source, outgoing, value));
    test_true(ecs_pair_set(external_source, incoming, value));
    ecs_script_instance_free(instance);
    test_true(ecs_is_alive(kept_source));
    test_true(ecs_is_alive(kept_target));
    test_uint(ecs_lookup("Transient"), 0);
    test_false(ecs_has_cid(kept_source, position));
    test_flt(((const float *)ecs_pair_get(kept_source, outgoing))[0], 9);
    test_flt(((const float *)ecs_pair_get(external_source, incoming))[1], 12);
    ecs_fini();
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

void Instance_replaces_schema_transactionally(void) {
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
        "struct Shape(x: f32, y: i64)\nOutput { Shape: {9, 10} }\nBroken { Missing }",
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
    candidate = ecs_script_parse(
        "schema.si",
        "struct Shape(x: f64, y: i64)\nOutput { Shape: {7, 10} }",
        NULL,
        NULL
    );
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_flt(*(const double *)ecs_get_cid(output_entity, cid), 7);
    test_int(((const int64_t *)ecs_get_cid(output_entity, cid))[1], 10);
    test_uint(ecs_component_lookup("Shape"), cid);
    test_uint(ecs_lookup("Output"), output_entity);
    test_uint(ecs_component_count(), count);
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

static void register_region_probe(void) {
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
}

void Instance_control_regions(void) {
    setup();
    register_region_probe();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "controls.si",
        "if probe(Source[Position].x, 0) > 0 {\n"
        "  for (i, n) in [Source[Position].x, 10] {\n"
        "    \"Item_$i\" { Position: {probe($n, 1), 0} }\n"
        "  }\n} else { Empty {} }\n"
        "Output { Position: {Source[Position].y, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(region_calls[0], 1);
    test_uint(region_calls[1], 2);
    ecs_entity_t item = ecs_lookup("Item_1");
    float changed[] = { 2, 8 };
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 1);
    test_uint(region_calls[1], 2);
    test_flt(output(), 8);
    changed[0] = 7;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_uint(region_calls[1], 3);
    test_uint(ecs_lookup("Item_1"), item);
    changed[0] = -1;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 3);
    test_uint(region_calls[1], 3);
    test_true(ecs_lookup("Empty") != 0);
    test_uint(ecs_lookup("Item_0"), 0);
    test_false(ecs_is_alive(item));
    changed[0] = 3;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 4);
    test_uint(region_calls[1], 5);
    test_uint(ecs_lookup("Empty"), 0);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_lexical_transitions(void) {
    setup();
    register_region_probe();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "locals.si",
        "mut x = probe(Source[Position].x, 0)\n"
        "while $x < 5 { $x = $x + 1 }\n"
        "fn current() -> f64 { return $x }\n"
        "Output { Position: {current(), Source[Position].y} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_flt(output(), 5);
    float changed[] = { 2, 9 };
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 1);
    test_flt(output(), 5);
    test_flt(((const float *)ecs_get_cid(ecs_lookup("Output"), position))[1], 9);
    changed[0] = 8;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_flt(output(), 8);
    changed[1] = 4;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_flt(output(), 8);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_template_regions(void) {
    setup();
    register_region_probe();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "templates.si",
        "fn read() -> f64 { return probe(Source[Position].x, 0) }\n"
        "template Model { prop x: f64 = read()\n Position: {$x, 0} }\n"
        "with Position: {100, 0} { Model First() }\n"
        "Output { Position: {Source[Position].y, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(region_calls[0], 1);
    ecs_entity_t first = ecs_lookup("First");
    test_flt(*(const float *)ecs_get_cid(first, position), 2);
    float changed[] = { 2, 7 };
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 1);
    changed[0] = 6;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    test_flt(*(const float *)ecs_get_cid(first, position), 6);
    changed[1] = 9;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 2);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_schema_owning_values_and_consumers(void) {
    setup();
    ecs_script_instance_t *owner = ecs_script_instance_run(
        "schema.si",
        "struct Shape(x: f32, text: string)\nOutput { Shape: {3, \"old\"} }",
        NULL,
        NULL
    );
    test_true(owner != NULL);
    ecs_component_t shape = ecs_component_lookup("Shape");
    ecs_entity_t host = ecs_new_named(0, "Host");
    struct {
        float x;
        char *text;
    } previous = { 4, "host string" };
    ecs_set_cid(host, shape, &previous);
    ecs_script_instance_t *consumer = ecs_script_instance_run(
        "consumer.si",
        "Result { Position: {Output[Shape].x, 0} }",
        NULL,
        NULL
    );
    test_true(consumer != NULL);
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Result"), position), 3);
    ecs_script_t *candidate = ecs_script_parse(
        "schema.si",
        "struct Shape(text: string, x: f64, items: i64[2])\n"
        "Output { Shape: {\"new\", 12, [7, 8]} }",
        NULL,
        NULL
    );
    test_int(ecs_script_instance_reload(owner, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_uint(ecs_component_lookup("Shape"), shape);
    struct replacement {
        char *text;
        double x;
        int64_t items[2];
    };
    const struct replacement *value = ecs_get_cid(ecs_lookup("Output"), shape);
    test_str(value->text, "new");
    test_flt(value->x, 12);
    test_int(value->items[1], 8);
    const struct replacement *reset = ecs_get_cid(host, shape);
    test_true(reset->text == NULL);
    test_flt(reset->x, 0);
    test_int(reset->items[0], 0);
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Result"), position), 12);
    struct replacement changed = { "host edit", 21, { 9, 10 } };
    ecs_set_cid(ecs_lookup("Output"), shape, &changed);
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Result"), position), 21);
    ecs_script_instance_free(consumer);
    ecs_script_instance_free(owner);
    test_str(
        ((const struct replacement *)ecs_get_cid(ecs_lookup("Output"), shape))->text,
        "host edit"
    );
    ecs_fini();
}

void Instance_schema_ownership(void) {
    setup();
    ecs_script_eval_result_t error = { 0 };
    ecs_script_instance_t *intruder = ecs_script_instance_run(
        "intruder.si",
        "struct Position(x: i64)\nStolen { Position: {99} }",
        NULL,
        &error
    );
    test_true(intruder == NULL);
    test_true(error.error != NULL);
    free(error.error);
    test_uint(ecs_lookup("Stolen"), 0);
    test_uint(ecs_component_info(position)->size, 2 * sizeof(float));
    ecs_script_instance_t *owner = ecs_script_instance_run(
        "owner.si",
        "struct Owned(x: f32)\nOutput { Owned: {5} }",
        NULL,
        NULL
    );
    test_true(owner != NULL);
    ecs_component_t owned = ecs_component_lookup("Owned");
    error = (ecs_script_eval_result_t){ 0 };
    intruder = ecs_script_instance_run(
        "intruder.si",
        "struct Owned(x: i64)\nStolen { Owned: {99} }",
        NULL,
        &error
    );
    test_true(intruder == NULL);
    test_true(error.error != NULL);
    free(error.error);
    test_uint(ecs_lookup("Stolen"), 0);
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Output"), owned), 5);
    ecs_script_instance_free(owner);
    ecs_fini();
}

void Instance_annotation_reload_and_ownership(void) {
    ecs_init();
    ecs_script_instance_t *instance =
        ecs_script_instance_run("metadata.si", "@brief old\nOutput {}", NULL, NULL);
    test_true(instance != NULL);
    ecs_entity_t entity = ecs_lookup("Output");
    test_str(ecs_script_annotation(entity, "brief"), "old");
    ecs_script_t *candidate =
        ecs_script_parse("metadata.si", "@brief new\nOutput {}\nBroken { Missing }", NULL, NULL);
    ecs_script_eval_result_t error = { 0 };
    test_int(ecs_script_instance_reload(instance, candidate, &error), -1);
    test_true(error.error != NULL);
    free(error.error);
    ecs_script_free(candidate);
    test_str(ecs_script_annotation(entity, "brief"), "old");
    candidate = ecs_script_parse("metadata.si", "@brief new\nOutput {}", NULL, NULL);
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_uint(ecs_lookup("Output"), entity);
    test_str(ecs_script_annotation(entity, "brief"), "new");
    ecs_component_t component = ecs_component_lookup("siecs::script::annotation::brief");
    const char *text = "host edit";
    ecs_set_cid(entity, component, &text);
    ecs_script_instance_free(instance);
    test_true(ecs_is_alive(entity));
    test_str(ecs_script_annotation(entity, "brief"), "host edit");
    ecs_fini();
}

void Instance_caught_regions(void) {
    setup();
    register_region_probe();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "caught.si",
        "try {\n if Source[Position].x > 0 { Bad { Missing } }\n"
        " Safe { Position: {probe(10, 0), 0} }\n"
        "} catch { Recovered { Position: {probe(20, 1), 0} } }\n"
        "Output { Position: {Source[Position].y, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(region_calls[0], 0);
    test_uint(region_calls[1], 1);
    float changed[] = { 2, 9 };
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[1], 1);
    test_flt(output(), 9);
    changed[0] = -1;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 1);
    test_uint(region_calls[1], 1);
    test_true(ecs_lookup("Safe") != 0);
    test_uint(ecs_lookup("Recovered"), 0);
    changed[1] = 12;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 1);
    test_uint(region_calls[1], 1);
    changed[0] = 1;
    ecs_set_cid(source, position, changed);
    test_uint(region_calls[0], 1);
    test_uint(region_calls[1], 2);
    test_uint(ecs_lookup("Safe"), 0);
    test_true(ecs_lookup("Recovered") != 0);
    test_uint(ecs_lookup("Bad"), 0);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_anonymous_insertion_and_reorder(void) {
    setup();
    ecs_component_t link = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Link", .fields = "{ ecs_entity_t target; }" }
    );
    const char *initial = "const a = new { Position: {10, 0} }\n"
                          "const b = new { Position: {20, 0} }\n"
                          "A { Link: {$a} }\nB { Link: {$b} }";
    ecs_script_instance_t *instance = ecs_script_instance_run("identity.si", initial, NULL, NULL);
    test_true(instance != NULL);
    ecs_entity_t a = *(const ecs_entity_t *)ecs_get_cid(ecs_lookup("A"), link);
    ecs_entity_t b = *(const ecs_entity_t *)ecs_get_cid(ecs_lookup("B"), link);
    ecs_script_t *candidate = ecs_script_parse(
        "identity.si",
        "const inserted = new { Position: {30, 0} }\n"
        "const b = new { Position: {20, 0} }\n"
        "const a = new { /* same declaration */ Position: {10, 0} }\n"
        "A { Link: {$a} }\nB { Link: {$b} }\nC { Link: {$inserted} }",
        NULL,
        NULL
    );
    test_true(candidate != NULL);
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_uint(*(const ecs_entity_t *)ecs_get_cid(ecs_lookup("A"), link), a);
    test_uint(*(const ecs_entity_t *)ecs_get_cid(ecs_lookup("B"), link), b);
    ecs_entity_t c = *(const ecs_entity_t *)ecs_get_cid(ecs_lookup("C"), link);
    test_true(c != a && c != b);
    test_int(*(const float *)ecs_get_cid(a, position), 10);
    test_int(*(const float *)ecs_get_cid(b, position), 20);
    test_int(*(const float *)ecs_get_cid(c, position), 30);
    candidate = ecs_script_parse("identity.si", initial, NULL, NULL);
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_uint(*(const ecs_entity_t *)ecs_get_cid(ecs_lookup("A"), link), a);
    test_uint(*(const ecs_entity_t *)ecs_get_cid(ecs_lookup("B"), link), b);
    test_false(ecs_is_alive(c));
    ecs_script_instance_free(instance);
    test_false(ecs_is_alive(a));
    test_false(ecs_is_alive(b));
    ecs_fini();
}

void Instance_namespace_appears(void) {
    ecs_init();
    ecs_component_t component =
        ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "Future::Value",
                                                                    .fields = "{ int32_t x; }" });
    test_true(component != 0);
    ecs_script_instance_t *instance =
        ecs_script_instance_run("namespace.si", "using Future\nOutput { Value: {8} }", NULL, NULL);
    test_true(instance != NULL);
    test_int(ecs_script_instance_state(instance), EcsScriptPending);
    test_uint(ecs_lookup("Output"), 0);
    ecs_new_named(0, "Unrelated");
    test_int(ecs_script_instance_state(instance), EcsScriptPending);
    ecs_new_named(0, "Future");
    test_int(ecs_script_instance_state(instance), EcsScriptActive);
    test_int(*(const int32_t *)ecs_get_cid(ecs_lookup("Output"), component), 8);
    ecs_script_instance_free(instance);
    ecs_fini();
}

static unsigned schema_publish_calls;
static ecs_entity_t schema_publish_entity;
static ecs_component_t schema_publish_component;

static void schema_published(ecs_observer_event_t *event) {
    test_uint(event->entity, schema_publish_entity);
    test_uint(event->component, schema_publish_component);
    test_flt(*(const double *)event->trigger_data, 12.5);
    test_flt(*(const double *)ecs_get_cid(schema_publish_entity, schema_publish_component), 12.5);
    test_uint(ecs_component_info(schema_publish_component)->size, sizeof(double));
    schema_publish_calls++;
}

void Instance_schema_notifies_once(void) {
    ecs_init();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "publish.si",
        "struct Changing(x: i32)\nOutput { Changing: {7} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    schema_publish_entity = ecs_lookup("Output");
    schema_publish_component = ecs_component_lookup("Changing");
    schema_publish_calls = 0;
    ecs_observer_init(&(ecs_observer_desc_t){
        .on = EcsOnSet,
        .entity = schema_publish_entity,
        .query.components = { { schema_publish_component, EcsIn } },
        .callback = schema_published });
    ecs_script_t *candidate = ecs_script_parse(
        "publish.si",
        "struct Changing(x: f64)\nOutput { Changing: {12.5} }",
        NULL,
        NULL
    );
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    test_uint(schema_publish_calls, 1);
    ecs_script_free(candidate);
    candidate = ecs_script_parse(
        "publish.si",
        "struct Changing(x: i64)\nOutput { Changing: {7} }\nBroken { Missing: {1} }",
        NULL,
        NULL
    );
    test_int(ecs_script_instance_reload(instance, candidate, NULL), -1);
    test_uint(schema_publish_calls, 1);
    test_flt(*(const double *)ecs_get_cid(schema_publish_entity, schema_publish_component), 12.5);
    ecs_script_free(candidate);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_missing_component_recovers(void) {
    setup();
    ecs_remove_cid(source, position);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "missing-component.si",
        "Output { Position: {Source[Position].x, 0} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_int(ecs_script_instance_state(instance), EcsScriptPending);
    test_uint(ecs_lookup("Output"), 0);
    float changed[] = { 12, 0 };
    ecs_set_cid(source, position, changed);
    test_int(ecs_script_instance_state(instance), EcsScriptActive);
    test_flt(output(), 12);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_nearer_name_appears(void) {
    setup();
    ecs_entity_t space = ecs_new_named(0, "Space");
    ecs_script_instance_desc_t desc = { .parent = space };
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "shadow.si",
        "Output { Position: {Source[Position].x, 0} }",
        &desc,
        NULL
    );
    test_true(instance != NULL);
    ecs_entity_t target = ecs_lookup_from(space, "Output");
    test_flt(*(const float *)ecs_get_cid(target, position), 2);
    uint64_t evaluations = ecs_script_instance_stats(instance).evaluations;
    ecs_new_named(space, "Unrelated");
    test_int(ecs_script_flush(NULL), 0);
    test_uint(ecs_script_instance_stats(instance).evaluations, evaluations);
    ecs_sync_begin();
    ecs_entity_t nearer = ecs_new_named(space, "Source");
    float changed[] = { 37, 0 };
    ecs_set_cid(nearer, position, changed);
    ecs_sync_end();
    test_int(ecs_script_flush(NULL), 0);
    test_flt(*(const float *)ecs_get_cid(target, position), 37);
    test_uint(ecs_script_instance_stats(instance).evaluations, evaluations + 1);
    ecs_set_name(nearer, "Renamed");
    test_int(ecs_script_flush(NULL), 0);
    test_flt(*(const float *)ecs_get_cid(target, position), 2);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Instance_prefab_marking_restores(void) {
    ecs_init();
    ecs_component_t marker = ecs_tag_init("Marker");
    ecs_entity_t host = ecs_new_named(0, "Host");
    ecs_entity_t child = ecs_new_named(host, "Child");
    ecs_entity_t grandchild = ecs_new_named(child, "Grandchild");
    ecs_entity_t preserved = ecs_new_named(host, "Preserved");
    ecs_abstract(preserved);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "prefab.si",
        "prefab Host {}\nHost { Marker\nLater {} }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_true(ecs_has(host, Abstract));
    test_true(ecs_has_cid(host, marker));
    test_true(ecs_has(child, Abstract));
    test_true(ecs_has(grandchild, Abstract));
    test_true(ecs_has(ecs_lookup_from(host, "Later"), Abstract));
    ecs_script_t *candidate = ecs_script_parse("prefab.si", "Host {}", NULL, NULL);
    test_true(candidate != NULL);
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_false(ecs_has(host, Abstract));
    test_false(ecs_has(child, Abstract));
    test_false(ecs_has(grandchild, Abstract));
    test_true(ecs_has(preserved, Abstract));
    ecs_script_instance_free(instance);
    test_false(ecs_has(host, Abstract));
    instance = ecs_script_instance_run("prefab.si", "prefab Host {}", NULL, NULL);
    test_true(instance != NULL);
    test_true(ecs_has(grandchild, Abstract));
    ecs_script_instance_free(instance);
    test_false(ecs_has(host, Abstract));
    test_false(ecs_has(child, Abstract));
    test_false(ecs_has(grandchild, Abstract));
    test_true(ecs_has(preserved, Abstract));
    ecs_fini();
}

void Instance_prefab_reparented_descendants_restore(void) {
    ecs_init();
    ecs_entity_t host = ecs_new_named(0, "Host");
    ecs_entity_t before = ecs_new_named(0, "Before");
    ecs_entity_t before_child = ecs_new_named(before, "Child");
    ecs_entity_t after = ecs_new_named(0, "After");
    ecs_entity_t after_child = ecs_new_named(after, "Child");
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "prefab-relations.si",
        "Before { (ChildOf, Host) }\nprefab Host {}\nAfter { (ChildOf, Host) }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_true(ecs_has(before, Abstract));
    test_true(ecs_has(before_child, Abstract));
    test_true(ecs_has(after, Abstract));
    test_true(ecs_has(after_child, Abstract));
    ecs_script_instance_free(instance);
    test_false(ecs_has(host, Abstract));
    test_false(ecs_has(before, Abstract));
    test_false(ecs_has(before_child, Abstract));
    test_false(ecs_has(after, Abstract));
    test_false(ecs_has(after_child, Abstract));
    test_uint(ecs_target_id(before, ecs_rid(ChildOf)), 0);
    test_uint(ecs_target_id(after, ecs_rid(ChildOf)), 0);
    ecs_fini();
}
