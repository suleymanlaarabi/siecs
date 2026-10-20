# C: scenes and introspection

```c
void *data = NULL;
size_t size = 0;
if (ecs_save_memory(&data, &size)) {
    if (ecs_scene_validate(data, size)) {
        bool loaded = ecs_load_memory(data, size);
        (void)loaded; /* Apply the application's failure policy. */
    }
    ecs_scene_free(data);
}
```

Loading appends instances; configure compatible registrations first.
Use `ecs_save(path)`/`ecs_load(path)` for files and check their boolean result.
Scene data and ordinary allocated path strings have different release functions.

Inspect schema with `ecs_component_count/lookup/name/info`,
`ecs_resource_count/find/name/info`, and `ecs_relation_count/lookup/info`.
Metadata includes reflected type handles/descriptors and lifecycle policy.
Use Sireflect on those handles for structured tooling, not pointer arithmetic
based on guessed component layout.
The [introspection guide](../introspection.md) gives registry iteration, metadata
fields and a reflected-field inspection example; no runtime sources are needed.

[Ownership, schema, and serialization constraints](../scenes.md) · [API](../../api/c-scenes.md)
