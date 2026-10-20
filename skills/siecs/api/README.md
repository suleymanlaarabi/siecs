# API catalogue

Core API reference based on the public headers at revision `6f0c925`.

Select a subject; search within its file for the symbol you need. The catalogue
includes public descriptors and fields, enum values, helper macros, C ABI
functions, and C++ overloads. Application patterns live in the usage guides.

| Subject | C17 | C++20 |
| --- | --- | --- |
| world | [C](c-world.md) | [C++](cpp-world.md) |
| entities | [C](c-entities.md) | [C++](cpp-entities.md) |
| components | [C](c-components.md) | [C++](cpp-components.md) |
| resources | [C](c-resources.md) | [C++](cpp-resources.md) |
| queries | [C](c-queries.md) | [C++](cpp-queries.md) |
| systems | [C](c-systems.md) | [C++](cpp-systems.md) |
| relations | [C](c-relations.md) | [C++](cpp-relations.md) |
| inheritance | [C](c-inheritance.md) | [C++](cpp-inheritance.md) |
| events | [C](c-events.md) | [C++](cpp-events.md) |
| modules | [C](c-modules.md) | [C++](cpp-modules.md) |
| scenes | [C](c-scenes.md) | [C++](cpp-scenes.md) |
| interop | [C](c-interop.md) | [C++](cpp-interop.md) |

C member paths use `Type.field` to identify descriptor fields. C enum values
are global identifiers. C++ methods are grouped with their owning type; inherited
`query_filter` methods are in the queries catalogue. Debug-only declarations
are marked. Macro definitions/expansion internals are intentionally omitted.

Types and helpers from `ecs::detail`, private/protected members, internal runtime
headers, include guards, and temporary preprocessor switches are implementation
details. Public support symbols are listed under interop; ordinary application
code uses the supported declaration macros and typed builders.

Entity methods are placed by capability: data access under components,
inheritance under inheritance, edges under relations, and observation under
events. Identity and enabled-state methods remain under entities.

Configuration supplied by the application includes `SIECS_NO_CPP` and
`siecs_STATIC`; the runtime build uses `siecs_EXPORTS`. Reflection is always
enabled and vector support is required. See [integration](../references/interop.md).

Shared selection APIs have usage tables for [C](../references/c/filters.md) and
[C++](../references/cpp/filters.md). They apply to queries, systems and observers;
the C++ [callback parameter guide](../references/cpp/parameters.md) covers binding
and extension hooks. Use these guides for behavior, not implementation sources.

Keep the package aligned with the consuming application's API revision. When
maintaining it, update both syntax and behavior for a changed contract.
