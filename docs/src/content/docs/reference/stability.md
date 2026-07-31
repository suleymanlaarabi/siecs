---
title: API stability policy
description: Compatibility guarantees and review rules for the SIECS public API.
---

This policy applies to every declaration reachable from `siecs.h` or
`siecs/cpp.hpp`. Internal headers under `src/` and names in `ecs::detail` are
not public API and may change without a compatibility promise.

## Stability levels

**Stable** is the default. A stable symbol keeps its source spelling, calling
convention, ownership rules, and observable edge-case behavior throughout a
major version. A change to any of those is a breaking change and requires a
major-version migration note. Additive symbols are allowed in minor releases.

**Experimental** symbols are explicitly identified in their declaration and in
this page. They are available for adoption and feedback but may change or be
removed in a minor release. Experimental symbols must not be used as the only
path to correctness in a production integration.

**Deprecated** symbols remain source-compatible for one major release. Their
header comment names the replacement and the release in which removal is
allowed. Deprecation is never inferred from a spelling or from an alias alone.

## Current classifications

| Surface | Level | Contract |
| --- | --- | --- |
| Core world/entity/component/query/system APIs | Stable | Handles belong to the active world; pointers returned by accessors are borrowed and invalidated by mutation that migrates storage. |
| Typed C++ wrappers (`ecs::entity`, `query_handle`, `system`, `observer`, resource handles) | Stable | Wrappers do not outlive the world; RAII query handles own only their query id; callback views are valid only for the callback. |
| `ecs_in_source`, `ecs_not_source`, `ecs_filter_source`, `ecs::relation_source`, `ecs::instantiate` | Experimental | The relation helpers expose reverse-relation storage; `instantiate` currently creates only an empty entity with an `IsA` link, not a deep clone. Both semantics are under active design. |
| `ecs_resource`, `ecs_resource_read`, `ecs_try_resource*`, `On*` aliases | Deprecated | Use the `ecs_get_resource*` family and `Ecs*` phase names. Removal requires a major release. |

## Required review for a public API change

The change must update the declaration contract (purpose, preconditions,
ownership/lifetime, and edge cases), its stability classification, a focused
test, and the API reference when user-visible behavior changes. CI runs
`make check-api-docs`; it invokes `clang-doc` on the C++ headers and checks C
declarations and function-like macros directly, so overloads and templates are
checked individually rather than through a manually maintained symbol list.
