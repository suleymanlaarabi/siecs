# Reflection, scenes, and serialization

Introspection exposes component/resource counts, names, ids, sizes, reflection,
and relation descriptors. Reflection describes registered data; register the
schema before using tools or instantiating scenes. Do not serialize raw process
addresses or assume arbitrary C++ layouts are reflected automatically.

Scenes store entity components and relations, not resources, systems, observers,
or module registrations. Memory save returns a caller-owned buffer; release it
with `ecs_scene_free`. Validation reads the active schema without mutating the
world. Loading creates fresh handles without clearing the world; supported entity
references/relations are remapped through scene-local identity. Restored names
still obey collision replacement, so repeated loads need not coexist. Failure
during application can leave created entities; loading is not transactional.

The format records numeric component/relation ids, not a portable named schema.
Reproduce compatible ids, layouts, reflection and policies before validation or
loading, typically with the same module/registration order. Validation cannot
prove equivalence to a different runtime schema. Native scenes are runtime assets;
use a separate application format for long-lived editable content when needed.

Reflected nested structs/arrays, C strings, enums, and entity-reference roles
follow their registered metadata. Reflected arbitrary/function pointers are
rejected. Opaque trivial storage can be copied as raw bytes: hidden pointers or
entity handles are not fixed up. Reflect every field that needs structured
serialization; owned C++ containers are not automatically supported by lifecycle
hooks or `reflected(...)` alone.

File helpers return success/failure. Check the result and choose the
application's unload/replace policy explicitly. Core scenes are distinct from
the optional REST/JSON editing addon.
