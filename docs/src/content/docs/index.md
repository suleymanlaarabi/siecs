---
title: SIECS Documentation
description: Learn SIECS through a compact quickstart, concept manuals, and the public C17 and C++20 API reference.
---

SIECS is an archetype ECS for C17 and C++20. It combines a compact C runtime
with a typed C++ wrapper, contiguous component storage, cached queries, systems,
relations, reflection, and optional tooling.

## Getting started

- [Quickstart](./getting-started/) — learn the model through one compact tour.
- [Building and integrating](./building/) — choose the standalone distribution
  or Bake.
- [Cookbook](./cookbook/) — short, focused patterns for common ECS tasks.

## Manuals

Read the manuals in this order if you are new to ECS:

1. [ECS theory](./theory/) explains entities, components, tables and queries.
2. [Entities](./entities/) and [components](./components/) explain data and
   lifetime.
3. [Queries](./queries/) explain matching and iteration.
4. [Systems](./systems/) explain scheduling and frame logic.

Then continue with the advanced manuals:

- [Archetype storage](./archetype-ecs/)
- [Resources](./resources/)
- [Observers](./observers/)
- [Relations](./relations/)
- [Inheritance](./inheritance/)
- [Modules](./modules/)
- [Designing with SIECS](./ecs-design/)
- [REST explorer](./rest/)

## API reference

- [C and C++ API](./reference/api/)
- [API stability](./reference/stability/)

All examples use the public `<siecs.h>` facade. The C runtime uses C17 and the
typed wrapper uses C++20.
