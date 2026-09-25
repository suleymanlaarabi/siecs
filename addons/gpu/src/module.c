#include "input/input.h"
#include "interaction/interaction.h"
#include "render/render_internal.h"
#include <stdlib.h>
#include <string.h>

ECS_RESOURCE_DEFINE(GpuContext);
ECS_RESOURCE_DEFINE(FrameContext);
ECS_RESOURCE_DEFINE(GpuPipelines);
ECS_RESOURCE_DEFINE(GpuTargets);
ECS_RESOURCE_DEFINE(RenderQueue);
ECS_RESOURCE_DEFINE(StaticRenderCache);
ECS_RESOURCE_DEFINE(RenderStats);
ECS_RESOURCE_DEFINE(RenderView);
ECS_RESOURCE_DEFINE(RenderSettings);
ECS_MODULE_DEFINE(sigpu);

static const int default_multisampling = 4;
_Static_assert(sizeof(sigpu_axis_instance_t) == 32, "owned axis instance layout");
_Static_assert(sizeof(sigpu_rotated_instance_t) == 40, "owned rotated instance layout");
_Static_assert(sizeof(sigpu_shared_axis_instance_t) == 24, "shared axis instance layout");
_Static_assert(sizeof(sigpu_shared_rotated_instance_t) == 32, "shared rotated instance layout");
static void fini_rendering(void *data) {
    sigpu_render_schedule_fini();
    sigpu_fini();
    sigpu_render_schedule_reset();
}
void sigpu_import(const sigpu_props_t *props) {
    const sigpu_props_t defaults = {
        .title = "SIECS",
        .width = 1280,
        .height = 800,
        .samples = default_multisampling,
    };
    sigpu_props_t config = props ? *props : defaults;
    if (!config.title || !config.title[0])
        config.title = defaults.title;
    if (config.width <= 0)
        config.width = defaults.width;
    if (config.height <= 0)
        config.height = defaults.height;
    if (config.samples <= 0)
        config.samples = defaults.samples;

    ECS_MODULE_IMPORT(sispatial, { 0 });
    ECS_COMPONENT_REGISTER(Color, Cuboid, Cylinder, Sphere, Bloom, Camera);
    sigpu_settings_register();
    ECS_RESOURCE_REGISTER(GpuContext);
    ecs_set_resource(GpuContext, { 0 });
    ECS_RESOURCE_REGISTER(FrameContext);
    ecs_set_resource(FrameContext, { 0 });
    ECS_RESOURCE_REGISTER(GpuPipelines);
    ecs_set_resource(GpuPipelines, { 0 });
    ECS_RESOURCE_REGISTER(GpuTargets);
    ecs_set_resource(GpuTargets, { 0 });
    ECS_RESOURCE_REGISTER(RenderQueue);
    ecs_set_resource(RenderQueue, { 0 });
    ECS_RESOURCE_REGISTER(StaticRenderCache);
    ecs_set_resource(StaticRenderCache, { 0 });
    ECS_RESOURCE_REGISTER(RenderStats);
    ecs_set_resource(RenderStats, { 0 });
    ECS_RESOURCE_REGISTER(RenderView);
    ecs_set_resource(RenderView, { 0 });
    ECS_RESOURCE_REGISTER(RenderSettings);
    ecs_set_resource(RenderSettings, { 0 });
    ecs_set_resource(
        WindowConfig,
        { .width = config.width, .height = config.height, .title = config.title }
    );
    const WindowConfig *window = ecs_get_resource_read(WindowConfig);
    sigpu_materials_init();
    sigpu_init(window->title, window->width, window->height, config.samples);
    sigpu_camera(0.0f, 2.0f, -6.0f, 0.0f, 0.0f, 0.0f, 60.0f);
    sigpu_input_init();
    ecs_set_resource(Sky, { .color = { 13, 13, 20, 255 } });
    ecs_set_resource(
        Sun,
        { .x = -1.0f, .y = -2.0f, .z = 1.0f, .color = { 255, 245, 220, 255 }, .intensity = 1.0f }
    );
    ecs_set_resource(AmbientLight, { .color = { 255, 255, 255, 255 }, .intensity = 0.2f });
    ecs_set_resource(Fog, { .color = { 13, 13, 20, 255 }, .start = 0.0f, .end = 0.0f });
    ecs_set_resource(Shadows, { .enabled = false, .distance = 35.0f });
    ecs_set_resource(Multisampling, { .samples = config.samples });
    ecs_set_resource(CameraClip, { .near_plane = 0.1f, .far_plane = 1000.0f });
    ecs_set_resource(BloomSettings, { .enabled = true, .threshold = 0.0f, .intensity = 1.0f });
    const ecs_system_id_t input_system = sigpu_input_system();
    const ecs_system_id_t camera_system = sigpu_camera_register();
    sigpu_interaction_init(camera_system);
    const ecs_system_id_t static_cache_system = sigpu_static_chunks_register(camera_system);
    sigpu_bounds_register(static_cache_system);
    sigpu_primitives_register();
    sigpu_render_schedule_register(input_system);
    ecs_at_fini({ .callback = fini_rendering });
}

uint16_t sigpu_component_id(const char *name) {
#define COMPONENT_ID(type)                                                                         \
    if (strcmp(name, #type) == 0)                                                                  \
    return ecs_id(type)
    COMPONENT_ID(Color);
    COMPONENT_ID(Cuboid);
    COMPONENT_ID(Cylinder);
    COMPONENT_ID(Sphere);
    COMPONENT_ID(Bloom);
    COMPONENT_ID(Camera);
    COMPONENT_ID(PointerEvents);
    COMPONENT_ID(Position2d);
    COMPONENT_ID(Velocity2d);
    COMPONENT_ID(GlobalPosition2d);
    COMPONENT_ID(Scale2d);
    COMPONENT_ID(GlobalScale2d);
    COMPONENT_ID(Rotation2d);
    COMPONENT_ID(GlobalRotation2d);
    COMPONENT_ID(Position3d);
    COMPONENT_ID(Velocity3d);
    COMPONENT_ID(GlobalPosition3d);
    COMPONENT_ID(Rotation3d);
    COMPONENT_ID(GlobalOrientation3d);
    COMPONENT_ID(Scale3d);
    COMPONENT_ID(GlobalScale3d);
    COMPONENT_ID(Static);
#undef COMPONENT_ID
    return sigpu_interaction_component_id(name);
}
