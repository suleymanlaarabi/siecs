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
| `ecs_system_id_t` | System id. Id `0` is reserved. |
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
    ecs_component_hook_t on_add;
    bool is_relation;
    const sireflect_struct_desc_t *struct_desc;
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
typedef enum {
    EcsIn,
    EcsOut,
    EcsInOut,
    EcsFilter,
    EcsNot,
} ecs_term_access_t;

typedef struct {
    ecs_component_t id;
    ecs_term_access_t access;
} ecs_query_term_t;

typedef struct {
    ecs_query_term_t terms[16];
} ecs_query_desc_t;
```

```c
ecs_in(Component);
ecs_out(Component);
ecs_inout(Component);
ecs_filter(Component);
ecs_not(Component);
```

`ecs_field()` returns only `EcsIn`, `EcsOut`, and `EcsInOut` terms in
declaration order.

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

## Systems

```c
typedef enum {
    EcsOnLoad,
    EcsPostLoad,
    EcsPreUpdate,
    EcsOnUpdate,
    EcsPostUpdate,
    EcsPreRender,
    EcsOnRender,
    EcsPostRender,
    EcsPhaseCount,
} ecs_phase_t;
```

`ecs_progress()` runs enabled systems in phase order. `OnPreUpdate`,
`OnUpdate`, `OnPostUpdate`, and `OnRender` are compatibility aliases for the
matching `Ecs*` names.

```c
typedef struct {
    const char *name;
    ecs_query_desc_t query;
    void (*callback)(ecs_iter_t *);
    ecs_phase_t phase;
    ecs_system_id_t after[4];
    bool disabled;
} ecs_system_desc_t;
```

```c
#define ecs_system(world, ...)
ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc);
void ecs_progress(ecs_world_t *world);
void ecs_run_phase(ecs_world_t *world, ecs_phase_t phase);
void ecs_run_system(ecs_world_t *world, ecs_system_id_t system);
void ecs_enable_system(ecs_world_t *world, ecs_system_id_t system, bool enabled);
```

`after` declares systems that must run first in the same phase. System id `0` is
reserved, so an empty `after` array is written as `{ 0 }` or omitted. Dependency
cycles and cross-phase dependencies are debug assertion failures.

Example:

```c
static void Move(ecs_iter_t *it) {
    Position *p = ecs_field(it, 0);
    Velocity *v = ecs_field(it, 1);

    for (uint32_t i = 0; i < it->count; i++) {
        p[i].x += v[i].x;
        p[i].y += v[i].y;
    }
}

ecs_system(world, {
    .name = "Move",
    .phase = EcsOnUpdate,
    .query = { .terms = { ecs_inout(Position), ecs_in(Velocity) } },
    .callback = Move,
});

ecs_progress(world);
```
