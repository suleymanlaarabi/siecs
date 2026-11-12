#include "../../../../../src/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <test.h>

static ecs_component_t value_type;
static ecs_entity_t input;
static ecs_script_task_t pending;
static int starts;
static int cancellations;

static void setup(void) {
    ecs_init();
    value_type =
        ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "Value",
                                                                    .fields = "{ int64_t x; }" });
    input = ecs_new_named(0, "Input");
    int64_t zero = 0;
    ecs_set_cid(input, value_type, &zero);
    starts = cancellations = 0;
    pending = 0;
}

static int64_t value(const char *name) {
    ecs_entity_t entity = ecs_lookup(name);
    test_true(entity != 0);
    const int64_t *data = ecs_get_cid(entity, value_type);
    test_true(data != NULL);
    return *data;
}

void Async_wait_for_component_condition(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "await.si",
        "async { await Input[Value].x > 0\nOutput { Value: {42} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(ecs_lookup("Output"), 0);
    int64_t ready = 1;
    ecs_set_cid(input, value_type, &ready);
    test_int(value("Output"), 42);
    test_uint(ecs_script_instance_stats(instance).suspensions, 1);
    test_uint(ecs_script_instance_stats(instance).resumptions, 2);
    ecs_script_instance_free(instance);
    ecs_fini();
}

static void start_task(
    ecs_script_task_t task,
    const ecs_script_value_t *arguments,
    uint32_t count,
    void *context
) {
    (void)arguments;
    (void)context;
    test_uint(count, 0);
    pending = task;
    starts++;
}
static void cancel_task(ecs_script_task_t task, void *context) {
    (void)context;
    test_uint(task, pending);
    cancellations++;
}
static void register_task(void) {
    test_int(
        ecs_script_function_register(&(ecs_script_function_desc_t){
            .name = "fetch",
            .result_type = sireflect_type_by_name("int64_t"),
            .start = start_task,
            .cancel = cancel_task }),
        0
    );
}

#ifdef _WIN32
static DWORD ECS_PLATFORM_THREAD_CALL complete_vector(void *argument) {
#else
static void *complete_vector(void *argument) {
#endif
    test_true(ecs_script_task_complete(pending, argument));
    return 0;
}

void Async_owning_vector_result(void) {
    setup();
    sireflect_handle_t label =
        sireflect_try_register_dynamic_struct("TaskLabel", "{ char *text; }");
    sireflect_handle_t strings =
        sireflect_vector_type(sireflect_type_fields(label)->fields[0].type);
    test_int(
        ecs_script_function_register(&(
            ecs_script_function_desc_t
        ){ .name = "fetch", .result_type = strings, .start = start_task, .cancel = cancel_task }),
        0
    );
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "vector-task.si",
        "struct Label(text: string)\nasync { const labels = await fetch()\n"
        "Output { Label: {$labels[1]} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    sireflect_vector_t invalid = { .count = SIZE_MAX };
    test_false(ecs_script_task_complete(pending, &invalid));
    test_uint(ecs_lookup("Output"), 0);
    char first[] = "alpha", second[] = "beta";
    char *texts[] = { first, second };
    sireflect_vector_t result = { texts, 2 };
    ecs_platform_thread_t worker;
    test_true(ecs_platform_thread_create(&worker, complete_vector, &result));
    ecs_platform_thread_join(&worker);
    second[0] = 'X';
    test_false(ecs_script_task_complete(pending, &result));
    test_int(ecs_script_flush(NULL), 0);
    test_str(
        *(char *const *)ecs_get_cid(ecs_lookup("Output"), ecs_component_lookup("Label")),
        "beta"
    );
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_typed_host_task(void) {
    setup();
    register_task();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "host.si",
        "async { const result = await fetch()\nOutput { Value: {$result} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_int(starts, 1);
    test_uint(ecs_lookup("Output"), 0);
    int64_t result = 99;
    test_true(ecs_script_task_complete(pending, &result));
    test_false(ecs_script_task_complete(pending, &result));
    test_int(ecs_script_flush(NULL), 0);
    test_int(value("Output"), 99);
    ecs_script_instance_free(instance);
    test_int(cancellations, 0);
    ecs_fini();
}

void Async_reload_keeps_identical_continuation(void) {
    setup();
    register_task();
    const char *code = "async { const result = await fetch()\nOutput { Value: {$result} } }";
    ecs_script_instance_t *instance = ecs_script_instance_run("host.si", code, NULL, NULL);
    test_true(instance != NULL);
    test_int(starts, 1);
    ecs_script_task_t retained = pending;
    ecs_script_t *candidate = ecs_script_parse(
        "host.si",
        "// edited comment\nasync { const result = await fetch()\nOutput { Value: {$result} } }",
        NULL,
        NULL
    );
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_int(starts, 1);
    test_uint(pending, retained);
    test_int(cancellations, 0);
    int64_t result = 17;
    test_true(ecs_script_task_complete(retained, &result));
    test_int(ecs_script_flush(NULL), 0);
    test_int(value("Output"), 17);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_cancellation_and_late_result(void) {
    setup();
    register_task();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "host.si",
        "async { const result = await fetch()\nOutput { Value: {$result} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_script_task_t cancelled = pending;
    ecs_script_t *candidate = ecs_script_parse("host.si", "Replacement {}", NULL, NULL);
    test_int(ecs_script_instance_reload(instance, candidate, NULL), 0);
    ecs_script_free(candidate);
    test_int(cancellations, 1);
    int64_t result = 7;
    test_false(ecs_script_task_complete(cancelled, &result));
    ecs_script_instance_free(instance);
    test_int(cancellations, 1);
    ecs_fini();
}

void Async_loops_and_zero_delay(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "loop.si",
        "async { for i in 0..3 { await sleep(0)\n\"Item_$i\" { Value: {$i} } } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    for (int i = 0; i < 5; i++)
        test_int(ecs_script_flush(NULL), 0);
    test_int(value("Item_0"), 0);
    test_int(value("Item_2"), 2);
    ecs_script_instance_free(instance);
    ecs_fini();
}

static double controlled_time(void *context) { return *(double *)context; }

void Async_iteration_boundaries_and_collections(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "iteration.si",
        "async {\n"
        "for i in 2147483647..2147483649 { await sleep(0)\n"
        "\"High_$i\" { Value: {$i} } }\n"
        "for i in -2147483650..-2147483648 { await sleep(0)\n"
        "\"Low_$i\" { Value: {$i} } }\n"
        "for (index, item) in [11, 22] { await sleep(0)\n"
        "if $index == 0 { continue }\n\"Item_$index\" { Value: {$item} } }\n"
        "for i in 5..2 { Unexpected {} }\n"
        "for (index, item) in [] { Unexpected {} }\n}",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    for (int i = 0; i < 12; i++)
        test_int(ecs_script_flush(NULL), 0);
    test_int(value("High_2147483648"), 2147483648LL);
    test_int(value("Low_-2147483650"), -2147483650LL);
    test_int(value("Item_1"), 22);
    test_uint(ecs_lookup("Item_0"), 0);
    test_uint(ecs_lookup("Unexpected"), 0);
    test_int(ecs_script_instance_state(instance), EcsScriptActive);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_controlled_clock(void) {
    setup();
    double now = 100;
    test_int(ecs_script_clock_set(controlled_time, &now), 0);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "clock.si",
        "async { await sleep(2)\nEarly { Value: {2} } }\n"
        "async { await sleep(5)\nLate { Value: {5} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    now = 101.5;
    test_int(ecs_script_flush(NULL), 0);
    test_uint(ecs_lookup("Early"), 0);
    now = 102;
    test_int(ecs_script_flush(NULL), 0);
    test_int(value("Early"), 2);
    test_uint(ecs_lookup("Late"), 0);
    uint64_t resumptions = ecs_script_instance_stats(instance).resumptions;
    now = NAN;
    test_int(ecs_script_flush(NULL), 0);
    now = 99;
    test_int(ecs_script_flush(NULL), 0);
    test_uint(ecs_script_instance_stats(instance).resumptions, resumptions);
    test_uint(ecs_lookup("Late"), 0);
    now = 105;
    test_int(ecs_script_flush(NULL), 0);
    test_int(value("Late"), 5);
    test_uint(ecs_script_instance_stats(instance).resumptions, resumptions + 1);
    ecs_script_instance_free(instance);
    test_int(ecs_script_clock_set(NULL, NULL), 0);
    ecs_fini();
}

void Async_clock_switch_preserves_remaining_delay(void) {
    setup();
    double now = 10;
    test_int(ecs_script_clock_set(controlled_time, &now), 0);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "clock-switch.si",
        "async { await sleep(5)\nOutput { Value: {42} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    now = 11;
    double other = NAN;
    test_int(ecs_script_clock_set(controlled_time, &other), -1);
    other = 1000;
    test_int(ecs_script_clock_set(controlled_time, &other), 0);
    other = 1003.5;
    test_int(ecs_script_flush(NULL), 0);
    test_uint(ecs_lookup("Output"), 0);
    other = 1004;
    test_int(ecs_script_flush(NULL), 0);
    test_int(value("Output"), 42);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_inherited_templates(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "async-inheritance.si",
        "template Base { prop x: i64 = 4\nValue: {$x} }\n"
        "template Derived: Base { prop y: i64 = 8\nChild { Value: {$y} } }\n"
        "async { await Input[Value].x > 0\nDerived Output() }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(ecs_lookup("Output"), 0);
    int64_t ready = 1;
    ecs_set_cid(input, value_type, &ready);
    test_int(value("Output"), 4);
    test_int(value("Output::Child"), 8);
    test_true(ecs_has_cid(ecs_lookup("Output"), ecs_component_lookup("Base")));
    test_true(ecs_has_cid(ecs_lookup("Output"), ecs_component_lookup("Derived")));
    ecs_script_instance_free(instance);
    test_uint(ecs_lookup("Output"), 0);
    ecs_fini();
}

static ecs_script_t *environment_program(int offset, int initial, int scoped, bool comment) {
    char source[1024];
    snprintf(
        source,
        sizeof source,
        "fn adjust(n: i64) -> i64 { $n + %d }\n%s\n"
        "template Base { prop x: i64 = %d\nValue: {$x} }\n"
        "template Derived: Base {}\n"
        "with Value: {%d} { async { const result = await fetch()\n"
        "Derived Output()\nAdjusted { Value: {adjust($result)} }\nDefaulted {} } }",
        offset,
        comment ? "// comment-only reload" : "",
        initial,
        scoped
    );
    return ecs_script_parse("environment.si", source, NULL, NULL);
}

void Async_environment_changes_restart_tasks(void) {
    setup();
    register_task();
    ecs_script_t *program = environment_program(1, 1, 3, false);
    test_true(program != NULL);
    ecs_script_instance_t *instance = ecs_script_instance_new(program, NULL, NULL);
    ecs_script_free(program);
    test_true(instance != NULL);
    test_int(starts, 1);
    ecs_script_task_t first = pending;
    program = environment_program(1, 1, 3, true);
    test_true(program != NULL);
    test_int(ecs_script_instance_reload(instance, program, NULL), 0);
    ecs_script_free(program);
    test_int(starts, 1);
    test_int(cancellations, 0);
    test_uint(pending, first);
    const int changes[][3] = { { 1, 2, 3 }, { 2, 2, 3 }, { 2, 2, 4 } };
    for (uint32_t i = 0; i < 3; i++) {
        ecs_script_task_t previous = pending;
        program = environment_program(changes[i][0], changes[i][1], changes[i][2], true);
        test_true(program != NULL);
        test_int(ecs_script_instance_reload(instance, program, NULL), 0);
        ecs_script_free(program);
        test_int(starts, (int)i + 2);
        test_int(cancellations, (int)i + 1);
        test_true(pending != previous);
        int64_t late = 99;
        test_false(ecs_script_task_complete(previous, &late));
        test_uint(ecs_lookup("Output"), 0);
    }
    int64_t result = 10;
    test_true(ecs_script_task_complete(pending, &result));
    test_int(ecs_script_flush(NULL), 0);
    test_int(value("Output"), 2);
    test_int(value("Adjusted"), 12);
    test_int(value("Defaulted"), 4);
    ecs_script_instance_free(instance);
    test_int(cancellations, 3);
    ecs_fini();
}

static ecs_script_t *definition_functions_program(int offset) {
    char code[1024];
    snprintf(
        code,
        sizeof code,
        "const child = new {}\nfn target() -> entity { $child }\n"
        "fn schedule(n: i64) -> i64 { async { await Input[Value].x > 0\n"
        "Derived Output()\nLinked { Link: {target()} } }\n$n }\n"
        "fn helper(n: i64) -> i64 { $n + %d }\n"
        "template Base { prop x: i64 = helper(2)\nValue: {$x} }\n"
        "template Derived: Base {}\nconst ignored = schedule(0)",
        offset
    );
    return ecs_script_parse("definition-functions.si", code, NULL, NULL);
}

void Async_definition_functions(void) {
    setup();
    ecs_component_t link = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Link", .fields = "{ ecs_entity_t target; }" }
    );
    ecs_script_t *program = definition_functions_program(5);
    test_true(program != NULL);
    ecs_script_instance_t *instance = ecs_script_instance_new(program, NULL, NULL);
    ecs_script_free(program);
    test_true(instance != NULL);
    program = definition_functions_program(6);
    test_true(program != NULL);
    test_int(ecs_script_instance_reload(instance, program, NULL), 0);
    ecs_script_free(program);
    test_uint(ecs_lookup("Output"), 0);
    int64_t ready = 1;
    ecs_set_cid(input, value_type, &ready);
    test_int(value("Output"), 8);
    ecs_entity_t linked = ecs_lookup("Linked");
    const ecs_entity_t *target = ecs_get_cid(linked, link);
    test_true(target != NULL);
    ecs_entity_t child = *target;
    test_true(ecs_is_alive(child));
    test_true(child != linked);
    test_true(child != ecs_lookup("Output"));
    ecs_script_instance_free(instance);
    test_false(ecs_is_alive(child));
    test_uint(ecs_lookup("Output"), 0);
    ecs_fini();
}

void Async_scoped_functions(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "scoped-functions.si",
        "fn number() -> i64 { 3 }\nwith Value: {number()} {\n"
        "fn number() -> i64 { 99 }\nasync { await Input[Value].x > 0\n"
        "Defaulted {}\nExplicit { Value: {number()} } } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    int64_t ready = 1;
    ecs_set_cid(input, value_type, &ready);
    test_int(value("Defaulted"), 3);
    test_int(value("Explicit"), 99);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_typed_errors_and_catch(void) {
    setup();
    register_task();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "failure.si",
        "async { try { const result = await fetch()\nOutput { Value: {$result} } }\n"
        "catch(NetworkError) { Fallback { Value: {12} } } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_int(starts, 1);
    test_true(ecs_script_task_fail(pending, "NetworkError", "offline"));
    test_int(ecs_script_flush(NULL), 0);
    test_uint(ecs_lookup("Output"), 0);
    test_int(value("Fallback"), 12);
    test_int(ecs_script_instance_state(instance), EcsScriptActive);
    ecs_script_instance_free(instance);
    test_uint(ecs_lookup("Fallback"), 0);
    ecs_fini();
}

void Async_unhandled_error_is_inspectable(void) {
    setup();
    register_task();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "failure.si",
        "async { const result = await fetch()\nOutput { Value: {$result} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_true(ecs_script_task_fail(pending, "NetworkError", "offline"));
    test_int(ecs_script_flush(NULL), -1);
    test_int(ecs_script_instance_state(instance), EcsScriptFailed);
    test_true(ecs_script_instance_diagnostic(instance)->error != NULL);
    test_uint(ecs_lookup("Output"), 0);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_scopes_and_while_continue(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "scopes.si",
        "async { mut index: i32 = 0\nwith Value: {3} { while $index < 3 {\n"
        "index = $index + 1\nawait sleep(0)\nif $index < 3 { continue }\n"
        "Output {} } } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    for (int i = 0; i < 5; i++)
        test_int(ecs_script_flush(NULL), 0);
    test_int(value("Output"), 3);
    ecs_script_instance_free(instance);
    test_uint(ecs_lookup("Output"), 0);
    ecs_fini();
}

void Async_repeated_world_lifetimes(void) {
    for (int i = 0; i < 3; i++) {
        setup();
        register_task();
        ecs_script_instance_t *instance = ecs_script_instance_run(
            "host.si",
            "async { const result = await fetch()\nOutput { Value: {$result} } }",
            NULL,
            NULL
        );
        test_true(instance != NULL);
        int64_t result = i + 1;
        test_true(ecs_script_task_complete(pending, &result));
        test_int(ecs_script_flush(NULL), 0);
        test_int(value("Output"), i + 1);
        ecs_script_instance_free(instance);
        test_uint(ecs_lookup("Output"), 0);
        ecs_fini();
    }
}

void Async_member_outputs_across_suspensions(void) {
    setup();
    ecs_component_t pair = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Pair", .fields = "{ int64_t x; int64_t y; }" }
    );
    ecs_entity_t target = ecs_new_named(0, "Output");
    int64_t initial[] = { 10, 20 };
    ecs_set_cid(target, pair, initial);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "members.si",
        "async { Output { Pair = {x: 1} }\nawait Input[Value].x > 0\n"
        "Output { Pair = {y: 2} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    int64_t host[] = { 99, 20 };
    ecs_set_cid(target, pair, host);
    int64_t ready = 1;
    ecs_set_cid(input, value_type, &ready);
    const int64_t *data = ecs_get_cid(target, pair);
    test_int(data[0], 99);
    test_int(data[1], 2);
    ecs_script_eval_result_t result = { 0 };
    ecs_script_instance_t *conflict =
        ecs_script_instance_run("conflict.si", "Output { Pair = {y: 3} }", NULL, &result);
    test_true(conflict == NULL);
    test_true(result.error != NULL);
    free(result.error);
    ecs_script_instance_free(instance);
    data = ecs_get_cid(target, pair);
    test_int(data[0], 99);
    test_int(data[1], 20);
    ecs_fini();
}

void Async_waits_on_inherited_value(void) {
    setup();
    ecs_component_t shared = ecs_component_dynamic_init(&(
        ecs_dynamic_component_desc_t
    ){ .name = "Shared", .fields = "{ int64_t x; }", .inheritance = EcsInheritShared });
    ecs_entity_t base = ecs_new();
    int64_t zero = 0;
    ecs_set_cid(base, shared, &zero);
    ecs_is_a(input, base);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "inherited.si",
        "async { await Input[Shared].x > 0\nOutput { Value: {42} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(ecs_lookup("Output"), 0);
    int64_t ready = 1;
    ecs_set_cid(base, shared, &ready);
    test_int(value("Output"), 42);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_stabilizes_reactive_consumers(void) {
    setup();
    ecs_script_instance_t *consumer = ecs_script_instance_run(
        "consumer.si",
        "Consumer { Value: {Output[Value].x + 1} }",
        NULL,
        NULL
    );
    test_true(consumer != NULL);
    test_int(ecs_script_instance_state(consumer), EcsScriptPending);
    ecs_script_instance_t *producer = ecs_script_instance_run(
        "producer.si",
        "async { await Input[Value].x > 0\nOutput { Value: {42} } }",
        NULL,
        NULL
    );
    test_true(producer != NULL);
    int64_t ready = 1;
    ecs_set_cid(input, value_type, &ready);
    test_int(value("Output"), 42);
    test_int(value("Consumer"), 43);
    test_int(ecs_script_instance_state(consumer), EcsScriptActive);
    ecs_script_instance_free(consumer);
    ecs_script_instance_free(producer);
    ecs_fini();
}

void Async_namespace_imports_across_suspension(void) {
    setup();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "imports.si",
        "First { struct Item(x: i64)\nSource { Item: {10} } }\n"
        "Second { struct Item(x: i64)\nSource { Item: {20} } }\n"
        "using First\n"
        "fn read() -> i64 { Source[Item].x }\n"
        "template Model { prop x: i64 = read()\nItem: {$x} }\n"
        "async {\n"
        " if true { using Second\nawait Input[Value].x > 0\n"
        "   Local { Item: {Source[Item].x} }\n }\n"
        " with Item: {Source[Item].x} {\n"
        "   await Input[Value].x > 1\nDefault {}\nModel Object()\n }\n"
        " Result { Value: {read()} }\n}",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_component_t first = ecs_component_lookup("First::Item");
    ecs_component_t second = ecs_component_lookup("Second::Item");
    int64_t ready = 1;
    ecs_set_cid(input, value_type, &ready);
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Local"), second), 20);
    test_false(ecs_has_cid(ecs_lookup("Local"), first));
    ready = 2;
    ecs_set_cid(input, value_type, &ready);
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Default"), first), 10);
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Object"), first), 10);
    test_int(value("Result"), 10);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_inline_owning_capture(void) {
    setup();
    double now = 0;
    test_int(ecs_script_clock_set(controlled_time, &now), 0);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "capture.si",
        "struct Inner(text: string)\nstruct Outer(inner: Inner)\n"
        "Source { Outer: {{\"captured text\"}} }\n"
        "const inner = Source[Outer].inner\n"
        "async { await sleep(1)\nOutput { Inner: {$inner.text} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_uint(ecs_lookup("Output"), 0);
    now = 1;
    test_int(ecs_script_flush(NULL), 0);
    const char *const *text = ecs_get_cid(ecs_lookup("Output"), ecs_component_lookup("Inner"));
    test_true(text != NULL);
    test_str(*text, "captured text");
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Async_missing_component_recovers(void) {
    setup();
    ecs_remove_cid(input, value_type);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "missing-component.si",
        "async { Output { Value: {Input[Value].x} } }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    test_int(ecs_script_instance_state(instance), EcsScriptPending);
    test_uint(ecs_lookup("Output"), 0);
    int64_t ready = 42;
    ecs_set_cid(input, value_type, &ready);
    test_int(ecs_script_flush(NULL), 0);
    test_int(value("Output"), 42);
    test_int(ecs_script_instance_state(instance), EcsScriptActive);
    ecs_script_instance_free(instance);
    ecs_fini();
}
