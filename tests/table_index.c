#include "ecs/storage/table_index.h"
#include "ecs/storage/component_index.h"
#include "ecs/table.h"
#include "ecs/world.h"
#include "ecs/world_internal.h"
#include <criterion/criterion.h>
#include <stdlib.h>

Test(table_index, basic) {
    ecs_world_t *world = ecs_init();

    ecs_component(
        world,
        {
            .name = "Pos",
            .size = 8,
        }
    );
    ecs_component(
        world,
        {
            .name = "Vel",
            .size = 8,
        }
    );

    uint16_t *ids1 = malloc(sizeof(uint16_t));
    ids1[0] = 1;
    ecs_type_t type1 = { .ids = ids1, .count = 1 };

    uint16_t *ids2 = malloc(2 * sizeof(uint16_t));
    ids2[0] = 1;
    ids2[1] = 2;
    ecs_type_t type2 = { .ids = ids2, .count = 2 };

    uint16_t *ids3 = malloc(sizeof(uint16_t));
    ids3[0] = 1;
    ecs_type_t type3 = { .ids = ids3, .count = 1 };

    uint16_t idx1 = ecs_table_index_get_or_create(world, type1);
    uint16_t idx2 = ecs_table_index_get_or_create(world, type2);
    uint16_t idx3 = ecs_table_index_get_or_create(world, type3);

    cr_assert(idx1 != idx2);
    cr_assert(idx1 == idx3);

    ecs_table_t *t1 = ecs_table_index_at(&world->table_index, idx1);
    cr_assert_eq(t1->type.count, 1);
    cr_assert_eq(t1->type.ids[0], 1);

    ecs_table_index_fini(&world->table_index);
    ecs_component_index_fini(&world->component_index);
}

Test(table_index, resize) {
    ecs_world_t *world = ecs_init();

    // Create many types to trigger resize
    for (uint32_t i = 0; i < 100; i++) {
        ecs_component(world, {
            .name = NULL,
            .size = 8,
        });

        uint16_t *ids = malloc(sizeof(uint16_t));
        ids[0] = (uint16_t)(i + 1);
        ecs_type_t type = { .ids = ids, .count = 1 };

        uint16_t idx = ecs_table_index_get_or_create(world, type);
        cr_assert_eq(idx, i + 1); // +1 because index 0 is the empty table (created by the ecs_init)

        // Lookup again with a DIFFERENT allocation because get_or_create takes ownership
        uint16_t *ids_lookup = malloc(sizeof(uint16_t));
        ids_lookup[0] = (uint16_t)(i + 1);
        ecs_type_t type_lookup = { .ids = ids_lookup, .count = 1 };

        uint16_t idx_lookup = ecs_table_index_get_or_create(world, type_lookup);
        cr_assert_eq(
            idx_lookup,
            i + 1
        ); // +1 because index 0 is the empty table (created by the ecs_init)
    }
    ecs_fini(world);
}
