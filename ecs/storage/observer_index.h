#pragma once
#include "ecs/datastructure/vec.h"
#include "ecs/storage/query_index.h"
#include "ecs/table.h"
#include "ecs/world.h"
#include <stdint.h>

typedef struct {
    ecs_event_t event;
    ecs_query_t query;
    ecs_observer_callback_t callback;
} ecs_observer_t;

typedef struct {
    ecs_vec_t observers;  // ecs_observer_t
    uint16_t event_count; // next free event id; starts past the builtin events
} ecs_observer_index_t;

void ecs_observer_index_init(ecs_observer_index_t *index);
void ecs_observer_index_fini(ecs_observer_index_t *index);

uint16_t ecs_observer_index_create(ecs_observer_index_t *index, const ecs_observer_desc_t *desc);

// Cache a freshly created observer onto every existing table it matches.
void ecs_observer_index_match_tables(
    ecs_observer_index_t *index,
    ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
);

// Cache every existing observer that matches a freshly created table.
void ecs_observer_index_add_table(ecs_observer_index_t *index, ecs_table_t *table);
