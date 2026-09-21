---
title: Tables and columns
description: How SIECS maps archetypes to packed table rows and contiguous component columns, and what that means for iteration and pointer validity.
---

An archetype describes structure. A table is the storage for that structure.

SIECS stores entities that have the same owned component set in the same table. Each data-bearing component is represented by a column, and the same row index refers to the same entity across all columns.

This chapter turns the abstract idea of an archetype into a concrete memory model.

## Logical rows, physical columns

Consider the archetype:

```text
{Position, Velocity, Health}
```

Logically, it looks like a table:

| Row | Entity | Position | Velocity | Health |
| --- | --- | --- | --- | --- |
| 0 | E10 | P10 | V10 | H10 |
| 1 | E42 | P42 | V42 | H42 |
| 2 | E77 | P77 | V77 | H77 |

Physically, SIECS stores the entity handles and component values in separate contiguous arrays:

```text
entities: [E10][E42][E77]
Position: [P10][P42][P77]
Velocity: [V10][V42][V77]
Health:   [H10][H42][H77]
```

Row 1 means:

```text
entity   = E42
Position = P42
Velocity = V42
Health   = H42
```

This layout is the basis for SIECS batch iteration.

## Why columns exist

Suppose movement needs only `Position` and `Velocity`.

A movement loop can walk:

```text
Position: [P10][P42][P77]...
Velocity: [V10][V42][V77]...
```

without touching `Health` at all.

With a large object layout, the same loop might step over unrelated fields between useful values.

Column storage therefore makes it possible for different systems to consume different subsets of the same entity structure efficiently.

## Tables encode a structural invariant

Once a query is iterating a table with archetype:

```text
{Position, Velocity, Health}
```

it knows every row has owned storage for those components.

That means the inner loop does not need:

```text
if row has Position
if row has Velocity
```

The test was already performed at table matching time.

This is one of the most important performance properties of archetype storage: repeated structural checks move out of the per-entity loop.

## SIECS table internals

The current SIECS implementation stores, among other metadata:

```text
entity_count
entity_capacity
entities pointer
component columns
component type
transition/index metadata
observer metadata
```

The public API treats tables as implementation-managed storage. Application code generally interacts through entity operations and iterators rather than directly mutating table internals.

Understanding the layout is still valuable because API behavior follows from it.

## One table, one exact owned type

A table does not contain optional per-row component membership.

If one row has `Velocity` and another does not, they cannot share the same ordinary archetype table.

Instead:

```text
Table A = {Position}
Table B = {Position, Velocity}
```

Optional query terms work by matching both kinds of tables and producing a field for tables where the component exists.

This is why optional fields are naturally **batch-wide** in SIECS: a table either has the owned column or it does not.

## Tags affect table identity without adding payload

A zero-sized tag participates in the archetype but does not require a data array like a normal component.

Conceptually:

```text
Table type = {Position, Selected}

entities: [E1][E2][E3]
Position: [P1][P2][P3]
Selected: structural fact, no value payload
```

The tag still changes query matching and structural migration.

This is why tags can be efficient selectors but are not free to toggle.

## Packed rows

SIECS keeps rows packed rather than leaving arbitrary holes.

Suppose:

```text
row 0 -> A
row 1 -> B
row 2 -> C
row 3 -> D
```

Removing B can cause D to move into row 1:

```text
row 0 -> A
row 1 -> D
row 2 -> C
```

This keeps the active prefix compact for iteration.

The cost is that row numbers are unstable.

Do not expose table rows as persistent domain identity.

## Why packed rows help iteration

A packed table can iterate:

```text
0 .. entity_count - 1
```

without testing tombstones or skipping free slots.

The inner loop stays simple:

```c
for (uint32_t i = 0; i < it.count; i++) {
    // process contiguous row i
}
```

This simplicity is valuable for both CPU execution and compiler optimization.

## Table capacity and growth

A table has a current row count and a capacity.

When it fills, storage grows. In the current SIECS implementation, table arrays may be reallocated during growth.

That means the memory address of a component column can change even if the entities remain in the same archetype.

Therefore:

> A pointer into a SIECS component column is not a permanent pointer to that component instance.

This rule matters for code that tries to cache component addresses between frames.

## Pointer validity

A direct component pointer/reference is best understood as a **view into current storage**.

It can be invalidated by operations including:

- table growth;
- entity migration;
- row removal/compaction;
- entity destruction;
- command-buffer flushes that change structure.

The exact safe scope depends on the API being used.

During a normal query batch, field pointers are intended for processing that batch. Long-lived identity should be represented by entity handles, not retained column pointers.

## Table growth and entity migration are different

Both can invalidate addresses, but for different reasons.

### Growth

The entity stays in the same table type. The backing arrays may move.

### Migration

The entity changes table type because its structure changes.

For application code, both are reasons not to treat storage addresses as identity.

For performance analysis, they are different costs.

## Component alignment and size

Each column stores values of one registered component type.

Component size influences:

- bytes per row;
- migration cost;
- allocation size;
- cache footprint;
- memory bandwidth during iteration.

A 4-byte component and a 512-byte component both occupy one logical column, but their physical consequences differ substantially.

This is why component-size decisions belong in ECS design, not only in C struct design.

## Table row width is conceptual

Because columns are separate, there is no single contiguous “row struct” containing all component bytes.

Still, it is useful to calculate a conceptual row payload:

```text
sizeof(Position)
+ sizeof(Velocity)
+ sizeof(Health)
+ entity handle overhead
+ column/table metadata amortization
```

That gives a rough sense of how much storage a population consumes.

However, actual memory traffic depends on which columns a query touches.

A query reading only Position does not have to stream Velocity and Health just because they share the same archetype.

## Table fragmentation versus memory fragmentation

“Fragmentation” can mean two different things.

### Archetype fragmentation

Entities are spread across many structural tables.

### Allocator/memory fragmentation

Physical heap allocations are scattered or waste capacity.

These are related but not identical.

An ECS can have few archetypes but still suffer allocator fragmentation, or many archetypes with compact allocations.

When diagnosing performance, name the problem precisely.

## Small tables

A table with only a few rows is not automatically bad.

Some structural shapes are naturally rare:

```text
main camera
player singleton-like entity
special boss
editor controller
```

The problem appears when a hot query must visit large numbers of tiny tables and the structural distinctions are not useful.

Measure hot populations rather than applying a universal minimum table size.

## Empty table zero and the empty archetype

New entities start without user components.

Conceptually they live in the empty archetype:

```text
{}
```

The empty table has entity rows but no user component data columns.

This demonstrates an important point: entity identity storage and component data storage are related but not the same thing.

An entity can exist before user data is attached.

## Query fields map to columns

When a SIECS iterator exposes:

```c
Position *positions = ecs_field(&it, 0);
```

that field normally points at the matching table's Position column for the current batch.

This is why field indexing is defined at the query level while the returned pointer changes as the iterator advances to the next table.

Conceptually:

```text
iteration on T1 -> field 0 points to T1.Position
iteration on T2 -> field 0 points to T2.Position
iteration on T3 -> field 0 points to T3.Position
```

The query term stays the same; the backing table changes.

## Shared/inherited fields are different

SIECS inheritance can make a query field visible even when the current table does not own a local column for that component.

The iterator can distinguish field kinds such as owned, shared, or absent.

This matters because a shared inherited value is not a writable per-row array in the same sense as an owned column.

The physical storage model therefore becomes:

```text
owned field  -> current table column
shared field -> external/base storage
optional none -> no storage for this batch
```

That distinction should be respected by systems.

## Relation storage is not one universal column model

SIECS relations support multiple storage modes.

Depending on the mode, target information may be represented differently from an ordinary component column.

For example, a `ByTarget` relation can group sources by a shared target, while `ByDepth` carries hierarchy-oriented semantics.

Do not generalize the ordinary component table layout to every relation operation.

The relation manual explains those specialized layouts.

## Why table layout helps SIMD and vectorization

A simple contiguous loop:

```c
for (uint32_t i = 0; i < count; i++) {
    positions[i].x += velocities[i].x;
}
```

has properties that compilers can optimize well:

- fixed stride;
- no per-row virtual dispatch;
- predictable loop bounds;
- arrays of homogeneous types.

Depending on aliasing, compiler flags, data layout, and operation shape, this can enable auto-vectorization or efficient scalar pipelines.

An ECS does not guarantee SIMD. It provides a loop shape that is more compatible with it than a heterogeneous object dispatch loop.

## Table ordering is not semantic ordering

The sequence of rows in a table is a storage order.

The sequence of tables in a query is also not automatically a domain ordering.

If simulation correctness depends on:

```text
parents before children
low priority before high priority
network id order
```

use an explicit ordering mechanism or a different data structure.

Relying on incidental row order can break when entities are removed, migrated, or tables are created in a different sequence.

## Table operations are optimized around common structure

The archetype model assumes that moving between known component sets is a normal operation.

SIECS maintains information that lets it navigate component additions/removals between tables rather than recomputing every structure from scratch.

This does not make migration free. It reduces lookup overhead around an operation that still has to move/initialize/destroy data as appropriate.

## A concrete storage example

Suppose 10,000 entities share:

```text
Position 8 bytes
Velocity 8 bytes
Health   8 bytes
```

Ignoring allocator and metadata overhead, the payload is roughly:

```text
Position = 80 KB
Velocity = 80 KB
Health   = 80 KB
```

A movement system may stream only the first two columns: roughly 160 KB of component payload.

An AoS object with an additional 200 bytes of unrelated state per entity would expose a much larger memory footprint to a naive full-object walk.

The exact cache behavior depends on hardware and code generation, but the component-column model makes the desired working set explicit.

## When table layout can still be poor

Columnar storage is not enough if:

- the component itself contains many cold fields;
- each row immediately dereferences scattered heap pointers;
- the query visits thousands of tiny tables;
- structural changes continuously move large components;
- a system randomly accesses distant entities for every row.

The table model helps the first-order layout. Application data can still defeat locality.

## Common mistakes

### Caching field pointers across frames

Table growth or migration can invalidate them.

### Treating row numbers as permanent entity ids

Packed row removal can move entities.

### Assuming tags consume a normal data column

They affect structure without requiring a per-row payload.

### Assuming every field is an owned contiguous array

Inherited/shared and optional fields have different semantics.

### Assuming table order is stable gameplay order

Storage ordering should not become hidden simulation semantics.

### Ignoring component size during structural migration

Large components make table transitions more expensive.

## The rule to remember

A SIECS table provides this invariant:

```text
same table
    => same owned structural shape
    => same set of ordinary component columns
    => rows can be processed as a homogeneous batch
```

That invariant is what queries exploit.

Next: **[Structural changes](/introduction/structural-changes/)**.
