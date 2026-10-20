# C17 API: scenes

[Usage and contracts](../references/c/scenes.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_load` — `bool ecs_load(const char *path)`
- `ecs_load_memory` — `bool ecs_load_memory(const void *data, size_t size)`
- `ecs_save` — `bool ecs_save(const char *path)`
- `ecs_save_memory` — `bool ecs_save_memory(void **data_out, size_t *size_out)`
- `ecs_scene_free` — `void ecs_scene_free(void *data)`
- `ecs_scene_validate` — `bool ecs_scene_validate(const void *data, size_t size)`
