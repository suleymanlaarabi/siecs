---
title: API Reference
description: Public SIECS symbols from siecs.h.
---

Include the public API with:

```c
#include <siecs.h>
```

## Core Types

| Type | Meaning |
| --- | --- |
| `ecs_world_t` | Opaque world object. |
| `ecs_entity_t` | Entity handle. |
| `ecs_component_t` | Component id. |
| `ecs_query_id_t` | Query id. |
| `ecs_system_id_t` | System id. System execution is not stable yet. |
| `ecs_event_t` | Observer event id. |
| `ecs_iter_t` | Query iterator. Users may read `world` and `count`. |

## World

```c
ecs_world_t *ecs_init(void);
void ecs_fini(ecs_world_t *world);
```

## Components

```c
ECS_COMPONENT_DECLARE(Name, { /* fields */ });
ECS_COMPONENT_DEFINE(Name);
ECS_COMPONENT_REGISTER(world, Name);
```

```c
#define ecs_component(world, ...)
ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc);
```

```c
typedef struct {
    const char *name;
    uint64_t size;
    ecs_component_hook_t on_set;
    ecs_component_hook_t on_remove;
    bool is_relation;
    const char *source_name;
} ecs_component_desc_t;
```

## Entities

```c
ecs_entity_t ecs_new(ecs_world_t *world);
int ecs_is_alive(const ecs_world_t *world, ecs_entity_t entity);
void ecs_kill(ecs_world_t *world, ecs_entity_t entity);
```

## Component Access

Typed helpers:

```c
ecs_add(world, entity, Component);
ecs_remove(world, entity, Component);
ecs_has(world, entity, Component);
ecs_get(world, entity, Component);
ecs_try_get(world, entity, Component);
ecs_set(world, entity, Component, { /* value */ });
```

Id-based functions:

```c
void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);
void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);
bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);
void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);
void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid);
void ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id, const void *data);
```

`ecs_get()` and `ecs_get_cid()` assume the component exists. Use
`ecs_try_get()` or `ecs_try_get_cid()` when absence is valid.

```c
void ecs_with(ecs_world_t *world, ecs_component_t component, ecs_component_t require);
```

`ecs_with(world, component, require)` declares that adding `component` also adds
`require` first.

Requirement cycles are invalid and are asserted in debug builds when declared.

## Queries

```c
#define ecs_query(world, ...)
uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *query);
void ecs_query_fini(ecs_world_t *world, ecs_query_id_t qid);
```

```c
typedef struct {
    ecs_component_t read[10];
    ecs_component_t required[8];
    ecs_component_t excluded[6];
} ecs_query_desc_t;
```

```c
ecs_iter_t ecs_query_iter(ecs_world_t *world, ecs_query_id_t query_id);
bool ecs_iter_next(ecs_iter_t *it);
void *ecs_field(ecs_iter_t *it, uint16_t query_term);
```

## Observers

```c
#define OnAdd 0
#define OnRemove 1
#define OnSet 2
```

```c
#define ecs_observer(world, ...)
ecs_event_t ecs_event(ecs_world_t *world);
uint32_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc);
void ecs_observer_trigger(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
);
```

```c
typedef struct {
    ecs_event_t on;
    ecs_query_desc_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
} ecs_observer_desc_t;
```

## Relations

```c
ECS_RELATION_DECLARE(Name);
ECS_RELATION_DEFINE(Name);
ecs_source(Name);
```

`ChildOf` is declared by the public API and registered during world bootstrap.

## Planned API

These symbols are currently declared but should not be treated as stable until
they are implemented and tested:

```c
ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc);
```
