---
title: API Reference
description: Public ECS functions, macros, and types from ecs/world.h.
---

## Types

| Type | Description |
| --- | --- |
| `ecs_world_t` | Opaque world handle. |
| `ecs_entity_t` | Entity handle. |
| `ecs_component_t` | Component ID. |
| `ecs_query_id_t` | Query ID. |
| `ecs_component_desc_t` | Component registration descriptor. |
| `ecs_query_desc_t` | Query descriptor with required/excluded arrays. |
| `ecs_iter_t` | Query iterator state. |

## World

```c
ecs_world_t *ecs_init(void);
void ecs_fini(ecs_world_t *world);
```

## Components

```c
ECS_COMPONENT_DECLARE(Name, { /* fields */ })
ECS_COMPONENT_DEFINE(Name)
ECS_BIT_DEFINE(Name)
ECS_COMPONENT_REGISTER(world, Name)

ecs_component_t ecs_component_init(
    ecs_world_t *world,
    const ecs_component_desc_t *desc
);
```

## Entities

```c
ecs_entity_t ecs_new(ecs_world_t *world);
int ecs_is_alive(ecs_world_t *world, ecs_entity_t entity);
void ecs_kill(ecs_world_t *world, ecs_entity_t entity);
```

## Component access

```c
void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);
void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);
bool ecs_has_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);
void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);
```

Typed wrappers:

```c
ecs_add(world, entity, ComponentName);
ecs_remove(world, entity, ComponentName);
ecs_has(world, entity, ComponentName);
ecs_get(world, entity, ComponentName);
```

## Bitsets

```c
void ecs_set_bit(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t id,
    bool value
);

bool ecs_get_bit(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t id
);
```

## Queries

```c
ecs_query_id_t ecs_query_init(
    ecs_world_t *world,
    const ecs_query_desc_t *query
);

ecs_iter_t ecs_query_iter(ecs_world_t *world, ecs_query_id_t query_id);
bool ecs_iter_next(ecs_iter_t *it);
struct ecs_table_s *ecs_iter_table(ecs_iter_t *it);
void *ecs_field(ecs_iter_t *it, ecs_component_t cid);
```

Macro wrapper:

```c
ecs_query(world, {
    .required = { ecs_id(Position), ecs_id(Velocity) },
    .excluded = { ecs_id(Hidden) },
});
```

## Relationships

```c
void ecs_with(
    ecs_world_t *world,
    ecs_component_t component,
    ecs_component_t require
);
```

`ecs_with()` declares that one component requires another component.
