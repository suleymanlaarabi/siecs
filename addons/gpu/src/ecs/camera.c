#include "render/render_internal.h"
void sigpu_camera(
    float x,
    float y,
    float z,
    float target_x,
    float target_y,
    float target_z,
    float fov
) {
    SIGPU_RENDERVIEW->camera.position = (sigpu_vec3_t){ x, y, z };
    SIGPU_RENDERVIEW->camera.target = (sigpu_vec3_t){ target_x, target_y, target_z };
    SIGPU_RENDERVIEW->camera.fov = fminf(fmaxf(fov, 1.0f), 179.0f);
}

static void update_camera(ecs_iter_t *it) {
    const field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    const field_GlobalOrientation3d orientations = FIELD(GlobalOrientation3d, it, 1);
    const Camera *cameras = ecs_field(it, 2);
    const ptrdiff_t camera_stride = ecs_field_is_shared(it, 2) ? 0 : 1;
    for (uint32_t index = 0; index < it->count; index++) {
        const GlobalPosition3d position = AT(positions, index);
        const GlobalOrientation3d orientation = AT(orientations, index);
        const Direction3d forward = sispatial_forward_3d(&orientation);
        sigpu_camera(
            position.x,
            position.y,
            position.z,
            position.x + forward.x,
            position.y + forward.y,
            position.z + forward.z,
            cameras[index * camera_stride].fov
        );
    }
}

static void prepare_camera(ecs_iter_t *it) {
    if (SIGPU_FRAMECONTEXT->frame_width && SIGPU_FRAMECONTEXT->frame_height)
        sigpu_view_prepare(
            (float)SIGPU_FRAMECONTEXT->frame_width / SIGPU_FRAMECONTEXT->frame_height
        );
}

ecs_system_id_t sigpu_camera_register(void) {
    ecs_system_id_t select = ecs_system({
        .name = "SelectCamera", .phase = EcsPreRender,
        .query.components = {
            { .id = ecs_id(GlobalPosition3d), .access = EcsIn },
            { .id = ecs_id(GlobalOrientation3d), .access = EcsIn },
            { .id = ecs_id(Camera), .access = EcsIn },
        },
        .callback = update_camera, .main_thread_only = true,
    });
    return ecs_system(
        {
            .name = "PrepareCamera",
            .phase = EcsPreRender,
            .after = { select },
            .callback = prepare_camera,
            .main_thread_only = true,
        }
    );
}
