# C++: scenes and introspection

```cpp
void *data = nullptr;
std::size_t size = 0;
if (ecs_save_memory(&data, &size)) {
    if (ecs_scene_validate(data, size)) {
        const bool loaded = ecs_load_memory(data, size);
        (void)loaded; // Apply the application's failure policy.
    }
    ecs_scene_free(data);
}
```

The memory scene API is shared C ABI; do not invent a C++ memory wrapper.
File operations have typed `ecs::save(std::string)` and `ecs::load(std::string)`
helpers. Handle failure and always free a successful save buffer.

Native C++ data needs reflected fields when the serializer must traverse it.
Shared macro-declared C types retain their reflection metadata. Registry and
reflection inspection use the C APIs from the [C scene guide](../c/scenes.md).
Typed names are available through `ecs::type_name<T>()`.
For registry enumeration, metadata and reflected-field inspection, use
[introspection](../introspection.md).

[Ownership, schema, and serialization constraints](../scenes.md) · [API](../../api/cpp-scenes.md)
