#include "ecs/type.h"
#include <criterion/criterion.h>

Test(type, add_from_empty_type) {
    ecs_type_t empty = { 0 };

    ecs_type_t type = ecs_type_with_add(&empty, 1);

    cr_assert_eq(type.count, 1);
    cr_assert_eq(type.ids[0], 1);

    ecs_type_fini(&type);
}

Test(type, empty_types_are_equal) {
    cr_assert(ecs_type_equals(NULL, 0, NULL, 0));
}
