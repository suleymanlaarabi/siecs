#include "../../src/datastructure/string.h"
#include <siecs_test.h>
#include <string.h>

void string_init(void) {
    ecs_str_t str;
    ecs_str_init(&str);
    test_assert(str.data == NULL);
    test_assert(str.len == 0);
    test_assert(str.capacity == 0);
    ecs_str_fini(&str);
}

void string_append(void) {
    ecs_str_t str = ecs_str_new();
    ecs_str_char_append(&str, 'h');
    ecs_str_char_append(&str, 'e');
    ecs_str_char_append(&str, 'l');
    ecs_str_char_append(&str, 'l');
    ecs_str_char_append(&str, 'o');
    test_assert(strcmp(ecs_str_cstr(&str), "hello") == 0);
    test_assert(str.len == 5);

    ecs_str_t str2 = ecs_str_from_cstr(" world");
    ecs_str_str_append(&str, &str2);
    test_assert(strcmp(ecs_str_cstr(&str), "hello world") == 0);
    test_assert(str.len == 11);

    ecs_str_fini(&str);
    ecs_str_fini(&str2);
}

void string_trim(void) {
    ecs_str_t str = ecs_str_from_cstr("  hello  ");
    ecs_str_trim(&str);
    test_assert(strcmp(ecs_str_cstr(&str), "hello") == 0);
    test_assert(str.len == 5);
    ecs_str_fini(&str);
}

void string_starts_ends_with(void) {
    ecs_str_t str = ecs_str_from_cstr("hello world");
    ecs_str_t prefix = ecs_str_from_cstr("hello");
    ecs_str_t suffix = ecs_str_from_cstr("world");
    ecs_str_t fail = ecs_str_from_cstr("nope");

    test_true(ecs_str_starts_with(&str, &prefix));
    test_true(ecs_str_ends_with(&str, &suffix));
    test_false(ecs_str_starts_with(&str, &fail));

    ecs_str_fini(&str);
    ecs_str_fini(&prefix);
    ecs_str_fini(&suffix);
    ecs_str_fini(&fail);
}
