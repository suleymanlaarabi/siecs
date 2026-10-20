# C++: shared declarations and support APIs

Use the shared C declarations when a type crosses language/library boundaries.
Their traits make `entity.set(Position{...})`, `ecs::resource<Settings>()`,
`ecs::relation<OwnedBy>()`, and `ecs::import<Module>()` use the C identities.

`ECS_COMPONENT_DECLARE_CPP` and `ECS_RESOURCE_DECLARE_CPP` accept shared fields
and C++-only methods. `ECS_COMPONENT_DECLARE_CPP_REFLECTED` also specifies the
reflected fields separately. Define descriptors once in the owning package.

```cpp
/* In a C/C++ shared header, after including siecs.h: */
ECS_COMPONENT_DECLARE_CPP(Position,
    ECS_CPP_FIELDS(float x; float y;),
    ECS_CPP_METHODS(float length_squared() const { return x*x + y*y; })
);
/* In exactly one C implementation file: ECS_COMPONENT_DEFINE(Position); */
```

`ECS_CPP_FIELDS/METHODS` group macro arguments, including declarations with commas.
For `ECS_COMPONENT_DECLARE_CPP_REFLECTED`, provide the layout block, reflected
field block, then method block. Describe only serializable fields in the reflected
block, preserving their actual layout. The resource CPP macro takes layout and
methods; there is no separate reflected-resource CPP variant.

Native reflection can use:

```cpp
struct Position {
    reflected(float x;);
};
```

`type_name<T>()` returns a compiler-derived registration name; consistently
compile shared types across binaries. `function_traits` and `for_each_type`
support callback adaptation. Registration support functions and generated
macros appear in the catalogue, but normal applications should use the typed
builders. Do not call into `ecs::detail` for ordinary application features.

For public tuple-based type dispatch, `ecs::for_each_type<std::tuple<A, B>>(
[]<typename T>() { /* Register/use T. */ })` visits types at compile time;
`ecs::function_traits<Callable>::args_tuple` and `return_type` expose a supported
concrete callback signature. These helpers do not infer generic/overloaded or
noexcept-qualified callback signatures; wrap those with a typed lambda.

For operations without a typed facade, use the shared C ABI and explicit
typed ids. Compile the runtime as C and keep every linked binary's contracts
aligned.

[Configuration, ownership across binaries, and support symbols](../interop.md) · [API](../../api/cpp-interop.md)
