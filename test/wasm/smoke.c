#include <siecs.h>

#include <stdbool.h>

ECS_COMPONENT_DECLARE(WasmPosition, { int value; });
ECS_COMPONENT_DEFINE(WasmPosition);

ECS_RESOURCE_DECLARE(WasmResource, { int value; });
ECS_RESOURCE_DEFINE(WasmResource);

static void wasm_system(ecs_iter_t *it) {
    WasmPosition *position = ecs_field(it, 0);
    WasmResource *resource = ecs_get_resource(WasmResource);

    for (uint32_t i = 0; i < it->count; i++) {
        position[i].value += resource->value;
    }
}

int siecs_wasm_smoke(void) {
    ecs_init_w_features(&(ecs_world_feat_desc_t){ .target_fps = 1 });
    ECS_COMPONENT_REGISTER(WasmPosition);
    ECS_RESOURCE_REGISTER(WasmResource);

    const ecs_component_info_t *info = ecs_component_info(ecs_id(WasmPosition));
    if (!info || !info->reflection || info->type == 0) {
        ecs_fini();
        return 1;
    }

    ecs_set_resource(WasmResource, { .value = 5 });
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, WasmPosition, { .value = 7 });

    ecs_system({
        .name = "WasmSmoke",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_inout(WasmPosition) } },
        .callback = wasm_system,
    });

    if (!ecs_progress()) {
        ecs_fini();
        return 2;
    }

    WasmPosition *position = ecs_get(entity, WasmPosition);
    const DeltaTime *delta_time = ecs_get_resource_read(DeltaTime);
    bool valid = position && position->value == 12 && delta_time &&
                 ecs_is_alive(entity);
    ecs_fini();
    return valid ? 0 : 3;
}
