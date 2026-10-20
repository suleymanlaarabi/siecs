# C: entities

Use `ecs_new()` for a fresh anonymous entity, `ecs_new_no_reuse()` for a new
index, and `ecs_new_named(parent, path)` to find/create a path.

```c
ecs_entity_t scene = ecs_new_named(0, "Scene");
ecs_entity_t actor = ecs_new_named(scene, "Actor");
ecs_entity_t found = ecs_lookup("Scene::Actor");
ecs_add(actor, Disabled);
ecs_remove(actor, Disabled);
```

`ecs_lookup_from` resolves relative paths. Check zero on lookup failure;
`ecs_is_alive` validates an existing world handle. `ecs_entity_id/generation`
extract fields; `ecs_entity_from_index` resolves the currently live generation.

`ecs_set_name` can replace a colliding entity. `ecs_entity_name` is borrowed;
`ecs_entity_path` allocates a string that the caller frees. `ecs_kill` destroys
the entity and applies relation cleanup. Use `ChildOf` for owned hierarchy.

`ecs_new_named(parent, NULL)` creates an anonymous entity; a `::`-prefixed path
is absolute even with a parent. `ecs_set_name(entity, NULL)` removes its name.
An empty name also removes naming; an empty creation path fails. Other invalid
local names fail, including reserved anonymous syntax or a `::` path in set_name.
To reparent, use `ecs_relate(entity, ChildOf, parent)`; to return to root, remove
the ChildOf edge. Reparenting can replace a sibling with the same name.
Use `ecs_has(entity, Disabled)` to inspect the enabled tag; C has no separate
entity enable/disable functions. Neither disabling nor enabling deletes data.

[Identity, collisions, and deferred names](../entities.md) · [API](../../api/c-entities.md)
