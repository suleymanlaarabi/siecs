---
title: SIECS ECS
description: Architecture and usage guide for the SIECS entity component system.
---

SIECS is a compact C ECS built around entities, components, archetype tables,
queries, and cached table matches.

The public API lives in `ecs/world.h`.

## Core model

| Concept | Role |
| --- | --- |
| World | Owns entities, component metadata, archetype tables, and query cache. |
| Entity | `uint64_t` handle containing an index and generation. |
| Component | Registered data type identified by `ecs_component_t`. |
| Bitset component | Zero-sized boolean tag stored as packed bits per table row. |
| Table | Archetype storage for entities sharing the same component set. |
| Query | Required and excluded component filters over matching tables. |

## What to read first

1. [Getting started](./getting-started/) to create a world and attach data.
2. [Components](./components/) to declare, define, register, add, and remove
   components.
3. [Queries](./queries/) to iterate matching archetype tables.
4. [API reference](./reference/api/) for the public symbols.
