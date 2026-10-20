# C: modules

```c
ECS_MODULE_DECLARE(Motion, { float speed; });
ECS_MODULE_DEFINE(Motion);

void Motion_import(const Motion_props_t *props) {
    ECS_COMPONENT_REGISTER(Position, Velocity);
    /* Register systems; copy props->speed into owned state if needed later. */
    (void)props;
}

/* After world initialization: */
ECS_MODULE_IMPORT(Motion, { .speed = 2 });
```

Declare in a public header and define once when sharing the module.
Import dependencies inside `Motion_import`. The first import runs its callback;
later imports reuse its id and do not apply new props.

`ecs_module_enable/disable/is_enabled` manages captured behavior.
`ecs_module_find(&ecs_id(Motion))` finds a known typed registration;
`ecs_module_name` inspects it. Raw descriptors use `ecs_module/ecs_module_init`.

`ecs_module_load(path)` loads a shared library exporting
`void ecs_module_import(void)`. Check its zero failure result and use a runtime
shared with the application.

## Raw module descriptors

`ecs_module({...})` wraps `ecs_module_init(&desc)`. The descriptor fields are
`.name` (borrowed string), `.id` (persistent `ecs_module_id_t*`, initially zero),
`.import` (`void(const void*)`), `.desc` (borrowed properties), `.desc_size`
(properties byte size), and `.disabled` (initial captured behavior state).
Keep id storage alive until finalization, which resets it. Properties only need
to live through the synchronous import callback; they are not a resource.
Without `.id`, each raw import creates a new registration rather than deduplicating
by name. Nested imported modules capture their own registrations independently.

The loader appends the platform suffix: pass `"plugins/motion"`, not
`"plugins/motion.so"`. C++ library entry points need C linkage. Dynamic imports
keep the library loaded until world teardown; there is no hot-unload/reload API.

[Ownership, properties, and module lifetime](../modules.md) · [API](../../api/c-modules.md)
