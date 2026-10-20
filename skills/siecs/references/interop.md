# C/C++ and library integration

The runtime is C17; the typed facade is C++20. Source-layout C++ applications
include `<siecs/cpp.hpp>`. The standalone `siecs.h` embeds that facade; compile
`siecs.c` as C and link its object into the C++ application.

For shared C/C++ data, declare types in a shared header with the supported
component/resource/relation macros, define them once, then register in the world.
Their C++ traits preserve the C identity. Use the `_CPP` declaration variants
for C++ methods on a shared layout; do not add hidden instance fields.

Export ids, descriptors, and module callbacks across shared-library boundaries.
`SIECS_API` exports the runtime; `SIECS_PUBLIC_API` handles declarations owned by
other packages. `siecs_STATIC` selects static linking and `siecs_EXPORTS` is a
runtime-build setting. Keep headers and linked binaries from the same build.

`SIECS_NO_CPP` suppresses automatic C++ inclusion when intentionally using the
C ABI from C++; ordinary typed applications should leave it unset. Reflection
is always enabled (`SIECS_HAS_META`); vector support is required (`SICORE_VEC`).
`SICORE_NO_VEC` is unsupported.

Public macro-expansion helpers and generated id/reflection symbols appear in
the catalogues for completeness. Use their declaration macros rather than
handcrafting generated names. C++ callback traits, `type_name`, and type iteration
support custom adapters; application code normally uses typed builders.

Only the current core API is described. Keep the skill and application's API
revision aligned; do not introduce previous-API aliases or format migration plumbing.
