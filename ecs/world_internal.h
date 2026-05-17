#pragma once
#include "ecs/storage/component_index.h"
#include "ecs/storage/entity_index.h"
#include "ecs/storage/observer_index.h"
#include "ecs/storage/query_index.h"
#include "ecs/storage/table_index.h"

typedef struct ecs_world_s {
    ecs_entity_index_t entity_index;
    ecs_component_index_t component_index;
    ecs_table_index_t table_index;
    ecs_query_index_t query_index;
    ecs_observer_index_t observer_index;
} ecs_world_t;
