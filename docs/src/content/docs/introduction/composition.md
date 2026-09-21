---
title: Composition
description: Build entity capabilities from components, tags, resources, and relations without creating rigid inheritance trees or accidental archetype explosions.
---

Composition is the part of ECS that changes how a program describes what an entity **is capable of participating in**.

Instead of assigning each entity one exclusive behavioral type, ECS builds structure from independent pieces of data and facts.

That sounds similar to the general software principle “favor composition over inheritance,” but ECS makes the composition **queryable at runtime** and ties it directly to storage.

## Independent axes do not form a clean class tree

Consider these features:

```text
Moves
HasHealth
Renderable
Selectable
Networked
Flying
Frozen
Parented
```

Now consider domain categories:

```text
Player
Enemy
Projectile
Door
Platform
Camera
Light
```

The capability matrix might look like:

| Entity kind | Moves | Health | Renderable | Selectable | Networked |
| --- | --- | --- | --- | --- | --- |
| Player | yes | yes | yes | yes | yes |
| Enemy | yes | yes | yes | yes | maybe |
| Projectile | yes | maybe | yes | maybe | maybe |
| Door | no | yes | yes | yes | maybe |
| Platform | yes | maybe | yes | yes | no |
| Camera | yes | no | no | yes | no |
| Light | maybe | no | yes | yes | no |

There is no single inheritance axis that naturally captures every column.

An ECS models the columns directly.

## Components are capabilities expressed as data

A moving entity can have:

```text
Position
Velocity
```

A damageable entity can have:

```text
Health
```

A renderable entity can have:

```text
Transform
MeshRef
MaterialRef
```

A system does not need a common base class. It needs the relevant component set.

For example:

```text
Move          -> Position + Velocity
Damage        -> Health
Render        -> Transform + MeshRef + MaterialRef
Selection     -> Selected
```

This creates behavior reuse across domain categories.

## Tags represent structural facts

Not every capability needs data.

A zero-sized tag can represent:

```text
Selected
Enemy
Disabled
Abstract
Frozen
```

A tag participates in matching but does not need a per-row data payload.

This makes tags powerful for semantic classification.

However, a tag still changes the component set and therefore the archetype.

That cost must be part of the design.

## Tags are not “free bools”

Suppose an entity has:

```text
Position + Velocity
```

Adding `Frozen` creates:

```text
Position + Velocity + Frozen
```

Those are different structural shapes.

If `Frozen` changes once every several seconds, using a tag may be excellent: movement queries can exclude frozen entities structurally.

If the flag changes every frame on most entities, structural migration may cost more than a stable value:

```cpp
struct MotionState {
    bool frozen;
};
```

The value version keeps one archetype but adds a branch or value test.

The tag version increases structural filtering power but adds migrations and possibly more tables.

Composition is therefore both a semantic and storage decision.

## Avoid subclass combinations

Imagine an object hierarchy trying to represent:

```text
Enemy
FlyingEnemy
ArmoredEnemy
FlyingArmoredEnemy
PoisonedFlyingArmoredEnemy
```

The problem is not merely verbosity. Independent dimensions become encoded into named combinations.

ECS composition can separate them:

```text
Enemy
Flying
Armor
Poisoned
```

The actual entity shape emerges from the components that are present.

Systems can care about only one dimension:

```text
PoisonSystem -> Poisoned + Health
FlightSystem -> Flying + Position
ArmorSystem  -> Armor + IncomingDamage
```

No system needs to enumerate every combined subtype.

## Composition also reduces central knowledge

A central `GameObject` class tends to accumulate awareness of features:

```text
physics
rendering
AI
networking
editor selection
audio
quests
debugging
```

An ECS can let each feature own its component and systems.

For example, an editor module can introduce `Selected` without changing the simulation entity type.

A networking module can add replication metadata without forcing gameplay components to depend on networking.

This is one of the strongest maintainability arguments for ECS in large projects.

## Component granularity controls composition quality

Composition works poorly when components are too coarse.

Example:

```cpp
struct EnemyState {
    Position position;
    Velocity velocity;
    Health health;
    AI ai;
    RenderData render;
};
```

A generic movement system cannot easily reuse `Position + Velocity` for non-enemies if those values are locked inside `EnemyState`.

The component boundary preserves the domain class rather than exposing reusable capabilities.

## Components can also be too fine

The opposite extreme is:

```text
PositionX
PositionY
VelocityX
VelocityY
HealthCurrent
HealthMaximum
```

This creates many independent structural axes for fields that are almost always accessed and managed together.

The result may be:

- noisy queries;
- more archetype combinations;
- more registration metadata;
- more structural mutation;
- less readable code.

Composition should expose **meaningful variation**, not mechanically split every field.

## Structural composition versus value composition

An important distinction is whether variation should affect query membership.

### Structural variation

Use component/tag presence when systems benefit from selecting the difference before iteration.

Example:

```text
Renderable
Selected
PhysicsBody
```

### Value variation

Use fields or enums when systems usually process the same entities but branch on state.

Example:

```cpp
struct AnimationState {
    enum Mode { Idle, Walk, Run } mode;
};
```

Turning `Idle`, `Walk`, and `Run` into three tags may create migrations every animation transition without giving useful structural filtering.

The best representation depends on who queries the state and how often it changes.

## Stable roles are good composition candidates

Components and tags are especially effective for roles that are stable relative to the frequency of iteration.

Examples:

```text
PlayerControlled
NetworkReplicated
StaticGeometry
Enemy
EditorOnly
```

If entities spend many frames in one role, queries can repeatedly benefit from that structural classification.

## Temporary facts need scrutiny

Temporary state is not automatically wrong as a component.

For example, `Dead` may be structural because many systems should immediately stop matching the entity and a cleanup pipeline may specifically query dead entities.

But a marker used for one pass only:

```text
NeedsTempPass
```

that is added to 100,000 entities and removed one system later can be suspicious.

Alternatives may include:

- a value field;
- an event;
- an explicit list;
- a temporary resource;
- a different query;
- a dedicated staging structure.

The right answer depends on the operation.

## Composition produces archetypes

Every component combination that actually exists maps to an archetype.

Suppose a base moving entity has:

```text
Position + Velocity
```

and three independent tags can be present:

```text
Selected
Frozen
Networked
```

Possible shapes include:

```text
Position + Velocity
Position + Velocity + Selected
Position + Velocity + Frozen
Position + Velocity + Networked
Position + Velocity + Selected + Frozen
Position + Velocity + Selected + Networked
Position + Velocity + Frozen + Networked
Position + Velocity + Selected + Frozen + Networked
```

With three boolean axes, up to `2^3 = 8` combinations exist.

With ten axes, the theoretical space is `2^10 = 1024`.

A real game may instantiate far fewer combinations, but the exponential space explains why careless structural booleans can fragment storage.

## Archetype explosion is a modeling problem

An archetype ECS is not harmed by having more than a handful of tables. Different structural shapes are the purpose of the model.

The problem is **accidental fragmentation** where many tiny tables exist because volatile or meaningless state was encoded structurally.

Warning signs:

- many tags toggle independently every frame;
- most tables contain very few entities;
- one hot query visits many tiny batches;
- component combinations are difficult to enumerate or reason about;
- component presence is being used where a field would express the same information better.

The solution is not to remove composition. It is to choose better structural axes.

## Composition and resources

Not all data belongs in an entity component.

If one value applies to the entire world, a SIECS resource can be a better composition boundary.

Example:

```text
Time
GameRules
InputState
```

A movement system can combine:

```text
per-entity: Position + Velocity
per-world:  Time resource
```

This avoids inventing a `Time` component on every moving entity.

Composition therefore happens across multiple ECS data domains, not only component sets.

## Composition and relations

Some structure exists **between entities** rather than inside one entity.

Examples:

```text
ChildOf(parent)
BelongsTo(team)
Targets(enemy)
```

A naive component model might store an entity id field:

```cpp
struct Parent {
    ecs_entity_t entity;
};
```

That is sometimes sufficient.

SIECS relations become useful when the edge itself should be queryable or traversable and when specialized storage such as `ByTarget` or `ByDepth` helps the workload.

This expands composition from:

```text
what does this entity contain?
```

to:

```text
how is this entity connected to others?
```

The [Relations](/relationships/) manual covers the actual API and storage modes.

## Composition and inheritance are different tools

Composition answers:

> Which independent pieces of data/facts does this entity have?

Inheritance answers a different question:

> Can this entity derive state or structure from another entity/base?

SIECS supports inheritance policies and `IsA`, but composition should remain the default tool for independent capabilities.

Using inheritance to encode every gameplay category can reintroduce the rigid taxonomy that ECS composition is intended to avoid.

## Feature modules become easier to isolate

A useful ECS architecture often groups related components and systems into modules.

For example:

```text
Physics module
    RigidBody
    Collider
    Physics systems

Rendering module
    MeshRef
    MaterialRef
    Visibility
    Rendering systems

Editor module
    Selected
    Gizmo
    Editor systems
```

An entity participates in a module by acquiring the relevant components.

The entity itself does not need a central definition that imports every feature module.

This improves separation between subsystems.

## Composition creates declarative behavior membership

In object code, behavior participation is often hidden in control flow:

```cpp
if (object->is_renderable()) { ... }
if (object->can_move()) { ... }
```

In ECS, participation can be encoded in structure:

```text
Render query -> Position + MeshRef
Move query   -> Position + Velocity
```

This makes feature membership observable and inspectable by tooling.

It also means changing structure changes which systems will see the entity.

That is a powerful property and should be treated as part of application semantics.

## Designing a component vocabulary

A component set becomes a vocabulary for the world.

Good component names describe data or stable facts:

```text
Position
Velocity
Health
Selected
Static
ParentTransform
```

Poor names often describe one implementation step:

```text
RunSystem3Now
WasVisitedThisLoop
TempFlag2
```

Transient algorithm state can be valid, but making it persistent ECS structure should be deliberate.

A useful test is:

> Would another system or tool understand why this component exists?

## Composition example: destructible moving platform

An object model may invent:

```text
DestructibleMovingPlatform
```

An ECS can express:

```text
Position
Velocity
Platform
Health
Renderable
```

Systems independently consume it:

```text
Movement      -> Position + Velocity
PlatformLogic -> Platform
Damage        -> Health
Render        -> Position + Renderable
```

If a static door shares `Health + Renderable`, the damage and rendering code are reused without sharing a movement base class.

The domain noun still matters to gameplay, but it is no longer the only axis of behavior.

## Composition example: editor selection

Selection is a classic cross-cutting capability.

Any entity may become selected:

```text
Player
Light
Camera
Trigger
PhysicsBody
```

A `Selected` tag lets editor systems query selected entities without changing the underlying domain classes.

If selection changes only when the user clicks, structural migration is likely negligible compared with the convenience of direct structural filtering.

This is a good example of a tag whose semantic and performance properties align.

## Composition example: rapidly changing visibility

Visibility may be different.

If a culling system changes visible state for tens of thousands of entities every frame, toggling a `Visible` tag can create large structural churn.

A render list or visibility value may be a better hot-path representation.

The conceptual lesson is:

> The most elegant semantic component is not automatically the best storage representation for a high-frequency algorithm.

## Common mistakes

### Designing components from an inheritance tree

If every subclass becomes one component, the ECS may preserve the old taxonomy without gaining reusable data boundaries.

### Making every boolean a tag

Tags change archetypes. Consider change frequency and query value.

### Splitting every field into a component

Composition should represent useful structural variation, not arbitrary granularity.

### Using one giant component per domain object

This hides reusable data and produces object-like working sets.

### Encoding temporary algorithm stages structurally without measuring

Short-lived tags can create migration that an explicit work queue would avoid.

### Treating relations as ordinary components when graph queries are required

Use the structure that matches how the edge is consumed.

## A design checklist

For every proposed component or tag, ask:

1. Does this state vary independently from neighboring state?
2. Does it have a distinct lifetime?
3. Do systems need to query for its presence?
4. How often will presence change?
5. How many archetype combinations can it create?
6. Is there actual data, or only a fact?
7. Would a value, resource, relation, event, or external structure fit better?
8. Will the component make behavior reusable across multiple domain categories?

Composition works best when those questions have clear answers.

Next: **[Archetypes](/introduction/archetypes/)**.
