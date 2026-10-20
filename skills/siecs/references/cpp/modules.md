# C++: modules

```cpp
struct Motion {
    static void import() {
        ecs::system("Move").each([](Position &p, const Velocity &v) {
            p.x += v.x;
        });
    }
};
auto motion = ecs::import<Motion>();
```

Native modules expose static `import(args...)`; `ecs::import<Module>(args...)`
passes arguments during the first import. Later calls reuse registration.
`ecs::module<Module>()` returns an empty handle before import; check it before
`enable/disable/is_enabled`. `module_ref` is non-owning.

C-declared modules use the same typed `import` with `Module::props_t`, or default
props when omitted. Their generated traits preserve the C module identity.
Dynamic library imports use the shared `ecs_module_load` C API; a native C++
module type alone is not a library loader.

[Ownership, properties, and module lifetime](../modules.md) · [API](../../api/cpp-modules.md)
