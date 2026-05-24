---
title: Internals
description: World storage, archetype migration, table edges, and test access.
---

This page documents implementation-level behavior needed for contributors.

## World layout

`ecs_world_t` is opaque publicly. Internal code includes `ecs/world_internal.h`
to access:

| Field | Role |
| --- | --- |
| `entity_index` | Entity records, generations, and free/reuse state. |
| `component_index` | Component metadata: name, size, bitset flag. |
| `table_index` | Archetype tables keyed by component type. |
| `query_index` | Cached query matches. |

## Archetype migration

`ecs_add_cid()` and `ecs_remove_cid()` move entities between tables.

Migration copies columns shared by source and destination tables. The newly
added column starts empty. Removed columns are skipped.

## Table edges

Tables cache add/remove edges:

| Edge | Meaning |
| --- | --- |
| Add edge | Current table plus one component. |
| Remove edge | Current table minus one component. |

Edges avoid recomputing target archetypes for repeated migrations.

## Test rule

Do not redefine opaque structs in tests. Include the internal header instead:

```c
#include "ecs/world_internal.h"
```

This keeps tests aligned with the real struct definition.
