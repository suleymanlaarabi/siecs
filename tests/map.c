#include "ecs/datastructure/map.h"
#include <criterion/criterion.h>
#include <stdint.h>
#include <stdio.h>

Test(map, empty_map_has_no_keys) {
    ecs_map_t map = { 0 };

    cr_assert_not(ecs_map_has(&map, "missing"));
    cr_assert_eq(ecs_map_get(&map, "missing"), UINT32_MAX);

    ecs_map_fini(&map);
}

Test(map, set_get_and_has_key) {
    ecs_map_t map = { 0 };
    ecs_map_init(&map, 0);

    ecs_map_set(&map, "Position", 42);

    cr_assert(ecs_map_has(&map, "Position"));
    cr_assert_eq(ecs_map_get(&map, "Position"), 42);
    cr_assert_not(ecs_map_has(&map, "Velocity"));

    ecs_map_fini(&map);
}

Test(map, overwrite_keeps_len_and_updates_value) {
    ecs_map_t map = { 0 };
    ecs_map_init(&map, 0);

    ecs_map_set(&map, "Position", 1);
    ecs_map_set(&map, "Position", 99);

    cr_assert_eq(map.len, 1);
    cr_assert_eq(ecs_map_get(&map, "Position"), 99);

    ecs_map_fini(&map);
}

Test(map, same_text_different_pointer_matches) {
    ecs_map_t map = { 0 };
    char key_a[] = "Position";
    char key_b[] = "Position";
    ecs_map_init(&map, 0);

    ecs_map_set(&map, key_a, 7);

    cr_assert_neq(key_a, key_b);
    cr_assert(ecs_map_has(&map, key_b));
    cr_assert_eq(ecs_map_get(&map, key_b), 7);

    ecs_map_fini(&map);
}

Test(map, grows_and_preserves_entries) {
    ecs_map_t map = { 0 };
    char keys[64][16];
    ecs_map_init(&map, 1);

    for (uint32_t i = 0; i < 64; i++) {
        snprintf(keys[i], sizeof(keys[i]), "key-%u", i);
        ecs_map_set(&map, keys[i], i + 10);
    }

    cr_assert_geq(map.cap, 128);
    cr_assert_eq(map.len, 64);
    for (uint32_t i = 0; i < 64; i++) {
        cr_assert_eq(ecs_map_get(&map, keys[i]), i + 10);
    }

    ecs_map_fini(&map);
}

Test(map, uint32_max_is_reserved_missing_value) {
    ecs_map_t map = { 0 };
    ecs_map_init(&map, 0);

    ecs_map_set(&map, "sentinel", UINT32_MAX);

    cr_assert_eq(ecs_map_get(&map, "sentinel"), UINT32_MAX);
    cr_assert_not(
        ecs_map_has(&map, "sentinel"),
        "UINT32_MAX is currently indistinguishable from missing"
    );

    ecs_map_fini(&map);
}
