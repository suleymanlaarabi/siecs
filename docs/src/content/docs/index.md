---
title: SIECS
description: User guide for the SIECS C entity component system.
---
SIECS is a small archetype ECS library with a compact C23 API, C++23 wrapper,
reflection, modules, and an optional REST explorer for tools.

The public user API is exposed by:

The public header is `<siecs.h>` in all supported language integrations.

The current API is centered around:

| Concept | Role |
| --- | --- |
| World | Owns all ECS storage. Create with `ecs_init()`, destroy with `ecs_fini()`. |
| Entity | `ecs_entity_t` handle created by `ecs_new()`. |
| Component | Registered data type identified by `ecs_component_t`. |
| Query | Cached matcher over archetype tables. |
| Iterator | Batch iterator returned by `ecs_query_iter()`. |
| System | Scheduled query callback run by `ecs_progress()`. |
| Observer | Callback triggered for built-in or custom events. |
| Module | Import unit that groups components, systems, and observers. |
| Relation | Component that stores an entity target, such as `ChildOf`. |
| Resource | One typed value stored on the world, not on entities. |

## What to read first

1. [Getting started](./getting-started/) for a complete minimal program.
2. [ECS theory](./theory/) for the storage and iteration model.
3. [Components](./components/) and [resources](./resources/) for data modeling.
4. [Queries](./queries/) for iteration with `ecs_iter_t`.
5. [Systems](./systems/) for scheduled logic.
6. [Observers and events](./observers/) for `EcsOnAdd`, `EcsOnRemove`, `EcsOnSet`, and custom events.
7. [Performance guide](./performance/) before writing hot-path code.
8. [API reference](./reference/api/) for the public symbols.
