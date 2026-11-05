#include "backend/transaction.h"
#include <stdlib.h>
#include <test.h>

static ecs_component_t position;
static int notifications;

static void committed(ecs_observer_event_t *event) {
    test_uint(event->component, position);
    test_int(event->event, EcsOnSet);
    const float *left = ecs_get_cid(ecs_lookup("Left"), position);
    const float *right = ecs_get_cid(ecs_lookup("Right"), position);
    test_true(left != NULL);
    test_true(right != NULL);
    test_flt(left[0], 10);
    test_flt(right[0], 20);
    test_true(event->entity == ecs_lookup("Left") || event->entity == ecs_lookup("Right"));
    notifications++;
}

static int prepare(script_transaction_t *transaction, const char *code) {
    ecs_script_eval_result_t result = { 0 };
    ecs_script_t *program = ecs_script_parse("transaction.si", code, NULL, &result);
    test_true(program != NULL);
    script_transaction_init(transaction);
    script_transaction_active = transaction;
    int status = ecs_script_eval(program, NULL, &result);
    script_transaction_active = NULL;
    free(result.error);
    ecs_script_free(program);
    return status;
}

void Transaction_failed_prepare_is_invisible(void) {
    ecs_init();
    uint32_t components = ecs_component_count();
    script_transaction_t transaction;
    test_int(
        prepare(&transaction, "struct Staged(x: f32)\nLeft { Staged: {10} }\nRight { Missing }"),
        -1
    );
    test_uint(ecs_lookup("Left"), 0);
    test_uint(ecs_lookup("Right"), 0);
    test_uint(ecs_component_lookup("Staged"), 0);
    test_uint(ecs_component_count(), components);
    script_transaction_fini(&transaction);
    test_uint(sireflect_type_by_name("Staged"), SIREFLECT_INVALID_HANDLE);
    ecs_fini();
}

void Transaction_commit_publishes_complete_state(void) {
    ecs_init();
    position =
        ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "Position",
                                                                    .fields = "{ float x; }" });
    notifications = 0;
    ecs_observer_init(&(ecs_observer_desc_t){ .on = EcsOnSet,
                                              .query.components = { { position, EcsIn } },
                                              .callback = committed });
    script_transaction_t transaction;
    test_int(prepare(&transaction, "Left { Position: {10} }\nRight { Position: {20} }"), 0);
    test_int(notifications, 0);
    test_uint(ecs_lookup("Left"), 0);
    test_int(script_transaction_commit(&transaction), 0);
    test_int(notifications, 2);
    script_transaction_fini(&transaction);
    ecs_fini();
}

void Transaction_prepared_reads_and_native_types(void) {
    ecs_init();
    script_transaction_t transaction;
    test_int(
        prepare(
            &transaction,
            "struct Staged(x: f32)\nSource { Staged: {7} }\n"
            "const source = Source[Staged]\nTarget { Staged: {$source.x + 1} }"
        ),
        0
    );
    test_uint(ecs_component_lookup("Staged"), 0);
    test_int(script_transaction_commit(&transaction), 0);
    ecs_component_t component = ecs_component_lookup("Staged");
    const float *value = ecs_get_cid(ecs_lookup("Target"), component);
    test_true(value != NULL);
    test_flt(value[0], 8);
    script_transaction_fini(&transaction);
    ecs_fini();
}
