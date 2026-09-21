---
title: Why ECS?
description: Why entity component systems exist, what architectural pressures they address, and the costs and tradeoffs of adopting one.
---

An Entity Component System is useful when an application has **many identifiable things**, those things share **overlapping pieces of state**, and the application repeatedly performs the same operations over subsets selected by that state.

Games are the most familiar example, but the pattern also appears in simulations, editors, visualization software, robotics, digital twins, and other programs that maintain large dynamic worlds.

The important part is not the genre. It is the shape of the data and work.

## The problem starts with overlapping capabilities

Imagine a game with:

- players;
- enemies;
- projectiles;
- moving platforms;
- cameras;
- lights;
- particles;
- editor gizmos.

Now add capabilities:

- has a position;
- moves;
- has health;
- can be selected;
- can be rendered;
- belongs to a parent;
- can receive damage;
- participates in AI;
- is network replicated.

These capabilities do not naturally form one tree.

A projectile and camera may both move but share almost nothing else. A door and enemy may both have health. Any entity may become selected by an editor. A rendering pass may care about position and visual data but not whether the entity is a player, projectile, or decoration.

As the number of independent axes grows, a class hierarchy becomes less useful as the primary runtime index.

## The object question and the ECS question

An object-oriented design often begins with:

> What kind of object is this?

and then invokes behavior through that object's type or interfaces.

An ECS often begins with:

> Which entities currently have the data this operation requires?

For movement:

```text
Position + Velocity
```

For health regeneration:

```text
Health + RegenRate
```

For selection outlines:

```text
Position + Selected
```

The difference is deeper than API style. It changes how the runtime can organize memory and how behavior discovers its inputs.

## Identity, data, behavior

The canonical ECS decomposition separates three concepts.

### Identity

An entity identifies one logical thing.

It is cheap to copy and pass around. It lets different components, systems, relations, and tools refer to the same logical thing without requiring them to share an object pointer.

In SIECS, the entity handle includes an index and generation. Identity can stay stable while the entity's physical component storage moves.

### Data

Components contain state or represent facts.

```c
Position
Velocity
Health
Selected
```

The entity does not become a different identity when a component is added or removed. Its *structure* changes.

### Behavior

Queries and systems operate on the data they need.

A movement system does not need to know the complete domain meaning of an entity. It needs the movement inputs.

This reduces coupling between domain taxonomy and processing logic.

## Why composition matters

Suppose the project has a `Character` base class and later needs a moving platform to participate in the same motion integration. Then a projectile. Then a camera.

There are many ways to solve this without ECS—interfaces, delegation, mixins, data tables—but ECS makes composition its central model.

An entity can be:

```text
Position + Velocity + Platform
```

or:

```text
Position + Velocity + Projectile
```

The movement system is reused because it depends on the shared components rather than a shared domain parent class.

This is particularly useful for cross-cutting features added late in development:

```text
Selected
DebugDraw
NetworkReplicated
Frozen
Invisible
```

A new tag or component can make an entity visible to a new family of queries without modifying a base class used by the rest of the application.

## Why archetypes matter

Composition alone does not require archetype storage. An ECS could store each component type independently.

SIECS chooses archetypes because it wants repeated queries to operate on groups that already share a known structure.

Suppose these entities exist:

```text
A = Position, Velocity
B = Position, Velocity
C = Position, Health
D = Position, Velocity, Health
```

SIECS groups them by exact component set:

```text
T1 = {Position, Velocity}
     A, B

T2 = {Position, Health}
     C

T3 = {Position, Velocity, Health}
     D
```

A query for `Position + Velocity` can match T1 and T3. The runtime does not have to rediscover component membership for A, then B, then C, then D on every pass.

This transforms the problem from:

```text
search entities repeatedly
```

into:

```text
maintain structural groups
iterate pre-matched groups repeatedly
```

That trade is valuable when iteration is common and structural shapes are relatively stable.

## Queries become an index over structure

A useful way to think about an archetype ECS is as a runtime index over component combinations.

The world maintains structural facts:

```text
Table 7 has Position, Velocity
Table 8 has Position, Health
Table 9 has Position, Velocity, Health
```

A persistent query maintains the subset of those tables that satisfy its terms.

The query is therefore not just syntactic convenience. It is a reusable index into world structure.

In SIECS, query caches store matching table identifiers. Iteration can focus on cached results, while table creation updates the relevant query relationships.

This matters for systems because system queries are evaluated repeatedly, often once per frame.

## Why bulk processing matters

Consider 100,000 entities where 25,000 have `Position + Velocity`.

A generic object update loop might involve:

```text
load object pointer
load dynamic type or flags
branch on capability
find movement state
update values
repeat
```

An archetype ECS can instead make the movement pass resemble:

```text
load matching table
load Position column
load Velocity column
iterate contiguous rows
repeat for next matching table
```

The second shape has several potential advantages:

- fewer repeated structural tests;
- narrower working sets;
- more predictable memory access;
- fewer unrelated fields in the hot path;
- easier compiler optimization of simple loops;
- straightforward batching.

These are opportunities, not guarantees. Poor component design can still create scattered access, too many archetypes, oversized components, or excessive migration.

## Why ECS can improve changeability

Performance gets most of the attention, but runtime composition can also improve software design.

Imagine adding an editor selection feature.

In an object hierarchy, every selectable type may need a common interface, base class change, external registry, or side table.

In ECS, selection can be represented structurally:

```text
Selected
```

An outline system can query:

```text
Position + Selected
```

No player, enemy, door, or light class needs to know that an editor feature exists.

This is a form of **open-world composition**: new behavior can be introduced by creating new data and new queries over existing entities.

## Explicit data dependencies

A system signature can describe its needs directly.

Conceptually:

```text
Move
    Position: read/write
    Velocity: read
    Time:     read resource
```

This is useful even before optimization.

A reader can answer:

- What state can this system mutate?
- What state can it only read?
- Which entities can it affect?
- Which world-level resources does it use?

SIECS also uses access metadata for scheduling and conflict analysis, so the declaration can become executable metadata rather than comments that may drift from reality.

## What ECS does not solve

An ECS does not automatically solve:

- game architecture;
- synchronization;
- deterministic simulation;
- serialization;
- networking;
- scene management;
- asset ownership;
- rendering architecture;
- multithreading;
- cache misses;
- bad algorithms.

It gives these subsystems a structured way to interact with world data, but each problem still requires its own design.

## ECS is not automatically fast

The phrase “ECS is fast” hides the actual cost model.

An archetype ECS trades some operations for others.

### Operations it tries to make cheap

- repeated iteration over common component combinations;
- finding all entities of a stable structural shape;
- processing component data in batches;
- adding new systems that operate on existing components;
- reusing behavior across domain categories.

### Operations that can be more expensive

- adding/removing components, because entities may migrate tables;
- maintaining many tiny archetypes;
- highly dynamic structural state;
- random access patterns that ignore table locality;
- maintaining query metadata as new table shapes appear.

The correct question is therefore:

> Does the workload perform enough useful iteration to justify maintaining the structure that makes that iteration cheap?

## ECS moves complexity

A common mistake is to describe ECS as if it removes architectural complexity.

It does not. It relocates it.

An object design may concentrate complexity in:

- class hierarchies;
- ownership graphs;
- virtual dispatch;
- interfaces;
- per-object update methods.

An ECS design may concentrate complexity in:

- component boundaries;
- query definitions;
- archetype combinations;
- system ordering;
- structural mutation;
- data lifetime;
- relation graphs.

The second set can be a better fit for large dynamic worlds, but it still requires careful design.

## ECS and data-oriented design are related, not identical

Data-oriented design asks the program to be organized around data transformations and access patterns.

ECS provides a useful framework for that when the data belongs to many composable entities.

You can write data-oriented software without ECS. You can also write a badly data-oriented ECS by:

- placing everything in one giant component;
- storing pointer-heavy object graphs inside components;
- doing random entity lookups inside every hot loop;
- toggling structural state constantly;
- splitting data into meaningless tiny components.

The architecture creates good defaults only when the component model reflects real access patterns.

## Why not just use arrays manually?

For a small, fixed simulation, manual arrays can be simpler and faster to reason about.

For example:

```text
positions[N]
velocities[N]
health[N]
```

The complexity appears when membership differs:

- some entities have health, others do not;
- some are renderable;
- some are children of others;
- some are temporarily disabled;
- new combinations appear dynamically;
- systems need overlapping subsets.

At that point, the application starts maintaining indices and synchronization rules between collections.

An ECS provides a general runtime for maintaining those structural sets and querying them consistently.

The benefit is not “arrays versus no arrays.” It is the combination of **composition, structural indexing, and reusable iteration**.

## Why not just use one `GameObject` with optional fields?

Another common model is one large object with flags:

```cpp
struct GameObject {
    bool has_velocity;
    bool has_health;
    bool is_selected;
    Position position;
    Velocity velocity;
    Health health;
    // ...
};
```

This can be perfectly adequate at small scale.

As optional features grow, however:

- every object reserves space for unrelated state unless indirection is added;
- systems repeatedly branch on feature flags;
- all data shares one lifetime and object layout;
- adding a cross-cutting feature expands the central object definition;
- bulk operations step over fields they do not use.

ECS decomposes the optional structure into first-class component membership.

## The cost of dynamic composition

Composition sounds free at the design level, but archetype storage gives each component combination a physical consequence.

If ten independent tags can toggle freely, there are theoretically up to:

```text
2^10 = 1024
```

possible combinations.

A real world may instantiate only a small fraction, but it demonstrates an important principle:

> Structural expressiveness can create storage fragmentation if every boolean becomes an independent structural axis.

Good ECS design separates **stable structure** from **volatile state**.

A stable role such as `PlayerControlled` may be a good tag.

A flag that flips several times per frame on thousands of entities may be better represented as data inside a stable component.

## Why SIECS specifically uses an archetype model

The current SIECS implementation reflects a particular optimization target:

- entities are grouped by structural type;
- data-bearing components live in table columns;
- rows are kept packed;
- query caches track matching tables;
- systems iterate query batches;
- structural mutation can be deferred through a command buffer;
- resources model world-level data outside entity tables.

This creates a direct path from a query declaration to a linear data loop.

```text
system requirements
    ↓
query cache
    ↓
matching table
    ↓
field pointers
    ↓
row loop
```

The architecture makes the repeated path explicit.

## The practical reason to choose ECS

Choose ECS because the application's *dominant work* benefits from these properties:

- many live identities;
- overlapping capabilities;
- repeated subset processing;
- useful structural composition;
- explicit data dependencies;
- batch-friendly data;
- a need for runtime inspection or tooling over world state.

Do not choose ECS merely because it is fashionable, because another engine uses one, or because a benchmark showed one tight loop outperforming a different architecture.

The architecture should match the program.

## Questions to ask before adopting ECS

Before committing a subsystem to ECS, answer:

1. What are the large repeated loops?
2. Which data does each loop read and write?
3. How many entities participate?
4. Which capabilities overlap across domain categories?
5. How frequently do component sets change?
6. Which state is global rather than per entity?
7. Which relationships form graphs rather than local component data?
8. Which data must have stable addresses?
9. Which parts of the program are better represented by ordinary objects or services?
10. What will actually be measured to verify the design?

If those answers are clear, ECS becomes a deliberate architecture rather than a collection of unfamiliar APIs.

Next: **[Data-oriented design](/introduction/data-oriented-design/)**.
