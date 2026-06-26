---
title: REST Explorer API
description: HTTP endpoints exposed by the SIECS REST addon.
---

The REST addon is intended for editor and explorer tooling. Enable it when
creating the world:

```c
ecs_world_t *world = ecs_with_features({ .rest = true });
```

The server listens on port `4040` and is polled by `ecs_progress()` when the
feature is enabled.

## Entity Explorer

The entity routes expose tree-friendly data for an editor:

| Route | Purpose |
| --- | --- |
| `GET /entities` | Root entities and entities without a parent. |
| `GET /entities/:id` | Direct children for an entity id. |
| `GET /entity/:id` | Entity detail, including component values when reflected. |

Entity list items are intentionally small:

```ts
type EntityListItem = {
  name: string;
  index: number;
  generation: number;
  hasChildren?: boolean;
};
```

## Component Schema

The schema routes are for editor forms. They describe reflected components and
fields using compact type information rather than dumping reflection internals.

```ts
type ComponentField = {
  name: string;
  type: number;
};

type Component = {
  id: number;
  name: string;
  isRelation: boolean;
  type: number;
  fields: ComponentField[];
};
```

Unreflected components are not editable from the explorer because the server
does not know their field layout.

## Mutating Component Values

Component mutation is component-level: the client submits the full component
value for one entity and one component. This keeps the server simple and avoids
partial-update ambiguity for structs.

Editor clients should:

1. Fetch the entity detail.
2. Fetch the reflected component schema.
3. Render form controls from field types.
4. Submit the full component value after editing.

## Production Notes

The REST addon is a development/editor feature. Do not expose it on an untrusted
network without authentication, routing restrictions, or a separate proxy layer.
