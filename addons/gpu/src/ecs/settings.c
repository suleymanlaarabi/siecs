#include "input/input.h"
#include "render/render_internal.h"
static sigpu_color_t to_sigpu(Color color) {
    return (sigpu_color_t){ color.r, color.g, color.b, color.a };
}
static SDL_FColor linear_color(sigpu_color_t color) {
    return (SDL_FColor){
        SIGPU_RENDERSETTINGS->linear_lut[color.r] / 255.0f,
        SIGPU_RENDERSETTINGS->linear_lut[color.g] / 255.0f,
        SIGPU_RENDERSETTINGS->linear_lut[color.b] / 255.0f,
        color.a / 255.0f,
    };
}

void sigpu_sky(sigpu_color_t color) {
    SIGPU_RENDERSETTINGS->sky_linear = linear_color(color);
    SIGPU_RENDERSETTINGS->sky_srgb = (SDL_FColor){
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f,
    };
}

void sigpu_sun(float dx, float dy, float dz, sigpu_color_t color, float intensity) {
    SIGPU_RENDERSETTINGS->sun_direction = sigpu_vec3_normalize((sigpu_vec3_t){ dx, dy, dz });
    SIGPU_RENDERSETTINGS->sun_color = linear_color(color);
    SIGPU_RENDERSETTINGS->sun_intensity = intensity;
}

void sigpu_ambient(sigpu_color_t color, float intensity) {
    SIGPU_RENDERSETTINGS->ambient_color = linear_color(color);
    SIGPU_RENDERSETTINGS->ambient_intensity = intensity;
}

void sigpu_fog(sigpu_color_t color, float start, float end) {
    SIGPU_RENDERSETTINGS->fog_color = linear_color(color);
    SIGPU_RENDERSETTINGS->fog_start = start;
    SIGPU_RENDERSETTINGS->fog_end = end;
    SIGPU_RENDERSETTINGS->fog_enabled = end > start;
}

void sigpu_shadows(bool enabled, float distance) {
    SIGPU_RENDERSETTINGS->shadows_enabled = enabled;
    SIGPU_RENDERSETTINGS->shadow_distance = distance;
}

void sigpu_bloom(bool enabled, float threshold, float intensity) {
    SIGPU_RENDERSETTINGS->bloom_enabled = enabled;
    SIGPU_RENDERSETTINGS->bloom_threshold = threshold;
    SIGPU_RENDERSETTINGS->bloom_intensity = intensity;
}

void sigpu_msaa(int samples) { sigpu_sample_count_set(samples); }

static void set_sky(const void *ptr) {
    const Sky *sky = ptr;
    sigpu_sky(to_sigpu(sky->color));
}
static void set_sun(const void *ptr) {
    const Sun *sun = ptr;
    sigpu_sun(sun->x, sun->y, sun->z, to_sigpu(sun->color), sun->intensity);
}
static void set_ambient(const void *ptr) {
    const AmbientLight *ambient = ptr;
    sigpu_ambient(to_sigpu(ambient->color), ambient->intensity);
}
static void set_fog(const void *ptr) {
    const Fog *fog = ptr;
    sigpu_fog(to_sigpu(fog->color), fog->start, fog->end);
}
static void set_shadows(const void *ptr) {
    const Shadows *shadows = ptr;
    sigpu_shadows(shadows->enabled, shadows->distance);
    sigpu_render_schedule_set_shadows(shadows->enabled);
}
static void set_camera_clip(const void *ptr) {
    const CameraClip *clip = ptr;
    if (clip->near_plane > 0.0f && clip->far_plane > clip->near_plane) {
        SIGPU_RENDERVIEW->camera.near_plane = clip->near_plane;
        SIGPU_RENDERVIEW->camera.far_plane = clip->far_plane;
    }
}
static void set_multisampling(const void *ptr) {
    const Multisampling *multisampling = ptr;
    sigpu_msaa(multisampling->samples);
}
static void set_bloom(const void *ptr) {
    const BloomSettings *bloom = ptr;
    sigpu_bloom(bloom->enabled, bloom->threshold, bloom->intensity);
    sigpu_render_schedule_set_bloom(bloom->enabled);
}

ECS_COMPONENT_DEFINE(Color, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Cuboid, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Cylinder, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Sphere, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Bloom, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Camera);
ECS_RESOURCE_DEFINE(WindowConfig);
ECS_RESOURCE_DEFINE(Sky, .on_set = set_sky);
ECS_RESOURCE_DEFINE(Sun, .on_set = set_sun);
ECS_RESOURCE_DEFINE(AmbientLight, .on_set = set_ambient);
ECS_RESOURCE_DEFINE(Fog, .on_set = set_fog);
ECS_RESOURCE_DEFINE(Shadows, .on_set = set_shadows);
ECS_RESOURCE_DEFINE(Multisampling, .on_set = set_multisampling);
ECS_RESOURCE_DEFINE(CameraClip, .on_set = set_camera_clip);
ECS_RESOURCE_DEFINE(BloomSettings, .on_set = set_bloom);

#define RESOURCE_REFLECTION(rname, ...)                                                            \
    static const sireflect_struct_desc_t reflection_##rname = { .name = #rname,                    \
                                                                .fields = #__VA_ARGS__,            \
                                                                .size = sizeof(rname),             \
                                                                .align = _Alignof(rname) }
RESOURCE_REFLECTION(WindowConfig, {
    int width;
    int height;
    const char *title;
});
RESOURCE_REFLECTION(Sky, { Color color; });
RESOURCE_REFLECTION(Sun, {
    float x;
    float y;
    float z;
    Color color;
    float intensity;
});
RESOURCE_REFLECTION(AmbientLight, {
    Color color;
    float intensity;
});
RESOURCE_REFLECTION(Fog, {
    Color color;
    float start;
    float end;
});
RESOURCE_REFLECTION(Shadows, {
    bool enabled;
    float distance;
});
RESOURCE_REFLECTION(Multisampling, { int samples; });
RESOURCE_REFLECTION(CameraClip, {
    float near_plane;
    float far_plane;
});
RESOURCE_REFLECTION(BloomSettings, {
    bool enabled;
    float threshold;
    float intensity;
});
#undef RESOURCE_REFLECTION

typedef struct {
    const char *name;
    ecs_resource_t *id;
    ecs_resource_desc_t *desc;
    const sireflect_struct_desc_t *reflection;
    sireflect_handle_t type;
} rendering_resource;
#define RESOURCE_ENTRY(name) { #name, &ecs_id(name), &ecs_id(name##_desc), &reflection_##name, 0 }
static rendering_resource rendering_resources[] = {
    RESOURCE_ENTRY(WindowConfig),  RESOURCE_ENTRY(Sky),           RESOURCE_ENTRY(Sun),
    RESOURCE_ENTRY(AmbientLight),  RESOURCE_ENTRY(Fog),           RESOURCE_ENTRY(Shadows),
    RESOURCE_ENTRY(Multisampling), RESOURCE_ENTRY(BloomSettings), RESOURCE_ENTRY(CameraClip),
};
#undef RESOURCE_ENTRY

void sigpu_settings_register(void) {
    for (size_t index = 0; index < sizeof(rendering_resources) / sizeof(*rendering_resources);
         index++) {
        rendering_resource *resource = &rendering_resources[index];
        ecs_resource_register(resource->id, resource->desc);
        resource->type = sireflect_register_struct(resource->reflection);
    }
}
uint16_t sigpu_resource_id(const char *name) {
    for (size_t index = 0; index < sizeof(rendering_resources) / sizeof(*rendering_resources);
         index++) {
        if (strcmp(name, rendering_resources[index].name) == 0)
            return *rendering_resources[index].id;
    }
    return sigpu_input_resource_id(name);
}

sireflect_handle_t sigpu_resource_type(const char *name) {
    for (size_t index = 0; index < sizeof(rendering_resources) / sizeof(*rendering_resources);
         index++) {
        if (strcmp(name, rendering_resources[index].name) == 0)
            return rendering_resources[index].type;
    }
    return sigpu_input_resource_type(name);
}
