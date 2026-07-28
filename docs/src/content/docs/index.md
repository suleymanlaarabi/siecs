---
title: SIECS Documentation
description: Technical documentation for SIECS, including the C and C++ APIs, storage model, queries, systems, and optional tooling.
---
SIECS is a small archetype ECS library with a compact C23 API, C++23 wrapper,
reflection, modules, and an optional REST explorer for tools.

The public header is `<siecs.h>` in all supported language integrations.

The current API is centered around:

| Concept | Role |
| --- | --- |
| World | Owns all ECS storage. Create with `ecs_init()` or `ecs::init()`, destroy with `ecs_fini()` or `ecs::fini()`. |
| Entity | `ecs_entity_t` handle created by `ecs_new()` or `ecs::entity::create()`. |
| Component | Registered data type identified by `ecs_component_t`. |
| Query | Cached matcher over archetype tables. |
| Iterator | Batch iterator returned by `ecs_query_iter()`. |
| System | Scheduled query callback run by `ecs_progress()`. |
| Observer | Callback triggered for built-in or custom events. |
| Module | Import unit that groups components, systems, and observers. |
| Relation | Component that stores an entity target, such as `ChildOf`. |
| Resource | One typed value stored on the world, not on entities. |

## Getting Started

- [Getting started](./getting-started/) builds a complete minimal program.
- [Cookbook](./cookbook/) collects focused examples.

## Manuals

- [ECS theory](./theory/) and [archetype storage](./archetype-ecs/) explain the
  data model.
- [Entities](./entities/), [components](./components/), and
  [resources](./resources/) cover stored data.
- [Queries](./queries/) and [systems](./systems/) cover iteration and
  scheduling.
- [Observers](./observers/), [relations](./relations/), and
  [inheritance](./inheritance/) cover events and entity relationships.
- [Modules](./modules/) cover reusable registration units.

## Guides and Reference

- [Designing with SIECS](./ecs-design/) documents the runtime's
  tradeoffs.
- [REST explorer](./rest/) documents the optional tooling API.
- [API reference](./reference/api/) lists the public C symbols and the typed C++ wrapper.
