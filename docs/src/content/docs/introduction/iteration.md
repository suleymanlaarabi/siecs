---
title: Iteration
description: How SIECS queries select archetype tables, expose fields, cache matches, and turn ECS structure into batch processing.
---

Iteration is the hot path that archetype storage is designed to serve.

Most ECS behavior is not “fetch one component from one entity.” It is:

> Find every entity whose structure satisfies a condition, then perform the same transformation over that set.

Queries express the condition. Iterators turn matching tables into batches.

## The naive mental model

A first mental model for a query is:

```text
for every entity:
    if it has Position and Velocity:
        process it
```

This describes the result, but not how an archetype ECS wants to achieve it.

SIECS can instead do:

```text
for every table known to contain Position and Velocity:
    process all rows
```

The structural test moves from the entity loop to table matching.

## Outer loop and inner loop

Archetype iteration naturally has two levels.

### Outer loop

Advance through matching tables.

### Inner loop

Process rows in one table.

In C, the shape is visible:

```c
ecs_iter_t it = ecs_query_iter(query);

while (ecs_iter_next(&it)) {
    Position *positions = ecs_field(&it, 0);
    const Velocity *velocities = ecs_field(&it, 1);

    for (uint32_t i = 0; i < it.count; i++) {
        positions[i].x += velocities[i].x;
        positions[i].y += velocities[i].y;
    }
}
```

The outer loop handles structure. The inner loop handles data.

## C++ can hide the outer loop

The C++ API can present per-entity callbacks:

```cpp
query.each([](Position &position, const Velocity &velocity) {
    position.x += velocity.x;
    position.y += velocity.y;
});
```

This is a higher-level interface over the same table-oriented storage model.

Do not confuse callback syntax with a different storage strategy.

The runtime still processes matching archetype batches.

## Query terms describe structural conditions

A query is a set of conditions over world structure.

At the simplest level:

```text
required Position
required Velocity
```

SIECS extends this with terms including:

- read-only input;
- output/read-write access;
- optional components;
- filter-only components;
- negated components;
- relation terms;
- inherited/up fields;
- `IsA` restrictions;
- ordering by relation target/depth.

The full syntax belongs in the [Queries](/queries/) manual.

The conceptual point is that a query is more than a loop predicate: it is a declarative description of the data view a system needs.

## Matching a table

Suppose the query requires:

```text
Position
Velocity
not Disabled
```

These tables behave as follows:

```text
{Position, Velocity}                   -> match
{Position, Velocity, Health}           -> match
{Position}                             -> no match
{Position, Velocity, Disabled}         -> no match
```

Once a table matches, all of its rows satisfy the ordinary structural terms.

The iterator does not need to repeat those checks for every row.

## Cached matching

Persistent SIECS queries maintain a cache of matching table identifiers.

The purpose of this cache is to avoid rediscovering structural matches on every iteration.

Conceptually:

```text
query Q = Position + Velocity

cache:
    table 3
    table 7
    table 12
```

Iteration becomes:

```text
visit table 3
visit table 7
visit table 12
```

Rows can enter or leave those tables without changing the structural fact that the table matches Q.

This is why archetype-level caching is effective when table shapes are relatively stable.

## Persistent versus temporary query use

Not every query is reused equally.

### Persistent query

Used repeatedly across frames or many calls.

Typical examples:

- system query;
- long-lived gameplay index;
- tool view refreshed often.

The cost of constructing and maintaining the query is amortized across many iterations.

### Temporary query

Created for a setup step, test, one-off tool operation, or rare scan.

SIECS exposes convenience APIs for temporary use and explicit query handles/ids for persistent use.

The design question is whether the query is a long-lived index or an ad-hoc operation.

## SIECS does not expose Flecs cache-kind policy

Some ECS query manuals discuss explicit modes such as cached, uncached, auto, and partial caching.

That is not the SIECS API model.

SIECS query ids use its query cache implementation. Documentation should therefore explain SIECS query lifetime and table caching directly rather than copying another library's cache-kind configuration.

This is an example of adapting a concept rather than cloning an API surface.

## Fields

A field is the data exposed by a query term for the current batch.

For owned data:

```text
Position field -> pointer to current table's Position column
Velocity field -> pointer to current table's Velocity column
```

Field indices follow field-producing terms, not arbitrary component ids.

Filter-only and negated terms constrain matching without necessarily producing a data field.

## Filter terms separate selection from access

Suppose rendering needs `Position` values but only wants entities tagged `Visible`.

The query can use:

```text
Position -> field
Visible  -> filter
```

`Visible` participates in table matching but does not need a payload pointer.

This is an important conceptual distinction:

```text
selection data
```

is not always the same as:

```text
processing data
```

Good query design expresses that difference.

## Optional fields

An optional component does not exclude tables that lack it.

For an archetype ECS, optionality is naturally table-wide.

Example query:

```text
Position required
Velocity optional
```

Then:

```text
Table {Position, Velocity}
    Velocity field exists for all rows

Table {Position}
    Velocity field absent for the whole batch
```

SIECS can represent the absent field as `NULL`/none for that batch.

The inner loop does not need to perform a sparse membership lookup for every row.

## Owned versus shared fields

With inheritance, a required component may come from a base rather than the current entity's table.

SIECS exposes field-kind information so code can distinguish:

```text
owned
shared
none
```

This distinction matters because a shared field can represent one inherited value used by the whole batch rather than a writable per-row column.

The query's logical data view is therefore richer than a list of raw arrays.

## Read and write intent

Query access modes communicate what the system will do.

Conceptually:

```text
Position -> read/write
Velocity -> read
```

That information matters for:

- documentation;
- scheduler conflict analysis;
- safe shared/inherited access;
- understanding side effects;
- future parallel execution decisions.

A system that declares everything read/write obscures useful dependency information.

Use the narrowest correct access mode.

## Resource access does not change entity matching

SIECS resources belong to the world, not individual entity rows.

A query/system may declare:

```text
components:
    Position RW
    Velocity R

resources:
    Time R
```

The `Time` resource affects scheduling/access metadata but does not create another archetype requirement.

A resource-only query can have no entity batches at all.

This is a useful example of separating **entity selection** from **world-level dependencies**.

## Relation terms extend structural selection

SIECS queries can match relation structure such as:

```text
has relation
points to target
has depth
```

The physical matching behavior depends on relation storage mode.

For example, `ByTarget` can group sources by a shared target, enabling efficient target-specific matching.

This is more structured than storing arbitrary target ids inside an opaque component and scanning values manually.

## Iteration is not a stable ordering guarantee

A query processes storage order unless explicit ordering semantics are requested.

Row order can change because:

- rows compact;
- entities migrate;
- tables grow;
- table populations change.

If gameplay correctness depends on order, encode that requirement explicitly.

Do not let incidental storage order become a hidden rule.

## Query ordering has a cost

Ordering by depth or relation target is useful for hierarchy-style processing.

However, ordering is not free metadata.

Whenever an algorithm requests a stronger ordering guarantee, it constrains how results are visited and may require additional maintenance or sorting logic.

Use ordering because the algorithm requires it, not to make debug output look nicer.

## Random access versus iteration

Queries are not the only legitimate way to read ECS data.

If the program already knows one entity:

```text
main camera
local player
current selection
```

a direct lookup can be clearer than creating a query.

The distinction is:

```text
known identity -> direct entity/component access
unknown set defined by structure -> query
```

Problems appear when a bulk algorithm is implemented as thousands of random lookups that could have been one linear query.

## Secondary access inside a query

Sometimes an iterated entity references another entity.

Example:

```text
for each projectile:
    lookup owner
```

The primary projectile data may be perfectly contiguous while the owner accesses are scattered.

This can still be correct, but the total access pattern is no longer purely linear.

If secondary access dominates, consider:

- grouping by target;
- relation storage;
- preprocessing a compact working set;
- moving shared data to a resource;
- caching derived values locally.

The best option depends on semantics.

## Query granularity

A broad query may touch more tables and entities than necessary.

A very narrow set of many specialized queries may add scheduling and maintenance overhead.

The useful unit is the transformation.

If two operations have different read/write sets or run at different frequencies, they often deserve separate queries/systems.

If they always process the same entities and data together, combining them may reduce passes.

This is a workload decision.

## One pass versus multiple passes

Suppose movement and damping both use `Velocity`.

Two systems:

```text
Move    -> Position RW, Velocity R
Damping -> Velocity RW
```

create two passes.

One combined system may reduce passes but mixes responsibilities.

Tradeoffs include:

- cache reuse;
- system modularity;
- scheduling flexibility;
- reuse across entities that only need one behavior;
- code complexity.

ECS does not dictate one answer. It makes the data sets explicit enough to make the tradeoff visible.

## Systems are scheduled persistent queries

A useful conceptual model is:

```text
system
    = query
    + callback
    + phase
    + dependency/access metadata
```

The query determines *which data* the system sees.

The scheduler determines *when* it sees it.

Keeping these concerns distinct prevents query theory from becoming mixed with phase/scheduler documentation.

## Structural mutation during iteration

Because fields point directly into table storage, immediate migration can invalidate the current batch.

SIECS normal systems defer mutations into the command buffer around query iteration.

Conceptually:

```text
iterate Q
    request structural edits
finish Q
flush edits
```

This allows the query loop to operate on a stable snapshot of table storage for its execution region.

## Count operations

A cached query can often count current matching rows by summing populations of matching tables.

This is different from constructing a vector of all matching entity handles and then taking its length.

The archetype index makes aggregate structural operations possible without materializing every result.

## Batch size matters

A hot query over:

```text
1 table × 100,000 rows
```

has a different outer-loop shape from:

```text
1,000 tables × 100 rows
```

The total row count is the same, but the second case has more field setup and table transitions.

This is why component modeling and archetype fragmentation affect iteration performance.

## Empty batches

Normal SIECS iteration advances through non-empty matching table batches.

This keeps the hot loop focused on actual rows.

The query cache can know about a table structurally even when it currently contributes no work.

That separation between structural match and current population is useful for maintaining persistent queries.

## Iteration and vectorization

The C outer/inner loop shape gives compilers a simple row loop over homogeneous arrays.

That can improve:

- auto-vectorization;
- software pipelining;
- prefetch behavior;
- branch predictability.

Actual results depend on:

- component layout;
- aliasing;
- compiler;
- optimization flags;
- arithmetic;
- target CPU.

The ECS provides a vectorization-friendly shape; it does not guarantee generated SIMD instructions.

## Query creation is not the same as query iteration

Performance discussions should distinguish:

```text
query construction/registration
query cache maintenance
query iteration
```

A persistent system may pay creation cost once and iteration cost thousands of times.

A tool creating many ad-hoc queries may have a different profile.

Benchmark the operation the application actually performs.

## Common mistakes

### Thinking a query checks every entity each frame

Persistent archetype queries can iterate cached table matches.

### Treating optional components as sparse per-row storage

Optionality is naturally batch/table-wide for ordinary archetype columns.

### Declaring all terms read/write

Precise access metadata is useful for reasoning and scheduling.

### Using random entity lookup for a large set that is structurally queryable

Let the archetype index do the selection.

### Assuming query order is stable domain order

Use explicit ordering when correctness depends on it.

### Copying query cache advice from another ECS API

Document SIECS's actual persistent/temporary query model instead.

## The rule to remember

A SIECS query turns structural knowledge into a data loop:

```text
terms
    ↓
matching tables
    ↓
current batch
    ↓
field pointers
    ↓
row loop
```

That pipeline is the main reason the rest of the ECS storage model exists.

Next: **[Cache locality](/introduction/cache-locality/)**.
