# C: components and tags

```c
ECS_COMPONENT(Position, { float x; });
ECS_COMPONENT(Velocity, { float x; });
ECS_TAG(Moving);

/* During world setup: */
ECS_COMPONENT_REGISTER(Position, Velocity, Moving);
ecs_with(Moving, Position, Velocity);
```

For shared declarations, put `ECS_COMPONENT_DECLARE`/`ECS_TAG_DECLARE` in a
header and their `DEFINE` forms in one translation unit. `ECS_CTOR` supplies
constructor support. Register hooks/type operations in the definition or an
`ecs_component_desc_t` before use.

Use `ecs_add/remove/has`, `ecs_set`, `ecs_get/try_get`, and `ecs_modified` for
typed access; their `_cid` variants accept dynamic ids. `ecs_move_cid` consumes
an initialized source value through its move operation. Owned-presence checks
differ from effective inherited presence (`ecs_has_cid_owned`).

For constructor installation, every descriptor field, hook signatures, owned
copy/move operations and dynamic schema examples, use [registration](registration.md).
`ecs_component_dynamic_init` accepts reflected fields. Count/lookup/name/info
functions expose the registry; `ecs_tag_init` registers a dynamic tag. Use
`ECS_SET_ONLY` when specifying a debug mutation policy portably.

[Lifecycle, requirements, and metadata](../components.md) · [API](../../api/c-components.md)
