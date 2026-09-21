---
title: Introduction to ECS
description: A complete mental model for entity component systems, data-oriented design, archetypes, tables, queries, structural changes, and the tradeoffs behind SIECS.
---

An Entity Component System is a way to organize a program around **identity**, **data**, and **data-driven behavior**.

That sentence is short, but an ECS only becomes useful when the consequences are understood. Why separate identity from data? Why store entities with the same components together? Why does adding a component cost more than changing a field? Why are queries able to process whole batches? Why can ECS code be cache-friendly, and when is that claim misleading? Why is composition often easier than inheritance for game state? When is an ECS the wrong architecture?

This section answers those questions before the API manuals begin.

SIECS is an **archetype ECS**. Its core storage model can be summarized as:

```text
entity identity
    ↓
component set
    ↓
archetype
    ↓
table
    ↓
contiguous component columns
    ↓
cached query matches
    ↓
batch iteration
```

The rest of the documentation builds on this chain. If this model is clear, the behavior of components, queries, systems, observers, relations, inheritance, and scheduling becomes much easier to predict.

## ECS vocabulary

The same words are used throughout the SIECS documentation with specific meanings.

| Term | Meaning in this documentation |
| --- | --- |
| **Entity** | A unique, live identity represented by an `ecs_entity_t` handle. The handle does not contain the entity's component data. |
| **Component** | A registered data type that can be attached to entities. A component may contain data or be zero-sized when used as a tag. |
| **Tag** | A zero-sized component used to represent a structural fact such as `Selected` or `Disabled`. |
| **Component set** | The exact set of components owned by an entity. |
| **Archetype** | A unique component set. Entities with the same owned component set share the same archetype. |
| **Table** | SIECS storage for one archetype. Rows represent entities; data-bearing components have columns. |
| **Column** | Contiguous storage for one component type inside one table. |
| **Structural change** | An operation that changes an entity's component set or other table-defining structure, causing it to move to different storage. |
| **Query** | A description of the structure to match. SIECS queries cache matching tables and iterate them as batches. |
| **Iterator / batch** | A view over one matching table at a time, including entity handles and component fields for its rows. |
| **System** | Repeated behavior associated with a query plus scheduling metadata. |
| **Resource** | One typed value associated with the world rather than one value per entity. |
| **Relation** | A directed edge from one entity to another using a registered relation kind. |
| **Deferred operation** | A mutation recorded into SIECS's command buffer and applied later, normally after an iteration boundary. |

These definitions are intentionally narrower than general programming terminology. For example, “table” here means an archetype storage table, not a database table.

## The three separations

An ECS starts by separating concerns that object-oriented designs often combine.

### Identity

An entity answers:

> Which logical thing are we talking about?

In SIECS, `ecs_entity_t` is a 64-bit handle. The runtime uses an entity index and a generation so that a handle for a destroyed entity does not silently become a valid reference to a later entity that reused the same index.

The handle is identity. It is not the entity's position, health, name, transform, inventory, or behavior.

### Data

Components answer:

> What state or facts are attached to this entity?

For example:

```c
Position { x, y }
Velocity { x, y }
Health   { current, max }
Selected // tag
```

The component set can change while the entity's identity remains the same.

### Behavior

Queries and systems answer:

> What operation should run on entities that have a particular shape?

Movement does not need to know whether an entity is a player, enemy, projectile, camera, or moving platform. It can be defined by data requirements:

```text
read/write Position
read       Velocity
```

That change in dependency direction is one of the main reasons to use an ECS.

## A small world, from entities to tables

Suppose a world has these entities:

```text
Player      Position, Velocity, Health
Enemy       Position, Velocity, Health
Tree        Position, Health
Projectile  Position, Velocity
```

The component values are different, but the structural shapes are:

```text
{Position, Velocity, Health}
{Position, Health}
{Position, Velocity}
```

SIECS can store them as:

```text
Table A: {Position, Velocity, Health}
  entities:  [Player][Enemy]
  Position:  [P0]    [P1]
  Velocity:  [V0]    [V1]
  Health:    [H0]    [H1]

Table B: {Position, Health}
  entities:  [Tree]
  Position:  [P2]
  Health:    [H2]

Table C: {Position, Velocity}
  entities:  [Projectile]
  Position:  [P3]
  Velocity:  [V3]
```

A movement query that requires `Position + Velocity` matches Table A and Table C. It does not need to test the Tree row because the table structure already proves that Table B cannot match.

This is the central mechanical advantage of an archetype ECS: **structure is indexed before the inner loop begins**.

## Component values and entity structure are different things

Consider an entity in:

```text
{Position, Velocity}
```

Changing:

```text
Position.x = 50
```

only changes bytes inside the current `Position` column.

Adding `Health` changes the entity's shape:

```text
{Position, Velocity}
        ↓ +Health
{Position, Velocity, Health}
```

The entity therefore belongs to another table. Its identity stays the same, but its storage location changes.

This distinction explains several important ECS rules:

- component pointers should not be treated as permanent identity;
- adding/removing components is more expensive than changing existing values;
- structural mutation during iteration needs special handling;
- frequently changing booleans should not automatically become tags;
- component-set stability affects performance.

SIECS provides a command buffer and defer scopes so structural changes can be recorded while iteration is active and applied at a safe boundary.

## Queries are not filters over an object list

A useful but inaccurate mental model is:

```text
for every entity:
    if entity has Position and Velocity:
        move it
```

An archetype ECS instead aims for:

```text
for every table already known to match Position + Velocity:
    get Position column
    get Velocity column
    process all rows
```

Persistent SIECS queries cache matching table identifiers. Tables may gain or lose rows continuously while the table *shape* remains stable, which makes table-level caching practical.

This is why the number and shape of archetypes matter just as much as the raw number of entities.

## ECS is not one optimization

ECS is often reduced to “cache locality.” That misses most of the design.

An archetype ECS combines several ideas:

1. **Runtime composition** — entity capabilities are represented by component sets rather than one rigid runtime class.
2. **Data-oriented storage** — data used by similar operations can be stored and traversed together.
3. **Structural indexing** — a query can select archetypes before processing rows.
4. **Batch iteration** — systems can work on contiguous arrays instead of repeatedly discovering component locations.
5. **Explicit access** — query terms describe what is read, written, filtered, inherited, or optional.
6. **Dynamic structure** — entities can change component sets while preserving identity.

Cache behavior is an important consequence of these choices, not the complete definition of ECS.

## What belongs in this introduction

This section explains the architecture and cost model. It deliberately does **not** duplicate every feature manual.

For example, the chapter on iteration explains why table-batch iteration exists and how to reason about it. The [Queries](/queries/) manual documents the exact SIECS terms such as `ecs_in`, `ecs_inout`, optional terms, relation terms, ordering, inherited fields, and query lifetime.

Likewise, this introduction explains why structural changes need safe boundaries. The [Systems](/systems/) manual documents exact scheduler behavior.

The rule is:

> Theory explains why the mechanism exists and what its consequences are. Manuals explain how to use the concrete SIECS API.

## Reading order

The chapters are designed to be read in order once, then used independently as references.

1. **[Why ECS](/introduction/why-ecs/)**  
   What problems an ECS is solving, what it changes about program structure, and what costs it introduces.

2. **[Data-oriented design](/introduction/data-oriented-design/)**  
   Access patterns, working sets, hot and cold data, AoS vs SoA, component granularity, cardinality, and lifetimes.

3. **[An entity is not an object](/introduction/entities-are-not-objects/)**  
   Identity, generation safety, names, reuse, storage movement, and why an entity handle is not an object pointer.

4. **[Composition](/introduction/composition/)**  
   Building capabilities from components and tags, avoiding inheritance explosions, and recognizing archetype explosion.

5. **[Archetypes](/introduction/archetypes/)**  
   Exact component sets, subset query matching, archetype graphs, fragmentation, and stable structural shapes.

6. **[Tables and columns](/introduction/tables-and-columns/)**  
   Physical storage, packed rows, contiguous columns, tags, row movement, growth, and pointer validity.

7. **[Structural changes](/introduction/structural-changes/)**  
   Migration, add/remove/set, deferred commands, lifecycle work, batching changes, and structural state machines.

8. **[Iteration](/introduction/iteration/)**  
   Cached table matching, outer/inner loops, fields, read/write intent, optional/filter terms, and random access tradeoffs.

9. **[Cache locality](/introduction/cache-locality/)**  
   Spatial locality, temporal locality, cache lines, bandwidth, pointer chasing, vectorization, and why measurement still matters.

10. **[ECS vs OOP](/introduction/ecs-vs-oop/)**  
    Object-centered and data-centered decomposition, encapsulation, polymorphism, hybrid architectures, and migration from classes.

11. **[When ECS is useful](/introduction/when-to-use-ecs/)**  
    Strong fits, weak fits, warning signs, cost models, and a practical decision checklist.

## How this maps to SIECS

SIECS has some choices that differ from other archetype ECS implementations. The rest of the documentation should describe SIECS rather than pretending every ECS has the same feature set.

Important examples include:

- SIECS exposes a 64-bit entity handle with index and generation fields.
- `ecs_new()` may reuse freed indices; `ecs_new_no_reuse()` / `ecs::entity::create_no_reuse()` provide monotonic allocation when needed.
- SIECS tables keep entity rows packed; removing a non-last row may move the last row into its place.
- Component columns may relocate when table storage grows.
- SIECS queries are table-oriented and maintain query caches; unlike Flecs, SIECS does not expose Flecs's cache-kind matrix of cached/uncached/auto query policies.
- SIECS has an explicit command buffer through `ecs_defer_begin()` / `ecs_defer_end()`, and normal systems are deferred around query iteration.
- Resources are stored separately from per-entity component data and also participate in scheduler access metadata.
- Relations have SIECS-specific storage modes (`Dense`, `ByTarget`, `ByDepth`) that should be explained in their own manual rather than forced into another ECS's relationship model.
- SIECS supports component inheritance policies and an `IsA` mechanism, but these must be documented using SIECS semantics rather than Flecs prefab semantics.

## A final mental model

When reading any SIECS API, ask which layer it operates on:

```text
IDENTITY
    entity handle, liveness, generation, name

STRUCTURE
    component set, tags, relations, archetype, table

DATA
    component columns, resources, relation targets

SELECTION
    query terms, table matches, fields

BEHAVIOR
    systems, observers, modules

EXECUTION
    phases, dependencies, deferred mutations, scheduler
```

Confusing these layers is the source of many ECS mistakes. Keeping them separate is the foundation for the rest of the SIECS documentation.

Continue with **[Why ECS](/introduction/why-ecs/)**.
