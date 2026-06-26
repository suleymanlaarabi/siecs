---
title: Relations
description: Relation components such as ChildOf.
---

Relations are components that store an entity target.

SIECS includes the built-in `ChildOf` relation:

```c
ecs_set(world, child, ChildOf, {
    .target = parent,
});
```

The generated relation struct contains:

```c
typedef struct {
    ecs_entity_t target;
} ChildOf;
```

## Declare Custom Relations

Declare and define a relation:

```c
ECS_RELATION_DECLARE(Targets);
ECS_RELATION_DEFINE(Targets, 0);
```

`ChildOf` is defined with `EcsRelationCascadeDelete`, so killing a parent also
kills its children. Custom relations only remove reverse links by default unless
they opt into that flag.

Register it like a component:

```c
ECS_COMPONENT_REGISTER(world, Targets);
```

Then set it on an entity:

```c
ecs_set(world, entity, Targets, {
    .target = other_entity,
});
```

## Source Component

For every relation component, SIECS also creates an internal source component
used to track reverse links.

```c
ecs_component_t source_id = ecs_source(Targets);
```

This is exposed for low-level use, but normal application code should prefer the
relation component itself.

## Lifecycle

When a relation target changes, SIECS removes the source entity from the old
target reverse list and appends it to the new target reverse list. Removing the
relation removes the reverse link.

When a target entity is killed:

- relations without `EcsRelationCascadeDelete` remove the relation from sources
- relations with `EcsRelationCascadeDelete` kill the source entities too

`ChildOf` uses cascade delete because child entities are considered owned by
their parent.
