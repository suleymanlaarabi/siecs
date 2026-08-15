#ifndef SIECS_STORAGE_OBSERVER_INDEX_H
#define SIECS_STORAGE_OBSERVER_INDEX_H
#include "../table.h"
#include "query_index.h"
#include "siecs.h"
#include <stdint.h>

typedef struct {
    ecs_event_t event;
    ecs_query_id_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
    ecs_observer_id_t next_module;
    bool enabled;
} ecs_observer_t;

typedef struct {
    sicore_vec_t observers; // ecs_observer_t
    uint16_t event_count;   // next free event id; starts past the builtin events
} ecs_observer_index_t;

extern ecs_observer_index_t observer_index;

void ecs_observer_index_init();
void ecs_observer_index_fini();

#endif
