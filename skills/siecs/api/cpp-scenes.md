# C++20 API: scenes

[Usage and contracts](../references/cpp/scenes.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C scenes reference](c-scenes.md). Public base members also apply to derived builders.

## Functions

- `ecs::load` — `inline bool load(const std::string &path)`
- `ecs::save` — `inline bool save(const std::string &path)`
