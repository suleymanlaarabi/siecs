# Modules

A module packages reusable component/resource/relation registrations, systems,
observers, and queries. Import its own dependencies inside the module so callers
need not reconstruct registration order. Keep its data contract small.

Typed/id-backed import is idempotent: the first properties/arguments win.
Raw `ecs_module_init` without id storage creates a new registration each time.
Import properties are borrowed only during the callback; copy anything needed
later into resources or owned callback state. Module ids reset at shutdown.

Enable/disable toggles captured systems and observers, not component registration
or every entity the module created. Dependencies are import boundaries, not
implicit scheduler barriers. Declare same-phase system ordering separately.
Nested imports own their registrations independently; toggling a parent does
not recursively toggle dependency modules.

Standalone queries created during import still need explicit lifetime management;
module capture tracks systems and observers, not those queries. Destroy C++ query
owners before world finalization.

Dynamic modules load a shared library through `ecs_module_load(path)`; pass its
path without the platform suffix (`.so`, `.dylib`, `.dll`), which is appended. The
library exports `ecs_module_import(void)`, registers into the current world,
and must share the runtime and compatible C declarations. Use one runtime across
binary boundaries so typed identities agree. Dynamic loading is not hot reload.
