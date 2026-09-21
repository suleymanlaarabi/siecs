---
title: ECS vs OOP
description: Compare object-centered and ECS-centered decomposition, including identity, behavior dispatch, encapsulation, polymorphism, ownership, and hybrid architectures.
---

“ECS versus OOP” is often framed as a programming-language argument. It is not.

SIECS has both C and C++ APIs. A C++ application can use classes, RAII, templates, encapsulation, generic programming, and ordinary objects while using ECS for world data.

The useful comparison is between **object-centered decomposition** and **data/query-centered decomposition**.

## Object-centered decomposition

A conventional object model groups state and behavior around one object boundary.

Example:

```cpp
class Character {
public:
    void update(float dt);
    void take_damage(int amount);
    void render(Renderer &renderer);

private:
    Position position;
    Velocity velocity;
    Health health;
    Inventory inventory;
    Animation animation;
};
```

The class says:

```text
these fields belong together
these methods define behavior
this object owns its invariants
```

That can be an excellent design when those statements are true.

## ECS-centered decomposition

An ECS can split the same state:

```text
Position
Velocity
Health
Inventory
Animation
```

and define transformations by requirements:

```text
Movement -> Position + Velocity
Damage   -> Health
Render   -> Position + Renderable
Animate  -> Animation + Time
```

The primary organization becomes:

```text
data shape
+ transformation
```

rather than:

```text
object type
+ methods
```

## Different questions

Object-oriented design often asks:

> Which object should own this responsibility?

ECS design often asks:

> Which data does this operation require, and which entities have that data?

Both are useful questions. They optimize different forms of change.

## Identity

In many object models, identity and storage are closely connected.

A pointer/reference to a `Character` often identifies the character and directly points at its state.

In SIECS:

```text
entity handle = identity
component storage = current data location
```

The entity can move between tables while the handle remains the same logical identity.

This is a major conceptual difference.

## Behavior dispatch

Object-oriented polymorphism often uses per-object dispatch:

```cpp
for (Object *object : objects) {
    object->update();
}
```

Different runtime types select different code paths.

ECS often uses structural dispatch:

```text
query Position + Velocity
```

Every matching entity participates in the same transformation regardless of domain class.

The query is effectively a dispatch mechanism based on data shape.

## Structural polymorphism

Suppose a future entity type `Drone` also has:

```text
Position
Velocity
```

A movement query already matches it.

No new `Drone::move()` override is required if movement behavior is fully described by those components.

This is a form of polymorphism based on **capability structure** rather than inheritance.

## Inheritance is one-dimensional; capabilities are often multi-dimensional

A class tree works best when there is a strong “is-a” hierarchy.

Gameplay often varies along independent dimensions:

```text
Flying
Damageable
Selectable
Networked
Renderable
Parented
```

Trying to encode all combinations in subclass names creates combinatorial pressure.

ECS composition represents each dimension independently.

This is one of the areas where ECS can be much easier to extend.

## OOP already supports composition

ECS does not invent composition.

An object-oriented program can compose objects through members, interfaces, delegation, strategy objects, or components in a traditional game-object system.

The ECS difference is that composition is:

- stored in a centralized world model;
- queryable by component presence;
- tied to archetype storage;
- available to systems without going through one object owner;
- dynamically mutable at runtime.

That runtime structural index is the distinguishing feature.

## Encapsulation changes shape

A private field provides strong local encapsulation:

```cpp
class Health {
private:
    int value;
};
```

ECS components are often simple data types exposed directly to systems.

That does not mean an ECS should abandon encapsulation.

Instead, encapsulation can live at different boundaries:

- module APIs;
- system ownership conventions;
- read/write query access;
- component hooks;
- observer boundaries;
- scheduler phases;
- C++ component methods where appropriate.

The key question becomes:

> Which transformations are allowed to mutate this data?

rather than only:

> Which member function can access this private field?

## Public data requires discipline

If every system can write every component arbitrarily, the codebase can become harder to reason about than an object model.

Useful conventions include:

```text
Physics owns writes to PhysicsState
Gameplay reads PhysicsState
Render reads Transform
TransformBuild owns writes to Transform
```

The SIECS scheduler's read/write metadata can help express those relationships, but semantic ownership remains an application responsibility.

## Invariants

Objects are excellent at enforcing local invariants.

Example:

```text
balance must never be negative
socket must be closed exactly once
file handle owns OS resource
```

For such state, an ordinary class may be better than exposing fields as independently mutable ECS components.

ECS is strongest when the important operations are **bulk transformations across populations**, not when each instance is primarily a self-contained invariant machine.

## Resource-owning objects

Many objects should remain ordinary objects:

```text
GPU device
file stream
network connection
thread pool
asset database
compiler context
window
```

These often have:

- nontrivial ownership;
- RAII lifetime;
- operating-system resources;
- complex methods;
- few instances;
- little need for archetype queries.

They can live outside ECS or be referenced through a SIECS resource.

Putting everything into entity components is not a design goal.

## Hybrid architecture

A realistic SIECS application might use:

```text
ordinary C++ objects/resources:
    Renderer
    AssetDatabase
    NetworkClient
    AudioDevice

ECS entities/components:
    Position
    Velocity
    Health
    Sprite
    Collider
    AIState
```

Systems can request a renderer resource while iterating renderable entities.

This combines strong object ownership where appropriate with data-oriented bulk world processing.

## Object graphs versus relation graphs

Object-oriented state often forms pointer graphs:

```text
parent.children[]
child.parent*
unit.squad*
weapon.owner*
```

ECS can model graph edges through entity handles or explicit relations.

SIECS relations can make those connections queryable and can provide specialized storage modes.

This shifts some graph structure from opaque object fields into the world index.

That can improve traversal and tooling, but it is not always necessary. A simple local handle field may be enough for a relationship that never needs graph queries.

## Ownership graphs remain important

ECS does not eliminate ownership and lifetime questions.

If entity A refers to entity B, the application still needs to decide:

- what happens when B dies?
- does A own B?
- should B outlive A?
- should the relation be removed automatically?
- is the reference optional?

Relations can encode policy, but domain lifetime must still be designed.

## Virtual dispatch versus query passes

Consider 100,000 heterogeneous objects.

Virtual dispatch can produce:

```text
object 0 -> UpdateA
object 1 -> UpdateC
object 2 -> UpdateA
object 3 -> UpdateB
```

An ECS can reorganize by operation:

```text
Move pass    -> all Position + Velocity
Damage pass  -> all Health
Render pass  -> all Position + Mesh
```

This can improve homogeneity and data locality.

The tradeoff is that behavior for one conceptual entity is now spread across several systems rather than methods on one object.

## Local reasoning versus global data flow

Objects can make one entity easy to inspect:

```text
Character::update
Character::render
Character::damage
```

ECS can make one *transformation* easy to inspect:

```text
MovementSystem
RenderSystem
DamageSystem
```

Which is better depends on what engineers need to reason about most often.

Large simulations often benefit from transformation-centric reasoning because the expensive work is population-wide.

Small business objects may benefit more from instance-centric reasoning.

## ECS can reduce hidden work

A method like:

```cpp
object.update();
```

can touch any private state or global service.

A well-designed ECS system signature can expose much of its data dependency:

```text
Position RW
Velocity R
Time resource R
```

This can make side effects more visible.

Of course, a system callback can still call arbitrary code and hide work. The architecture helps only if the codebase uses the explicit data model consistently.

## OOP can also be data-oriented

A class can wrap a packed array. A renderer can use data-oriented batches behind an object API. C++ templates can generate efficient loops. A virtual-free object model can use contiguous pools.

Therefore:

```text
OOP != slow
ECS != automatically fast
```

The relevant comparison is the actual data layout and control flow.

## ECS can still contain methods

A component type can have helper methods in C++ when that improves local behavior:

```cpp
struct Vec2 {
    float x, y;
    float length_squared() const;
};
```

That does not violate ECS principles.

The concern is not “methods are forbidden.” The concern is whether world-level behavior is hidden behind per-object dispatch in a way that defeats bulk data processing.

## Migrating from an object model

A poor migration strategy is:

```text
Class Player -> component Player
Class Enemy  -> component Enemy
Class Door   -> component Door
```

with each component containing all old fields.

That merely stores the old object model inside an ECS.

A better process is:

1. identify repeated population-level operations;
2. list the data each operation consumes;
3. find shared data across domain categories;
4. group fields by access and lifetime;
5. define components around those groups;
6. define systems around transformations;
7. keep object-style subsystems where they remain clearer.

## Example migration

Original:

```cpp
class Enemy {
    Position position;
    Velocity velocity;
    Health health;
    AIState ai;

    void update(float dt);
    void render();
};
```

Potential ECS decomposition:

```text
Position
Velocity
Health
AIState
Enemy tag
Renderable
```

Systems:

```text
Movement -> Position + Velocity
AI       -> Enemy + AIState + Position
Damage   -> Health
Render   -> Position + Renderable
```

Now a friendly NPC can reuse movement, health, and rendering without inheriting from `Enemy`.

## When object methods are still better

A function that operates on one coherent value can remain a method:

```cpp
health.apply_damage(amount);
quaternion.normalize();
asset_handle.is_valid();
```

The ECS architecture does not require every operation to become a global system.

The distinction is between **local behavior of a value** and **world behavior over populations**.

## Error handling and ownership

Objects often pair resource acquisition and release through constructors/destructors.

For ECS components with nontrivial lifecycle, SIECS component hooks and C++ lifecycle support can participate in moves/removals.

However, highly stateful resource-owning objects may still be easier to manage outside archetype migration.

Do not put complex ownership into a movable component merely to make everything “ECS.”

## Testing differences

Object tests often instantiate one object and call methods.

ECS tests often:

```text
create world
create entities/components
run query/system
assert resulting component state
```

This can make population behavior easy to test but introduces world setup.

Both styles can coexist.

## A comparison

| Concern | Object-centered design | ECS-centered design |
| --- | --- | --- |
| Primary unit | object/class | component set + query/system |
| Identity | often object reference | entity handle |
| State location | object fields | component columns/resources |
| Behavior selection | methods/virtual dispatch | structural query matching |
| Composition | members/interfaces/delegation | runtime component sets |
| Bulk processing | explicit additional design | first-class operation |
| Structural change | object state/type conventions | add/remove components/relations |
| Stable storage | often easier for heap objects | component addresses can move |
| Cross-cutting feature | interface/registry/mixin/etc. | often component/tag + system |
| Local invariants | natural class boundary | needs component/module ownership discipline |

This table describes tendencies, not laws.

## Common mistakes

### Treating ECS as an anti-OOP ideology

Use the architecture that fits each subsystem.

### Moving every object into ECS

Resource-owning services and unique state often remain clearer as ordinary objects.

### Recreating classes as giant components

This loses composition and narrow working sets.

### Abandoning encapsulation entirely

ECS still needs ownership rules and mutation boundaries.

### Assuming virtual dispatch is always the performance problem

Memory layout and workload can matter more.

### Hiding all ECS work behind entity methods

This can make data dependencies and bulk behavior opaque again.

## The rule to remember

Object-oriented and ECS architectures emphasize different centers of gravity:

```text
OOP:
    object -> behavior -> owned state

ECS:
    transformation -> required data -> matching entities
```

A strong SIECS application can use both.

Next: **[When ECS is useful — and when it is not](/introduction/when-to-use-ecs/)**.
