#include "siecs.h"
#include "utils.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct ecs_typed_id_slot_t {
    uint16_t value;
    char key[];
} ecs_typed_id_slot_t;

/* Slots are never freed: their address is cached by every binary. */
static sicore_map_t type_slot_names;
static sicore_vec_t type_slots; /* ecs_typed_id_slot_t *; pointees never move */
static atomic_flag type_slots_lock = ATOMIC_FLAG_INIT;

uint16_t *ecs_type_slot(const char *key, uint16_t initial) {
    ecs_assert_not_null(key);
    while (atomic_flag_test_and_set_explicit(&type_slots_lock, memory_order_acquire)) {
    }

    if (!type_slot_names.ctrl) {
        sicore_map_init(&type_slot_names);
        sicore_vec_init(&type_slots, sizeof(ecs_typed_id_slot_t *));
    }
    uint32_t index = sicore_map_get(&type_slot_names, key);
    ecs_typed_id_slot_t *slot;
    if (index == UINT32_MAX) {
        size_t length = strlen(key) + 1;
        slot = malloc(sizeof(ecs_typed_id_slot_t) + length);
        ecs_assert_not_null(slot);
        memcpy(slot->key, key, length);
        slot->value = initial;
        index = type_slots.size;
        sicore_vec_push(&type_slots, &slot, sizeof slot);
        sicore_map_set(&type_slot_names, slot->key, index);
    } else {
        slot = *sicore_vec_get_mut(&type_slots, index, ecs_typed_id_slot_t *);
    }

    atomic_flag_clear_explicit(&type_slots_lock, memory_order_release);
    return &slot->value;
}
