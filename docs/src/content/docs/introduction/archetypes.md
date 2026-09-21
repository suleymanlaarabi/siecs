---
title: Archetypes
description: Understand exact component sets, table identity, query subset matching, structural transitions, fragmentation, and the cost model of archetype ECS storage.
---

An archetype is an **exact structural shape**.

For ordinary component storage, two entities share an archetype when they own the same set of component kinds, regardless of the values stored in those components.

Archetypes are the bridge between ECS composition and physical table storage.

## Archetype means exact component set

Suppose:

```text
A = Position, Velocity
B = Position, Velocity
C = Position, Health
D = Position, Velocity, Health
```

Then:

```text
archetype(A) = archetype(B)
archetype(A) != archetype(C)
archetype(A) != archetype(D)
```

Even if A and B have completely different positions and velocities, their **structure** is identical.

Values do not participate in archetype identity.

## Component order does not define a different shape

Conceptually:

```text
{Position, Velocity}
```

and:

```text
{Velocity, Position}
```

represent the same set.

The runtime uses a canonical structural representation so there is one table for one structural type rather than duplicate tables created by API call order.

The application should think in sets, not component insertion sequences.

## Archetypes become tables

SIECS stores one table per structural type.

Conceptually:

```text
Archetype: {Position, Velocity}

Table:
    entities[]
    Position[]
    Velocity[]
```

Every row has owned `Position` and `Velocity` storage.

This invariant is what lets a query process a whole table without asking every entity whether those components exist.

## Exact archetypes, subset queries

An archetype is exact, but most queries match a subset.

A query requiring:

```text
Position + Velocity
```

matches:

```text
{Position, Velocity}
{Position, Velocity, Health}
{Position, Velocity, Renderable}
```

but not:

```text
{Position}
{Velocity, Health}
```

In set notation:

```text
required(query) ⊆ components(archetype)
```

Additional terms can exclude components, match optional data, include relations, inheritance, or ordering constraints, but subset matching is the core structural rule.

## Why matching tables is powerful

If the world contains 100,000 entities but only 12 archetype tables, a query can reason about 12 structural shapes before iterating entity rows.

If four tables match, the query processes those four batches.

That is fundamentally different from performing a membership lookup for every entity on every iteration.

The benefit becomes especially strong for persistent system queries that run repeatedly.

## Archetypes are created by actual combinations

You do not normally predeclare every archetype as a class.

They emerge from structural operations.

Start with:

```text
{}
```

Add `Position`:

```text
{Position}
```

Add `Velocity`:

```text
{Position, Velocity}
```

Add `Health`:

```text
{Position, Velocity, Health}
```

Remove `Velocity`:

```text
{Position, Health}
```

Each unique set corresponds to a table that SIECS can find or create.

The application creates archetypes indirectly by creating component combinations.

## Archetypes form a transition graph

Adding or removing one component creates a structural edge between shapes.

Example:

```text
                       +Health
{Position, Velocity} ------------> {Position, Velocity, Health}
        |                                      |
        | -Velocity                            | -Velocity
        v                                      v
{Position} ----------------------> {Position, Health}
                       +Health
```

This graph is useful for reasoning about structural mutation.

If thousands of entities repeatedly bounce across the same edge, that transition is part of the workload and should be measured.

SIECS internally caches table transition information so repeated add/remove operations can navigate known structure efficiently.

## A tag changes the archetype

A zero-sized tag has no payload column, but it still changes structure.

These are different archetypes:

```text
{Position, Velocity}
{Position, Velocity, Selected}
```

The second table can be matched by a `Selected` query.

That queryability is exactly why the structural distinction exists.

## Archetype count versus entity count

Performance discussions often focus only on entity count.

For archetype ECS iteration, table count also matters.

Compare two worlds with 100,000 matching entities.

### World A

```text
2 matching tables
50,000 rows each
```

### World B

```text
2,000 matching tables
50 rows each
```

Both have the same number of entities, but World B has many more batch boundaries and more structural metadata.

The inner row work may be identical while the outer iteration overhead differs substantially.

This is why archetype fragmentation matters.

## Empty archetypes

An archetype can remain known even when it currently has no rows, depending on runtime behavior and lifetime rules.

The important distinction is:

```text
archetype/table structure
```

versus:

```text
current entity population
```

A persistent query can care about the structural relationship even as entities move in and out.

SIECS query iteration skips empty table batches, while the query cache can still represent structural matches.

## Query caching and archetype stability

Archetype ECS query caching works because table shapes are usually more stable than entity membership.

Entities may move:

```text
T1 -> T2
T2 -> T3
T3 -> destroyed
```

but the tables themselves can remain as reusable structural categories.

A cached query can therefore keep:

```text
matching table ids = [T1, T3, T7]
```

while row counts change continuously.

This is cheaper than rebuilding a list of matching entities every frame.

SIECS's query cache model is built around this table-level relationship.

## SIECS and query cache policy

Some ECS libraries expose several query cache modes, including uncached and partially cached queries.

SIECS currently has its own simpler model: query ids are backed by query cache structures that track matching tables.

Do not copy cache-policy advice from another ECS as if SIECS exposes the same controls.

The relevant SIECS distinction is mainly between:

- temporary convenience queries created for one operation;
- persistent query handles/ids reused across iterations.

The [Queries](/queries/) manual documents their actual lifetime and API behavior.

## Archetype fragmentation

Fragmentation means useful entities are spread across more structural groups than the workload benefits from.

Common causes include:

- many independent boolean tags;
- volatile state represented structurally;
- components that differ only because of tooling metadata;
- features that introduce a unique marker for tiny populations;
- over-specialized component sets.

Fragmentation can increase:

- table count;
- query cache entries;
- outer-loop iteration overhead;
- partially used allocations;
- number of migration paths;
- complexity of reasoning about the world.

It does **not** mean every additional archetype is bad.

Distinct structure is the point of archetypes. The question is whether the distinctions help queries enough to justify their cost.

## The combinatorial space

With `n` independent structural booleans, the theoretical number of combinations is:

```text
2^n
```

Examples:

```text
5 tags  -> 32 combinations
10 tags -> 1024 combinations
20 tags -> 1,048,576 combinations
```

A world almost never materializes all of them, but the formula highlights why independent structural axes should be chosen carefully.

The actual metric is not theoretical combinations but **materialized archetypes and their populations**.

## Stable archetypes are ideal for hot loops

An archetype model works especially well when many entities spend long periods in stable shapes.

Example:

```text
Particle
    Position
    Velocity
    Color
    Lifetime
```

A particle may update these values every frame without changing components until it is destroyed.

The structure is stable while the data is highly dynamic.

That is an archetype-friendly workload.

## Structural churn changes the cost model

Now consider entities that add/remove several tags every frame.

Even if iteration is fast, migration can dominate total cost.

The right performance model therefore includes:

```text
query iteration cost
+ structural mutation cost
+ query/table maintenance cost
+ component lifecycle cost
```

A benchmark measuring only one stable iteration loop cannot prove that an architecture is optimal for a mutation-heavy application.

## Archetype design and component requirements

SIECS can express component requirements so adding one component also requires another.

This can constrain structural shapes.

For example, if `Renderable` requires `Transform`, an entity should not remain in a meaningful state where `Renderable` exists without `Transform`.

Requirements can therefore encode structural invariants and reduce invalid archetype combinations.

They should represent genuine invariants, not be used to hide unclear ownership.

## Inheritance complicates “owned component set”

SIECS supports inheritance where components can be owned or shared according to component policy.

For basic archetype reasoning, distinguish:

```text
owned structure
```

from:

```text
effective data visible through inheritance
```

A query may read a shared inherited component even when that value is not stored as an owned column in the current table.

This is why SIECS query fields can distinguish owned versus shared data.

The introductory archetype model still applies to owned storage, while the inheritance manual explains how effective component access extends it.

## Relations can also participate in table structure

SIECS relations have storage modes with different physical consequences.

Some relation representations affect table grouping and query matching in ways that are not equivalent to a plain data component.

Do not assume that every relation edge is simply “one more component id” using the exact same storage rules.

The [Relations](/relations/) manual should be used for relation-specific structure.

## Archetypes are not domain classes

A table containing:

```text
Position + Velocity + Health
```

may contain:

```text
players
enemies
projectiles
moving destructibles
```

if those entities happen to share the same structural data.

The archetype name is the component set, not a domain type name such as `EnemyTable`.

This is important because queries should be able to reuse the storage across domain categories.

## Archetypes are not manually managed memory pools

Application code normally should not move rows between tables directly.

It expresses structural intent:

```text
add component
remove component
set absent component
change relation
```

SIECS handles finding the destination type, moving shared state, initializing new state, updating entity records, and maintaining query/observer metadata.

The archetype model is visible so you can reason about cost, not so every application becomes a table allocator.

## Predict your archetypes during design

A useful design review exercise is to list expected hot shapes.

Example:

```text
StaticSprite
    Position, Sprite

MovingSprite
    Position, Velocity, Sprite

DamageableMovingSprite
    Position, Velocity, Sprite, Health

SelectedMovingSprite
    Position, Velocity, Sprite, Selected
```

Then ask:

- How many entities occupy each?
- Which queries match each?
- Which transitions occur frequently?
- Which tags create short-lived variants?
- Are several shapes accidental rather than useful?

This is more concrete than discussing component design in isolation.

## Think in populations, not only types

Two archetypes can have very different importance:

```text
A: 500,000 entities
B: 2 entities
```

Optimizing away one transition in B may be irrelevant.

A tiny specialized archetype can be perfectly fine if it simplifies code.

Performance decisions should weight table structure by **population and frequency of access**.

## Common mistakes

### Assuming every query corresponds to one archetype

Queries generally match multiple archetypes that contain the required subset.

### Assuming tag components cost no structure

Zero payload does not mean zero structural effect.

### Counting only entities when profiling query overhead

Table count and batch fragmentation matter too.

### Avoiding all structural changes

Dynamic composition requires migrations. The goal is useful, controlled structure, not zero mutation.

### Creating many volatile archetypes from rapidly changing state

Consider stable values for high-frequency state.

### Treating inherited/shared data exactly like owned columns

Query field ownership matters when inheritance is involved.

## The rule to remember

An archetype is the runtime answer to:

> What exact owned structure does this entity currently have?

A query is the runtime answer to:

> Which archetypes satisfy the structure this operation needs?

Tables turn those answers into storage.

Next: **[Tables and columns](/introduction/tables-and-columns/)**.
