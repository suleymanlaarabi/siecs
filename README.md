# siecs

Small, typed ECS for C.

`siecs` is an entity component system with a compact C API inspired by the way Flecs lets you describe data, create entities, and run logic over matching component sets.

```c
#include <siecs.h>

ECS_COMPONENT_DECLARE(Position, {
    float x;
    float y;
});

ECS_COMPONENT_DECLARE(Velocity, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(Position);
ECS_COMPONENT_DEFINE(Velocity);

static void Move(ecs_iter_t *it) {
    Position *p = ecs_field(it, 0);
    Velocity *v = ecs_field(it, 1);

    for (uint32_t i = 0; i < it->count; i++) {
        p[i].x += v[i].x;
        p[i].y += v[i].y;
    }
}

int main(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_entity_t e = ecs_new(world);
    ecs_set(world, e, Position, { 0, 0 });
    ecs_set(world, e, Velocity, { 1, 2 });

    ecs_system(world, {
        .name = "Move",
        .phase = EcsOnUpdate,
        .query = {
            .read = { ecs_id(Position), ecs_id(Velocity) }
        },
        .callback = Move,
    });

    ecs_progress(world);
    ecs_fini(world);
}
```

## What It Looks Like

### Components Are Plain C Types

```c
ECS_COMPONENT_DECLARE(Health, {
    int value;
    int max;
});

ECS_COMPONENT_DEFINE(Health);
```

Components are declared once, defined once, and registered per world:

```c
ECS_COMPONENT_REGISTER(world, Health);
```

### Entities Are Just IDs With Data

```c
ecs_entity_t player = ecs_new(world);

ecs_set(world, player, Health, { 80, 100 });

Health *health = ecs_get(world, player, Health);
health->value -= 10;
```

### Queries Drive Systems

```c
ecs_system(world, {
    .name = "DamageTick",
    .phase = EcsOnUpdate,
    .query = {
        .read = { ecs_id(Health) }
    },
    .callback = DamageTick,
});
```

Systems run when the world progresses:

```c
ecs_progress(world);
```

### Relations Are Components Too

`ChildOf` is built in:

```c
ecs_entity_t parent = ecs_new(world);
ecs_entity_t child = ecs_new(world);

ecs_set(world, child, ChildOf, { parent });
```

Killing a parent recursively kills its children.

### Component Requirements

Use `ecs_with` when one component should imply another:

```c
ecs_with(world, ecs_id(Renderable), ecs_id(Transform));

ecs_add(world, entity, Renderable);
ecs_has(world, entity, Transform); /* true */
```
