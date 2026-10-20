# C: registration, hooks and dynamic components

Use C declarations in shared headers and one definition per type in a C source.
For local types `ECS_COMPONENT`, `ECS_TAG` and `ECS_RESOURCE` combine both steps.
Register after world initialization, before tables/queries/systems use the types.

## Descriptors and constructors

```c
ECS_COMPONENT_DECLARE(Position, { float x; float y; });
ECS_CTOR(Position, { .x = 1, .y = 2 });
ECS_COMPONENT_DEFINE(Position, .ops.ctor = ecs_ctor_id(Position));
/* World setup: */
ECS_COMPONENT_REGISTER(Position);
```

`ECS_CTOR` only defines the batch constructor; install it explicitly in `.ops`.
Use `ECS_TAG_DECLARE/DEFINE` for size-zero presence markers. Component/resource
definitions accept descriptor overrides such as `.ops`, `.on_set`, `.on_remove`;
components additionally accept `.on_add` and `.inheritance`.
`ECS_SET_ONLY` sets the debug-only mutation policy and expands away in release.

Raw registration uses `ecs_component_init(&desc)` (or `ecs_component({...})`),
`ecs_component_register(&id, &desc)`, `ecs_resource_init(&desc)` or
`ecs_resource_register(&id, &desc)`. Initialize retained id storage to zero;
init allocates an id, register reuses id storage. Registration does not create
resource storage. Use raw descriptors for dynamic names or custom lifecycle.

| Field | Component | Resource |
| --- | --- | --- |
| `.name`, `.size` | Name, bytes; size zero means tag | Name and value bytes |
| `.ops` | Storage operations below | Same operations |
| `.struct_desc` | Optional reflected structure/layout | Optional reflected structure/layout |
| `.on_add` | `void(entity, component_id, void *value)` | Not available |
| `.on_set` | `void(entity, component_id, const void *incoming, void *current)` | `void(const void *incoming)` |
| `.on_remove` | `void(entity, component_id, void *value)` | `void(const void *previous)` |
| `.inheritance` | Owned by default; `EcsInheritShared` for readonly defaults | Not available |
| `.mutation` | Debug-only `EcsMutable` / `EcsSetOnly` | Not available |

Component on_set runs before copying/moving the value; OnSet observers also
receive incoming data before replacement. Resource on_set sees the incoming
value before storage transfer. on_remove runs while the previous value is valid,
before its destructor. Registration hooks must not assume a scheduler job of
their own or retain pointers after the call.

## Owned values: `ecs_type_ops_t`

| Operation | Signature | Destination |
| --- | --- | --- |
| `.ctor` | `void(void *dst, uint32_t count)` | Initialize each new value |
| `.dtor` | `void(void *values, uint32_t count)` | Destroy each initialized value |
| `.copy_ctor` | `void(void *dst, const void *src, uint32_t count)` | Uninitialized storage |
| `.copy` | Same as copy_ctor | Already initialized storage |
| `.move_ctor` | `void(void *dst, void *src, uint32_t count)` | Uninitialized storage; consume source |
| `.move` | Same as move_ctor | Already initialized storage; consume source |

Handle every row in count, not just one. Implement assignment separately from
construction so replacing an owned value releases its previous allocation.
Moves must implement the source-consumption contract themselves. Without a
matching move hook, transfer can fall back to copy plus source destructor; with
no copy hook it falls back to memcpy. Missing ordinary operations are zero-init,
memcpy and no destruction, not a deep-copy implementation.
These operations are also used by table migration and deferred-value storage.
Do not combine owning pointers with plain memcpy lifecycle defaults.

## Dynamic schema and requirements

```c
ecs_component_t point = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
    .name = "RuntimePoint", .fields = "{ float x; float y; }",
    .inheritance = EcsInheritOwned,
});
if (!point) { /* Invalid reflected schema; handle failure. */ }
ecs_component_t marker = ecs_tag_init("RuntimeMarker");
```

The reflected declaration supplies layout; dynamic descriptors have no `.size`
or `.ops`. Inspect metadata/offsets instead of assuming a C struct layout. Dynamic
registration can reuse a compatible reflected type; don't rely on fresh identity
for repeated declarations. Use `_cid` access and raw query terms for these ids.
For an explicitly sized/reflected type with custom operations, use the full
component descriptor instead.

`ecs_with(A, B, C)` makes adding A materialize B and C. Raw equivalent:
`ecs_with_many(a_id, b_id, c_id, 0)`. Requirements must be acyclic and declared
before table use; they are add-time composition, not ownership or a promise that
removing B will remove A. Default edges use
`ecs_with_relation(A, R, target)` / `ecs_with_relation_id(a_id, rid, target)`.

[Components](components.md) · [Resources](resources.md) · [Introspection](../introspection.md) · [API](../../api/c-components.md)
