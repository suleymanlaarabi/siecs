#include <siecs_test.h>

static uint32_t fini_call_count;
static int fini_order[3];

static void record_fini(void *data) {
    const DeltaTime *delta_time = ecs_get_resource_read(DeltaTime);

    test_assert(delta_time != NULL);

    fini_order[fini_call_count++] = *(const int *)data;
}

static void increment_fini(void *data) {
    uint32_t *count = data;
    (*count)++;
}

void world_at_fini_runs_all_in_reverse_registration_order(void) {
    int first = 1;
    int second = 2;
    int third = 3;

    fini_call_count = 0;
    fini_order[0] = 0;
    fini_order[1] = 0;
    fini_order[2] = 0;

    ecs_init();

    ecs_at_fini(
        {
            .callback = record_fini,
            .data = &first,
        }
    );

    ecs_at_fini(
        {
            .callback = record_fini,
            .data = &second,
        }
    );

    ecs_at_fini(
        {
            .callback = record_fini,
            .data = &third,
        }
    );

    ecs_fini();

    test_int(fini_call_count, 3);
    test_int(fini_order[0], 3);
    test_int(fini_order[1], 2);
    test_int(fini_order[2], 1);
}

void world_at_fini_is_cleared_on_restart(void) {
    uint32_t calls = 0;

    ecs_init();

    ecs_at_fini(
        {
            .callback = increment_fini,
            .data = &calls,
        }
    );

    ecs_fini();

    test_int(calls, 1);

    ecs_init();
    ecs_fini();

    test_int(calls, 1);
}
