# C: resources

```c
ECS_RESOURCE(Settings, { float speed; });

/* During setup: */
ECS_RESOURCE_REGISTER(Settings);
ecs_set_resource(Settings, { 2 });
const Settings *settings = ecs_get_resource_read(Settings);
```

Shared headers use `ECS_RESOURCE_DECLARE` plus one `ECS_RESOURCE_DEFINE`.
`DEFINE_UNREFLECTED` supports a deliberately unreflected declaration.
Raw registration uses `ecs_resource_desc_t`, hooks, and type operations.

Use `get_resource_read/try_get_resource_read` for const access, mutable forms
for writes, `has_resource` for presence, and `remove_resource` for cleanup.
The `_rid` APIs operate on dynamic ids and include copy/move setters.
Registry count/find/name/info and registration-presence APIs support tools.
For full descriptors, hook signatures and storage operations, use
[registration](registration.md); for id enumeration, [introspection](../introspection.md).

In a system/query descriptor, declare resource access separately:
`.resources = { ecs_in(Settings) }`. Resources never become iterator fields.

[Storage, lifecycle, and scheduling](../resources.md) · [API](../../api/c-resources.md)
