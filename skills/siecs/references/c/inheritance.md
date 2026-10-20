# C: templates and inheritance

```c
ecs_entity_t base = ecs_new();
ecs_set(base, Position, { 10 });
ecs_entity_t instance = ecs_new();
ecs_is_a(instance, base);
```

Owned inheritance is the default. For shared data, declare the component with
`ECS_COMPONENT_DECLARE` and define it using
`ECS_COMPONENT_DEFINE(Material, .inheritance = EcsInheritShared)` before
registration. Configure the base before attaching instances.

Use `ecs_is(instance, base)` for ancestry and `ecs_entity_base(instance)` for
the direct base. `ecs_abstract` marks a base and its owned child subtree.
The generic `IsA` relation APIs have the same instantiation semantics.

Use `.is_a = base` in a query for ancestry filtering. Read shared fields through
`ecs_in`, and check `ecs_field_is_shared` before row indexing. A local `ecs_set`
establishes an override. `ecs_propagate_add` adds a component recursively through
a supported acyclic indexed relation; it is not a universal graph operation.

[Copy/share policies and child snapshots](../inheritance.md) · [API](../../api/c-inheritance.md)
