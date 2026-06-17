---
title: Modules
description: Grouping components, systems, and observers behind one import.
---

Modules are import units. They let a library or feature register its components,
relations, systems, and observers through one typed entry point.

Entities are not attached to modules. Component data is not removed when a
module is disabled.

## Declare A Module

Declare a module in a header:

```c
ECS_MODULE_DECLARE(physics, {
    float gravity;
});
```

This creates typed module properties named `physics_props_t` and declares the import
function:

```c
void physics_import(ecs_world_t *world, const physics_props_t *props);
```

## Define And Import

Define the module in exactly one C file:

```c
ECS_MODULE_DEFINE(physics);

void physics_import(ecs_world_t *world, const physics_props_t *props) {
    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_system(world, {
        .name = "Move",
        .phase = EcsOnUpdate,
        .query = {
            .terms = { ecs_inout(Position), ecs_in(Velocity) },
        },
        .callback = move_system,
    });

    (void)props;
}
```

Import it from application code:

```c
ecs_module_id_t Physics = ECS_MODULE_IMPORT(world, physics, {
    .gravity = 9.81f,
});
```

The props are passed only to `physics_import()`. Store any runtime settings
you need in components, globals owned by your module, or user-managed state.

## Empty Modules

Modules with no parameters still use empty props:

```c
ECS_MODULE_DECLARE(rendering, {});
ECS_MODULE_DEFINE(rendering);

void rendering_import(ecs_world_t *world, const rendering_props_t *props) {
    (void)props;
    /* Register rendering systems and observers. */
}

ECS_MODULE_IMPORT(world, rendering, {});
```

## What Gets Captured

During `ECS_MODULE_IMPORT()`, SIECS records registrations made through the
normal public API:

```c
ECS_COMPONENT_REGISTER(world, Position);
ecs_system(world, { /* ... */ });
ecs_observer(world, { /* ... */ });
```

Those ids are appended to the active module. User code does not need a separate
`module_add_*` call.

Nested imports are supported. If one module imports another, registrations made
after the nested import continue to belong to the parent module.

## Enable And Disable

Modules are enabled by default. Disabling a module disables only the systems and
observers recorded by that module:

```c
ecs_module_disable(world, Physics);
ecs_module_enable(world, Physics);
```

Components and relations stay registered. Existing component data stays valid.
Component hooks still run because hooks belong to components, not to the module
scheduler.

Import a module disabled with the generic API:

```c
physics_props_t props = { .gravity = 9.81f };

ecs_module_id_t Physics = ecs_module(world, {
    .name = "physics",
    .id = &ecs_id(physics),
    .import = ecs_id(physics_import_wrapper),
    .desc = &props,
    .desc_size = sizeof(props),
    .disabled = true,
});
```

## Idempotency

Imports are idempotent for the active world. Importing the same module twice
returns the same module id and does not register duplicate systems or observers:

```c
ecs_module_id_t first = ECS_MODULE_IMPORT(world, physics, { .gravity = 9.81f });
ecs_module_id_t second = ECS_MODULE_IMPORT(world, physics, { .gravity = 1.0f });

/* first == second */
```

The first props value wins. Later imports with different props values do not
reconfigure an already imported module.

SIECS stores the imported id in the generated `ecs_id(module_name)` symbol and
resets it during `ecs_fini()`. A later world can import the module again, but the
same typed module is not supported in two live worlds at the same time.

## Performance Model

Modules do not add checks to query iteration, table matching, or system
callbacks. Capture happens only during registration.

Enable and disable are linear over the ids stored in the module:

- systems are toggled with `ecs_system_enable()` and `ecs_system_disable()`
- observers are toggled with `ecs_observer_enable()` and `ecs_observer_disable()`
- components are retained for ownership/debug information and are not toggled

The runtime handle is `ecs_module_id_t`; no string lookup is needed to enable or
disable a module.
