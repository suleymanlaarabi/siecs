# Runtime schema and registry inspection

These C ABI APIs also serve C++; the typed facade supplies ids, not a separate
registry iterator. Initialize/import the schema first. All metadata pointers
are world-owned and expire at finalization; do not free or mutate them.

| Registry | Find by name | Inspect one id | Range bound |
| --- | --- | --- | --- |
| Components | `ecs_component_lookup(name)` | `ecs_component_info(cid)`, `ecs_component_name(cid)` | `ecs_component_count()` |
| Resources | `ecs_resource_find(name)` | `ecs_resource_info(rid)`, `ecs_resource_name(rid)` | `ecs_resource_count()` |
| Relations | `ecs_relation_lookup(name)` | `ecs_relation_info(rid)` | `ecs_relation_count()` |

Lookup/find returns zero on absence; `*_info` returns NULL for out-of-range ids.
Skip unfilled entries too (component/resource info can be NULL; relation names
can be NULL). These counts are id-space upper bounds, not the number of application
types. Skip id zero and inspect each entry; internal relation storage entries and
reserved ids are not a list of serializable application components.

```c
for (uint32_t id = 1; id < ecs_component_count(); id++) {
    const ecs_component_info_t *info = ecs_component_info((ecs_component_t)id);
    if (!info || !info->name) continue;
    /* info->size, info->type, info->reflection, info->inheritance */
}
```

Component info contains `.name`, `.size`, reflected `.type` handle,
`.reflection` descriptor, `.inheritance`, and debug-only `.mutation`. Resource
info has `.name`, `.size`, `.type`, `.reflection`. Relation info has `.name` and
`.desc` (`storage`, `on_delete_target`, `acyclic`). A registered resource id does
not imply a live resource value; use `ecs_has_resource_rid` after confirming
`ecs_resource_is_registered_rid`.

## Reflected fields

```c
const ecs_component_info_t *component = ecs_component_info(cid);
if (component && component->type != SIREFLECT_INVALID_HANDLE) {
    const sireflect_type_info_t *type = sireflect_type_info(component->type);
    if (type) {
        for (size_t i = 0; i < type->fields.field_count; i++) {
            const sireflect_field_info_t *field = &type->fields.fields[i];
            /* field->name, field->type, field->offset describe the field. */
            (void)field;
        }
    }
}
```

`field->type` can be nested/array/pointer metadata, not necessarily a scalar.
Check its reflected kind/size before reading or editing a field. Obtain entity
storage with component APIs, then apply the registered offset; don't infer
offsets from declaration order or cast an unknown dynamic component to a guessed
struct. Preserve lifecycle invariants for owned fields. Tools can use
`ecs_modified_cid` after a valid direct edit to notify observers; this does not
invoke on_set hooks. SetOnly components require whole-value setters instead.

In C++, obtain ids with `ecs::component<T>().id()`, `ecs::resource<T>().id()`
and `ecs::relation<R>()`. `ecs::type_name<T>()` exposes the compiler-derived
native registration name. Reflection inclusion does not mean arbitrary native
type fields/containers are known; declare supported fields explicitly.

System/phase/module names are available through `ecs_system_name`,
`ecs_phase_name`, `ecs_module_name` for known live ids. There is no public
system/observer/module enumeration API. Event payload layout is a producer/
consumer contract, not an event schema registry.

[Dynamic registration](c/registration.md) · [Scene constraints](scenes.md) · [C component API](../api/c-components.md)
