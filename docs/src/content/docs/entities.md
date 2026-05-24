---
title: Entities
description: Entity handles, liveness, deletion, and generation reuse.
---

Entities are opaque `uint64_t` handles.

Internally, a handle packs:

| Part | Meaning |
| --- | --- |
| High 32 bits | Entity index. |
| Low 32 bits | Generation. |

## Create entities

```c
ecs_entity_t e = ecs_new(world);
```

New entities start in the empty archetype table.

## Check liveness

```c
if (ecs_is_alive(world, e)) {
    /* entity can be used */
}
```

## Kill entities

```c
ecs_kill(world, e);
```

Killing an entity removes its row from the current table. Tables use
swap-and-pop removal, so another entity may move into the removed row. The ECS
updates that moved entity record.

After kill, the entity generation is invalidated. Reusing the same index creates
a different handle with a newer generation.
