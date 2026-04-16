#include "../ecs/storage/table_index.h"
#include "../ecs/storage/component_index.h"
#include "../ecs/table.h"
#include <criterion/criterion.h>
#include <stdlib.h>

Test(table_index, basic) {
    ecs_table_index_t map;
    ecs_table_index_init(&map);

    ecs_component_index_t comp_idx;
    ecs_component_index_init(&comp_idx);
    ecs_component_index_create(&comp_idx, "Pos", 8, false);
    ecs_component_index_create(&comp_idx, "Vel", 8, false);

    uint16_t *ids1 = malloc(sizeof(uint16_t)); ids1[0] = 1;
    ecs_type_t type1 = { .ids = ids1, .count = 1 };

    uint16_t *ids2 = malloc(2 * sizeof(uint16_t)); ids2[0] = 1; ids2[1] = 2;
    ecs_type_t type2 = { .ids = ids2, .count = 2 };

    uint16_t *ids3 = malloc(sizeof(uint16_t)); ids3[0] = 1;
    ecs_type_t type3 = { .ids = ids3, .count = 1 };

    uint16_t idx1 = ecs_table_index_get_or_create(&map, type1, &comp_idx);
    uint16_t idx2 = ecs_table_index_get_or_create(&map, type2, &comp_idx);
    uint16_t idx3 = ecs_table_index_get_or_create(&map, type3, &comp_idx);

    cr_assert(idx1 != idx2);
    cr_assert(idx1 == idx3);

    ecs_table_t *t1 = ecs_table_index_at(&map, idx1);
    cr_assert_eq(t1->type.count, 1);
    cr_assert_eq(t1->type.ids[0], 1);

    ecs_table_index_fini(&map);
    ecs_component_index_fini(&comp_idx);
}

Test(table_index, resize) {
    ecs_table_index_t map;
    ecs_table_index_init(&map);

    ecs_component_index_t comp_idx;
    ecs_component_index_init(&comp_idx);

    // Create many types to trigger resize
    for (uint32_t i = 0; i < 100; i++) {
        uint16_t *ids = malloc(sizeof(uint16_t));
        ids[0] = (uint16_t)i;
        ecs_type_t type = { .ids = ids, .count = 1 };

        uint16_t idx = ecs_table_index_get_or_create(&map, type, &comp_idx);
        cr_assert_eq(idx, i);

        // Lookup again with a DIFFERENT allocation because get_or_create takes ownership
        uint16_t *ids_lookup = malloc(sizeof(uint16_t));
        ids_lookup[0] = (uint16_t)i;
        ecs_type_t type_lookup = { .ids = ids_lookup, .count = 1 };

        uint16_t idx_lookup = ecs_table_index_get_or_create(&map, type_lookup, &comp_idx);
        cr_assert_eq(idx_lookup, i);
    }
    ecs_table_index_fini(&map);
    ecs_component_index_fini(&comp_idx);
}
