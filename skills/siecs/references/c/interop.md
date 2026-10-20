# C: public declarations and linking

```c
/* shared.h */
ECS_COMPONENT_DECLARE(Position, { float x; });
ECS_RESOURCE_DECLARE(Settings, { float speed; });
ECS_RELATION_DECLARE(OwnedBy);

/* One implementation file, after including shared.h */
ECS_COMPONENT_DEFINE(Position);
ECS_RESOURCE_DEFINE(Settings);
ECS_RELATION_DEFINE(OwnedBy, {
    .storage = EcsRelationByTarget,
    .on_delete_target = EcsDeleteSources,
});
```

Register these declarations during world setup. Export the declaration ids,
descriptors, and module callbacks from their owning library; link all users to
the same runtime. `ecs_id/ecs_rid` access generated id storage, not arbitrary
interchangeable ids.

The `_CPP` component/resource declaration forms keep fields visible to C while
exposing additional methods to C++. Reflected variants can distinguish public
layout from reflection declarations. Generated `PP_*`, `SIECS_*`, reflection,
and id symbols are support machinery; normal code uses declaration macros.

[Configuration, ownership across binaries, and support symbols](../interop.md) · [API](../../api/c-interop.md)
