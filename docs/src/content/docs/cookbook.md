---
title: Cookbook
description: Small complete patterns for common SIECS tasks.
---

## Minimal World

```c
ecs_world_t *world = ecs_init();
ecs_entity_t entity = ecs_new(world);
ecs_fini(world);
```

## Game Loop With Time

```c
ECS_RESOURCE(Time, {
    float dt;
});

ECS_COMPONENT(Position, { float x; });
ECS_COMPONENT(Velocity, { float x; });

static void Move(ecs_iter_t *it) {
    const Time *time = ecs_get_resource_read(it->world, Time);
    Position *p = ecs_field(it, 0);
    const Velocity *v = ecs_field(it, 1);

    for (uint32_t i = 0; i < it->count; i++) {
        p[i].x += v[i].x * time->dt;
    }
}

ecs_world_t *world = ecs_init();
ECS_RESOURCE_REGISTER(world, Time);
ECS_COMPONENT_REGISTER(world, Position);
ECS_COMPONENT_REGISTER(world, Velocity);

ecs_set_resource(world, Time, { .dt = 0.016f });
ecs_system(world, {
    .name = "Move",
    .phase = EcsOnUpdate,
    .query.terms = { ecs_inout(Position), ecs_in(Velocity) },
    .callback = Move,
});
```

## Parent And Child

```c
ecs_entity_t parent = ecs_new(world);
ecs_entity_t child = ecs_new(world);

ecs_set(world, child, ChildOf, { parent });
```

`ChildOf` cascades deletion: killing the parent also kills its children.

## Optional Component Query

```c
ecs_query_id_t q = ecs_query(world, {
    .terms = { ecs_inout(Position), ecs_in_optional(Velocity) },
});

ecs_iter_t it = ecs_query_iter(world, q);
while (ecs_iter_next(&it)) {
    Position *p = ecs_field(&it, 0);
    const Velocity *v = ecs_field(&it, 1);

    for (uint32_t i = 0; i < it.count; i++) {
        if (v != NULL) {
            p[i].x += v[i].x;
        }
    }
}
```

## React To A Component Set

```c
static void OnPositionSet(ecs_observer_event_t *event) {
    const Position *incoming = event->trigger_data;
    (void)incoming;
}

ecs_observer(world, {
    .on = EcsOnSet,
    .query.terms = { ecs_in(Position) },
    .callback = OnPositionSet,
});
```

## Pause Entities With Disabled

```c
ecs_add(world, entity, Disabled);
ecs_remove(world, entity, Disabled);
```

Normal queries skip disabled entities. Add `ecs_filter(Disabled)` to match them
explicitly.
