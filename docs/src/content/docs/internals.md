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
| `system_index` | Registered systems and per-phase execution plans. |
| `module_index` | Imported modules and their captured ids. |
| `active_module` | Current import scope used to capture registrations. |
| `sireflect_registry` | Reflection registry owned by the world. |
| `server` | REST server state when the REST feature is enabled. |

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

## Modules

During `ecs_module_init()`, the world stores the active module id. Component,
system, and observer registration append their ids to that active module.

This capture happens only during registration. Runtime paths use the existing
system and observer enabled flags, so modules do not add work to query
iteration or table matching.

## Build Sources

SIECS is built from the package sources under `src/` and the public headers
under `include/`. Generated build cache files should not be edited by hand.
