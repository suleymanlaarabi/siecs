---
title: SIECS
description: User guide for the SIECS C entity component system.
---

SIECS is a small C ECS library packaged with Bake.

The public user API is exposed by:

```c
#include <siecs.h>
```

The current API is centered around:

| Concept | Role |
| --- | --- |
| World | Owns all ECS storage. Create with `ecs_init()`, destroy with `ecs_fini()`. |
| Entity | `ecs_entity_t` handle created by `ecs_new()`. |
| Component | Registered data type identified by `ecs_component_t`. |
| Query | Cached matcher over archetype tables. |
| Iterator | Batch iterator returned by `ecs_query_iter()`. |
| Observer | Callback triggered for built-in or custom events. |
| Relation | Component that stores an entity target, such as `ChildOf`. |

## What to read first

1. [Getting started](./getting-started/) for a complete minimal program.
2. [Components](./components/) for typed and id-based component access.
3. [Queries](./queries/) for iteration with `ecs_iter_t`.
4. [Observers and events](./observers/) for `OnAdd`, `OnRemove`, `OnSet`, and custom events.
5. [API reference](./reference/api/) for the public symbols.
