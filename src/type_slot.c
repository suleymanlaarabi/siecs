#include "siecs.h"
#include "utils.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct ecs_type_slot_t {
    struct ecs_type_slot_t *next;
    uint16_t value;
    char key[];
} ecs_type_slot_t;

/* Slots are never freed: their address is cached by every binary. */
static ecs_type_slot_t *type_slots;
static atomic_flag type_slots_lock = ATOMIC_FLAG_INIT;

uint16_t *ecs_type_slot(const char *key, uint16_t initial) {
    ecs_assert_not_null(key);
    while (atomic_flag_test_and_set_explicit(&type_slots_lock, memory_order_acquire)) {
    }

    ecs_type_slot_t *slot = type_slots;
    while (slot && strcmp(slot->key, key) != 0) {
        slot = slot->next;
    }
    if (!slot) {
        size_t length = strlen(key) + 1;
        slot = malloc(sizeof(ecs_type_slot_t) + length);
        ecs_assert_not_null(slot);
        memcpy(slot->key, key, length);
        slot->value = initial;
        slot->next = type_slots;
        type_slots = slot;
    }

    atomic_flag_clear_explicit(&type_slots_lock, memory_order_release);
    return &slot->value;
}
