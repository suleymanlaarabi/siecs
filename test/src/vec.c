#include "../../src/datastructure/vec.h"
#include <siecs_test.h>

void vec_u16_contains_and_remove(void) {
    ecs_vec_t vec;
    ecs_vec_init(&vec, sizeof(uint16_t));
    ecs_vec_push_u16(&vec, 10);
    ecs_vec_push_u16(&vec, 20);
    ecs_vec_push_u16(&vec, 30);

    test_true(ecs_vec_contains_u16(&vec, 20));
    test_false(ecs_vec_contains_u16(&vec, 99));

    ecs_vec_remove_u16(&vec, 20);
    test_assert(vec.size == 2);
    test_true(ecs_vec_contains_u16(&vec, 10));
    test_true(ecs_vec_contains_u16(&vec, 30));
    test_false(ecs_vec_contains_u16(&vec, 20));

    ecs_vec_remove_u16(&vec, 30);
    test_assert(vec.size == 1);
    test_true(ecs_vec_contains_u16(&vec, 10));

    ecs_vec_fini(&vec);
}
