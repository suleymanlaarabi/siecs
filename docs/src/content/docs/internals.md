---
title: Internals
description: Implementation notes for contributors.
---

This page is for contributors. User code should include only:

```c
#include <siecs.h>
```

Internal code uses headers in `src/`.

## World Layout

`ecs_world_t` is opaque in the public API. Its internal definition lives in
`src/world_internal.h`.

The world currently owns:

| Field | Role |
| --- | --- |
| `entity_index` | Entity records, generations, and free-list reuse. |
| `component_index` | Component metadata, hooks, and table membership. |
| `table_index` | Archetype tables keyed by component sets. |
| `query_index` | Cached query matches and field pointers. |
| `observer_index` | Registered observers and event ids. |

## Archetype Tables

Adding or removing a component migrates an entity between tables. Shared
component columns are copied into the destination table. Removed columns are
skipped. Newly added component storage starts zeroed until `ecs_set_cid()` copies
the provided value.

Tables cache add and remove edges so repeated migrations do not recompute the
same destination archetypes.

## Query Cache

Queries store matching table ids and cached field pointers. `ecs_iter_t` exposes
`world` and `count` publicly, but its query cache fields are implementation
details and should not be used by user code.

## Distribution

Bake generates the distribution files in `distr/` from the package sources.
Regenerate them with:

```sh
bake rebuild
```

Do not edit `distr/siecs.c` or `distr/siecs.h` manually.
