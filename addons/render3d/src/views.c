#include "render3d_internal.h"
#include <stdio.h>

/*
 * Views and the standard graph (RFC section 12.1). Every frame, in
 * PreRender, the cameras rendering to each surface get their GpuView and
 * uniforms; the passes of a surface are regenerated only when its cameras
 * or their structural settings (shadows, bloom, MSAA) change.
 */

#define R3D_MAX_VIEWS 8
#define R3D_MAX_OWNED 64

typedef struct r3d_surface_s r3d_surface_t;

/* user_data of a pass callback. */
typedef struct {
    r3d_surface_t *surface;
    uint32_t index;
    const char *uniform;
    float parameters[4];
    /* 0: static; 1: texel of a source twice the target; 2 and 3:
     * horizontal and vertical blur offsets. */
    uint8_t texel;
} r3d_pass_data_t;

typedef struct {
    ecs_entity_t camera;
    Camera3d desc;
    r3d_camera_t view;
    r3d_view_uniform_t uniform;
    float sky[4];
} r3d_view_t;

struct r3d_surface_s {
    ecs_entity_t surface;
    uint64_t signature;
    bool seen;
    ecs_entity_t owned[R3D_MAX_OWNED];
    uint32_t owned_count;
    r3d_view_t views[R3D_MAX_VIEWS];
    uint32_t view_count;
    ecs_entity_t cascades[R3D_CASCADES];
    float cascade_matrices[R3D_CASCADES][16];
    r3d_pass_data_t pass_data[R3D_MAX_OWNED];
    uint32_t pass_data_count;
};

typedef struct {
    r3d_surface_t **items;
    uint32_t count;
    uint32_t capacity;
} r3d_surfaces_t;

static r3d_surfaces_t surfaces;
static ecs_query_id_t cameras_query;
static ecs_query_id_t lights_query;

/* Settings of the first camera of a surface, defaults where absent. */
typedef struct {
    float sky[4];
    float ambient[4];
    float fog_color[4];
    float fog_start;
    float fog_end;
    bool shadows;
    float shadow_distance;
    bool bloom;
    float bloom_threshold;
    float bloom_intensity;
    uint32_t samples;
} r3d_settings_t;

static void linear_color(Color color, float out[4]) {
    out[0] = r3d_linear(color.r);
    out[1] = r3d_linear(color.g);
    out[2] = r3d_linear(color.b);
    out[3] = color.a / 255.0f;
}

static r3d_settings_t settings_of(ecs_entity_t camera) {
    r3d_settings_t s = { 0 };
    const Sky *sky = ecs_try_get(camera, Sky);
    linear_color(sky ? sky->color : (Color){ 13, 13, 20, 255 }, s.sky);
    const AmbientLight *ambient = ecs_try_get(camera, AmbientLight);
    linear_color(ambient ? ambient->color : (Color){ 255, 255, 255, 255 }, s.ambient);
    s.ambient[3] = ambient ? ambient->intensity : 0.2f;
    const Fog *fog = ecs_try_get(camera, Fog);
    linear_color(fog ? fog->color : (Color){ 13, 13, 20, 255 }, s.fog_color);
    s.fog_start = fog ? fog->start : 0.0f;
    s.fog_end = fog ? fog->end : 0.0f;
    const Shadows *shadows = ecs_try_get(camera, Shadows);
    s.shadows = shadows && shadows->enabled;
    s.shadow_distance = shadows ? shadows->distance : 35.0f;
    const BloomSettings *bloom = ecs_try_get(camera, BloomSettings);
    s.bloom = bloom ? bloom->enabled : true;
    s.bloom_threshold = bloom ? bloom->threshold : 0.0f;
    s.bloom_intensity = bloom ? bloom->intensity : 1.0f;
    const Multisampling *msaa = ecs_try_get(camera, Multisampling);
    s.samples = sigpu_max_samples(GpuFormatRg11b10f, msaa ? (uint32_t)msaa->samples : 4u);
    return s;
}

/* ----------------------------------------------------------------------
 * Pass callbacks
 * ---------------------------------------------------------------------- */

static void forward(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    const r3d_pass_data_t *data = user_data;
    const r3d_view_t *view = &data->surface->views[data->index];
    gpu_cmd_uniform(ctx, "view3d", &view->uniform, sizeof(view->uniform));
    if (data->index > 0) {
        /* Later cameras only clear their own viewport. */
        gpu_cmd_uniform(ctx, "sky", view->sky, sizeof(view->sky));
        if (gpu_cmd_program(ctx, r3d.sky_program, NULL)) {
            gpu_cmd_draw(ctx, 3, 1, 0, 0);
        }
    }
}

static void shadow(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    const r3d_pass_data_t *data = user_data;
    gpu_cmd_uniform(ctx, "shadow_view", data->surface->cascade_matrices[data->index],
                    sizeof(float) * 16);
}

static void fullscreen(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    const r3d_pass_data_t *data = user_data;
    float parameters[4];
    memcpy(parameters, data->parameters, sizeof(parameters));
    uint32_t width = 1, height = 1;
    gpu_pass_extent(ctx, &width, &height);
    if (data->texel == 1) {
        parameters[2] = 0.5f / (float)width;
        parameters[3] = 0.5f / (float)height;
    } else if (data->texel == 2) {
        parameters[0] = 1.0f / (float)width;
    } else if (data->texel == 3) {
        parameters[1] = 1.0f / (float)height;
    }
    if (data->uniform) {
        gpu_cmd_uniform(ctx, data->uniform, parameters, sizeof(parameters));
    }
    gpu_cmd_draw(ctx, 3, 1, 0, 0);
}

/* ----------------------------------------------------------------------
 * Graph generation
 * ---------------------------------------------------------------------- */

static ecs_entity_t own(r3d_surface_t *surface, ecs_entity_t entity) {
    if (surface->owned_count < R3D_MAX_OWNED) {
        surface->owned[surface->owned_count++] = entity;
    }
    return entity;
}

static void release_owned(r3d_surface_t *surface) {
    for (uint32_t i = surface->owned_count; i-- > 0;) {
        if (ecs_is_alive(surface->owned[i])) {
            ecs_kill(surface->owned[i]);
        }
    }
    surface->owned_count = 0;
    surface->pass_data_count = 0;
    memset(surface->cascades, 0, sizeof(surface->cascades));
}

static ecs_entity_t texture(r3d_surface_t *surface, uint8_t format, uint32_t usage,
                            uint8_t samples, float scale) {
    const ecs_entity_t entity = own(surface, ecs_new());
    ecs_relate(entity, ChildOf, surface->surface);
    ecs_set(entity, GpuTexture, { .format = format, .usage = usage, .samples = samples,
                                  .size_from = surface->surface, .scale = scale });
    return entity;
}

static r3d_pass_data_t *pass_data(r3d_surface_t *surface, uint32_t index) {
    r3d_pass_data_t *data = &surface->pass_data[surface->pass_data_count++];
    *data = (r3d_pass_data_t){ .surface = surface, .index = index };
    return data;
}

static ecs_entity_t pass(r3d_surface_t *surface, ecs_entity_t graph, const char *name,
                         int32_t order, ecs_entity_t program, ecs_entity_t phase,
                         ecs_entity_t view, gpu_pass_fn execute, r3d_pass_data_t *data) {
    char full[64];
    snprintf(full, sizeof(full), "sirender3d_%s", name);
    return own(surface, sigpu_pass(graph, &(GpuPassDesc){ .name = full, .kind = GpuPassRender,
                                                          .order = order, .program = program,
                                                          .phase = phase, .view = view,
                                                          .execute = execute,
                                                          .user_data = data }));
}

static ecs_entity_t post(r3d_surface_t *surface, ecs_entity_t graph, const char *name,
                         int32_t order, ecs_entity_t program, const char *uniform,
                         const float parameters[4], uint8_t texel) {
    r3d_pass_data_t *data = pass_data(surface, 0);
    data->uniform = uniform;
    if (parameters) {
        memcpy(data->parameters, parameters, sizeof(data->parameters));
    }
    data->texel = texel;
    return pass(surface, graph, name, order, program, 0, 0, fullscreen, data);
}

static void build(r3d_surface_t *surface, const r3d_settings_t *settings) {
    release_owned(surface);
    const GpuSurfaceTargets *targets = ecs_try_get(surface->surface, GpuSurfaceTargets);
    const ecs_entity_t graph = targets->graph;
    const ecs_entity_t color = targets->color;
    const uint32_t samples = settings->samples;
    const uint32_t target = GpuTextureColorTarget | GpuTextureSampled;
    const ecs_entity_t hdr = texture(surface, GpuFormatRg11b10f, target, 1, 0);
    const ecs_entity_t glow = texture(surface, GpuFormatRg11b10f, target, 1, 0);
    const ecs_entity_t hdr_ms =
        samples > 1 ? texture(surface, GpuFormatRg11b10f, GpuTextureColorTarget, samples, 0) : 0;
    const ecs_entity_t glow_ms =
        samples > 1 ? texture(surface, GpuFormatRg11b10f, GpuTextureColorTarget, samples, 0) : 0;
    const ecs_entity_t depth =
        texture(surface, GpuFormatD32f, GpuTextureDepthTarget, (uint8_t)samples, 0);
    const ecs_entity_t shadow_map = own(surface, ecs_new());
    ecs_relate(shadow_map, ChildOf, surface->surface);
    const uint32_t shadow_size = settings->shadows ? R3D_SHADOW_SIZE : 1;
    ecs_set(shadow_map, GpuTexture, { .dimension = GpuTexture2dArray, .format = GpuFormatD16,
                                      .usage = GpuTextureDepthTarget | GpuTextureSampled,
                                      .width = shadow_size, .height = shadow_size,
                                      .depth = R3D_CASCADES });

    if (settings->shadows) {
        for (uint32_t i = 0; i < R3D_CASCADES; i++) {
            surface->cascades[i] = own(surface, ecs_new());
            ecs_relate(surface->cascades[i], ChildOf, surface->surface);
            char name[32];
            snprintf(name, sizeof(name), "shadow%u", i);
            const ecs_entity_t cascade =
                pass(surface, graph, name, GPU_ORDER_SHADOW + (int32_t)i, 0, Render3dShadow,
                     surface->cascades[i], shadow, pass_data(surface, i));
            sigpu_pass_depth(cascade, shadow_map, GpuLoadClear, GpuStoreStore, 1.0f, (uint16_t)i);
        }
    }

    for (uint32_t v = 0; v < surface->view_count; v++) {
        r3d_view_t *view = &surface->views[v];
        char name[32];
        snprintf(name, sizeof(name), "forward%u", v);
        const ecs_entity_t forward_pass =
            pass(surface, graph, name, GPU_ORDER_SCENE + (int32_t)v, 0, Render3dOpaque,
                 view->camera, forward, pass_data(surface, v));
        const uint8_t load = v == 0 ? GpuLoadClear : GpuLoadLoad;
        const float black[4] = { 0, 0, 0, 1 };
        if (samples > 1) {
            sigpu_pass_color(forward_pass, hdr_ms, load, GpuStoreStore, view->sky, hdr);
            sigpu_pass_color(forward_pass, glow_ms, load, GpuStoreStore, black, glow);
        } else {
            sigpu_pass_color(forward_pass, hdr, load, GpuStoreStore, view->sky, 0);
            sigpu_pass_color(forward_pass, glow, load, GpuStoreStore, black, 0);
        }
        sigpu_pass_depth(forward_pass, depth, GpuLoadClear, GpuStoreDontCare, 1.0f, 0);
        sigpu_pass_sample(forward_pass, "shadow_map", shadow_map, r3d.shadow_sampler);
        GpuPass desc = *ecs_get(forward_pass, GpuPass);
        memcpy(desc.viewport, view->desc.viewport, sizeof(desc.viewport));
        ecs_set_cid(forward_pass, ecs_id(GpuPass), &desc);
    }

    if (settings->bloom) {
        const ecs_entity_t half = texture(surface, GpuFormatRg11b10f, target, 1, 0.5f);
        const ecs_entity_t half_b = texture(surface, GpuFormatRg11b10f, target, 1, 0.5f);
        const ecs_entity_t quarter = texture(surface, GpuFormatRg11b10f, target, 1, 0.25f);
        const ecs_entity_t quarter_b = texture(surface, GpuFormatRg11b10f, target, 1, 0.25f);
        const struct {
            const char *name;
            ecs_entity_t program, source, target;
            const char *uniform;
            float parameters[4];
            uint8_t texel;
        } steps[] = {
            { "bloom_down_half", r3d.bloom_down_program, glow, half_b, "bloom_down",
              { settings->bloom_threshold, 1, 0, 0 }, 1 },
            { "bloom_blur_half_h", r3d.bloom_blur_program, half_b, half, "bloom_blur", { 0 }, 2 },
            { "bloom_blur_half_v", r3d.bloom_blur_program, half, half_b, "bloom_blur", { 0 }, 3 },
            { "bloom_down_quarter", r3d.bloom_down_program, half_b, quarter_b, "bloom_down",
              { settings->bloom_threshold, 0, 0, 0 }, 1 },
            { "bloom_blur_quarter_h", r3d.bloom_blur_program, quarter_b, quarter, "bloom_blur",
              { 0 }, 2 },
            { "bloom_blur_quarter_v", r3d.bloom_blur_program, quarter, quarter_b, "bloom_blur",
              { 0 }, 3 },
        };
        for (uint32_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
            const ecs_entity_t step =
                post(surface, graph, steps[i].name, GPU_ORDER_POST + (int32_t)i, steps[i].program,
                     steps[i].uniform, steps[i].parameters, steps[i].texel);
            sigpu_pass_sample(step, "source", steps[i].source, r3d.linear_sampler);
            sigpu_pass_color(step, steps[i].target, GpuLoadDontCare, GpuStoreStore, NULL, 0);
        }
        const float intensity[4] = { settings->bloom_intensity, 0, 0, 0 };
        const ecs_entity_t composite = post(surface, graph, "composite", GPU_ORDER_POST + 20,
                                            r3d.composite_program, "composite", intensity, 0);
        sigpu_pass_sample(composite, "scene", hdr, r3d.linear_sampler);
        sigpu_pass_sample(composite, "bloom_half", half_b, r3d.linear_sampler);
        sigpu_pass_sample(composite, "bloom_quarter", quarter_b, r3d.linear_sampler);
        sigpu_pass_color(composite, color, GpuLoadDontCare, GpuStoreStore, NULL, 0);
    } else {
        const ecs_entity_t composite = post(surface, graph, "composite", GPU_ORDER_POST + 20,
                                            r3d.composite_plain_program, NULL, NULL, 0);
        sigpu_pass_sample(composite, "scene", hdr, r3d.linear_sampler);
        sigpu_pass_color(composite, color, GpuLoadDontCare, GpuStoreStore, NULL, 0);
    }
}

/* ----------------------------------------------------------------------
 * Views
 * ---------------------------------------------------------------------- */

static r3d_surface_t *surface_get(ecs_entity_t entity) {
    for (uint32_t i = 0; i < surfaces.count; i++) {
        if (surfaces.items[i]->surface == entity) {
            return surfaces.items[i];
        }
    }
    if (surfaces.count == surfaces.capacity) {
        const uint32_t capacity = surfaces.capacity ? surfaces.capacity * 2 : 4;
        r3d_surface_t **grown = realloc(surfaces.items, capacity * sizeof(*grown));
        if (!grown) {
            return NULL;
        }
        surfaces.items = grown;
        surfaces.capacity = capacity;
    }
    r3d_surface_t *surface = calloc(1, sizeof(r3d_surface_t));
    if (!surface) {
        return NULL;
    }
    surface->surface = entity;
    surfaces.items[surfaces.count++] = surface;
    return surface;
}

static int view_compare(const void *a, const void *b) {
    const r3d_view_t *left = a, *right = b;
    if (left->desc.order != right->desc.order) {
        return left->desc.order < right->desc.order ? -1 : 1;
    }
    return left->camera < right->camera ? -1 : left->camera > right->camera;
}

static uint64_t mix(uint64_t hash, uint64_t value) {
    return hash ^ (value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
}

static uint64_t signature_of(const r3d_surface_t *surface, const r3d_settings_t *settings) {
    uint64_t hash = mix(0, surface->view_count);
    for (uint32_t v = 0; v < surface->view_count; v++) {
        const r3d_view_t *view = &surface->views[v];
        hash = mix(hash, view->camera);
        const uint32_t *viewport = (const uint32_t *)view->desc.viewport;
        for (uint32_t i = 0; i < 4; i++) {
            hash = mix(hash, viewport[i]);
        }
        /* The first camera clears to its sky color. */
        if (v == 0) {
            for (uint32_t i = 0; i < 4; i++) {
                uint32_t bits;
                memcpy(&bits, &view->sky[i], sizeof(bits));
                hash = mix(hash, bits);
            }
        }
    }
    uint32_t bits[3];
    memcpy(&bits[0], &settings->bloom_threshold, 4);
    memcpy(&bits[1], &settings->bloom_intensity, 4);
    memcpy(&bits[2], &settings->shadow_distance, 4);
    hash = mix(hash, settings->shadows);
    hash = mix(hash, settings->bloom);
    hash = mix(hash, settings->samples);
    for (uint32_t i = 0; i < 3; i++) {
        hash = mix(hash, bits[i]);
    }
    return hash;
}

static void sun_of(r3d_vec3_t *direction, float color[4], float *intensity) {
    *direction = r3d_normalize((r3d_vec3_t){ -1.0f, -2.0f, 1.0f });
    linear_color((Color){ 255, 245, 220, 255 }, color);
    *intensity = 1.0f;
    for (ecs_iter_t it = ecs_query_iter(lights_query); ecs_iter_next(&it);) {
        const DirectionalLight *light = ecs_field(&it, 0);
        if (it.count) {
            *direction = r3d_normalize((r3d_vec3_t){ light[0].x, light[0].y, light[0].z });
            linear_color(light[0].color, color);
            *intensity = light[0].intensity;
        }
    }
}

static void set_view(ecs_entity_t entity, const GpuView *view) {
    const GpuView *current = ecs_try_get(entity, GpuView);
    if (!current || memcmp(current, view, sizeof(*view)) != 0) {
        ecs_set_cid(entity, ecs_id(GpuView), view);
    }
}

static void camera_corners(const r3d_camera_t *camera, float near_distance, float far_distance,
                           r3d_vec3_t corners[8]) {
    const float tangent = tanf(camera->fov * R3D_PI / 360.0f);
    const float sizes[2][2] = { { tangent * near_distance * camera->aspect,
                                  tangent * near_distance },
                                { tangent * far_distance * camera->aspect,
                                  tangent * far_distance } };
    const float distances[2] = { near_distance, far_distance };
    for (int i = 0; i < 8; i++) {
        const int far = i >= 4;
        const r3d_vec3_t center =
            r3d_add(camera->position, r3d_scale(camera->forward, distances[far]));
        const float x = i % 4 == 1 || i % 4 == 2 ? sizes[far][0] : -sizes[far][0];
        const float y = i % 4 >= 2 ? sizes[far][1] : -sizes[far][1];
        corners[i] = r3d_add(r3d_add(center, r3d_scale(camera->right, x)),
                             r3d_scale(camera->up, y));
    }
}

/* Cascades of the first camera along the sun (as in sigpu v1). */
static void cascades(r3d_surface_t *surface, const r3d_settings_t *settings, r3d_vec3_t sun,
                     r3d_view_uniform_t *uniform) {
    const r3d_camera_t *camera = &surface->views[0].view;
    const float distance = fminf(camera->far_plane, settings->shadow_distance);
    const float ends[R3D_CASCADES] = { fminf(distance, 120.0f), fminf(distance, 300.0f), distance };
    uint32_t count = distance > camera->near_plane ? 1 : 0;
    count += distance > 120.0f;
    count += distance > 300.0f;
    const r3d_vec3_t up = fabsf(sun.y) > 0.99f ? (r3d_vec3_t){ 1, 0, 0 } : (r3d_vec3_t){ 0, 1, 0 };
    const r3d_mat4_t orientation = r3d_look_at((r3d_vec3_t){ 0, 0, 0 }, sun, up);
    const float tangent = tanf(camera->fov * R3D_PI / 360.0f);
    const float range = fmaxf(distance * 3.0f, 200.0f);
    uniform->shadow_splits[3] = (float)count;
    for (uint32_t i = 0; i < R3D_CASCADES; i++) {
        GpuView view = { .orthographic = true, .cache = true };
        if (i >= count) {
            memcpy(uniform->light_view_projection[i], r3d_identity().m, sizeof(float) * 16);
            /* An empty cascade: a view no object is inside. */
            r3d_mat4_t far_away = r3d_identity();
            far_away.m[12] = 1e9f;
            memcpy(view.view_projection, far_away.m, sizeof(view.view_projection));
            memcpy(surface->cascade_matrices[i], far_away.m, sizeof(float) * 16);
            set_view(surface->cascades[i], &view);
            continue;
        }
        const float near = i ? ends[i - 1] : camera->near_plane;
        const float far = ends[i];
        const float overlap = i ? fminf(10.0f, (far - near) * 0.1f) : 0.0f;
        const float start = fmaxf(camera->near_plane, near - overlap);
        r3d_vec3_t corners[8];
        camera_corners(camera, start, far, corners);
        r3d_vec3_t center = { 0, 0, 0 };
        for (int c = 0; c < 8; c++) {
            center = r3d_add(center, r3d_scale(corners[c], 0.125f));
        }
        const float lateral = tangent * far * sqrtf(1.0f + camera->aspect * camera->aspect);
        const float longitudinal = (far - start) * 0.5f;
        float half = sqrtf(lateral * lateral + longitudinal * longitudinal);
        half /= 1.0f - 8.0f / (float)R3D_SHADOW_SIZE;
        const float texel = 2.0f * half / (float)R3D_SHADOW_SIZE;
        /* Snapping to texels keeps shadows still while the camera moves. */
        const r3d_vec3_t light_center = r3d_transform_point(&orientation, center);
        const float dx = roundf(light_center.x / texel) * texel - light_center.x;
        const float dy = roundf(light_center.y / texel) * texel - light_center.y;
        const r3d_vec3_t right = { orientation.m[0], orientation.m[4], orientation.m[8] };
        const r3d_vec3_t light_up = { orientation.m[1], orientation.m[5], orientation.m[9] };
        center = r3d_add(center, r3d_add(r3d_scale(right, dx), r3d_scale(light_up, dy)));
        const r3d_vec3_t eye = r3d_sub(center, r3d_scale(sun, range));
        const r3d_mat4_t look = r3d_look_at(eye, r3d_add(eye, sun), up);
        const r3d_mat4_t projection = r3d_orthographic(-half, half, -half, half, 0.1f, 2.0f * range);
        const r3d_mat4_t matrix = r3d_mul(projection, look);
        memcpy(uniform->light_view_projection[i], matrix.m, sizeof(float) * 16);
        memcpy(surface->cascade_matrices[i], matrix.m, sizeof(float) * 16);
        uniform->shadow_splits[i] = far;
        uniform->shadow_texel[i][0] = texel;
        uniform->shadow_texel[i][1] = 2.0f * range;
        memcpy(view.view_projection, matrix.m, sizeof(view.view_projection));
        view.position[0] = eye.x;
        view.position[1] = eye.y;
        view.position[2] = eye.z;
        view.forward[0] = sun.x;
        view.forward[1] = sun.y;
        view.forward[2] = sun.z;
        view.lod_scale = (float)R3D_SHADOW_SIZE / (2.0f * half);
        set_view(surface->cascades[i], &view);
    }
}

static void update_view(r3d_view_t *view, ecs_entity_t surface_entity) {
    uint32_t width = 1, height = 1;
    sigpu_extent(surface_entity, &width, &height);
    const float *viewport = view->desc.viewport;
    const float vw = viewport[2] > 0.0f ? viewport[2] : 1.0f;
    const float vh = viewport[3] > 0.0f ? viewport[3] : 1.0f;
    r3d_camera_t *camera = &view->view;
    camera->aspect = ((float)width * vw) / fmaxf((float)height * vh, 1.0f);
    camera->near_plane = view->desc.near_plane > 0.0f ? view->desc.near_plane : 0.1f;
    camera->far_plane =
        view->desc.far_plane > camera->near_plane ? view->desc.far_plane : camera->near_plane + 1000;
    camera->fov = fminf(fmaxf(view->desc.fov > 0.0f ? view->desc.fov : 60.0f, 1.0f), 179.0f);
    camera->orthographic = view->desc.projection == Render3dOrthographic;
    memcpy(camera->viewport, view->desc.viewport, sizeof(camera->viewport));
    const r3d_vec3_t world_up = { 0, 1, 0 };
    camera->right = r3d_normalize(r3d_cross(world_up, camera->forward));
    camera->up = r3d_cross(camera->forward, camera->right);
    const r3d_mat4_t look =
        r3d_look_at(camera->position, r3d_add(camera->position, camera->forward), world_up);
    const float ortho_height = view->desc.fov > 0.0f ? view->desc.fov : 10.0f;
    const r3d_mat4_t projection =
        camera->orthographic
            ? r3d_orthographic(-ortho_height * camera->aspect * 0.5f,
                               ortho_height * camera->aspect * 0.5f, -ortho_height * 0.5f,
                               ortho_height * 0.5f, camera->near_plane, camera->far_plane)
            : r3d_perspective(camera->fov, camera->aspect, camera->near_plane, camera->far_plane);
    const r3d_mat4_t matrix = r3d_mul(projection, look);
    memcpy(view->uniform.view_projection, matrix.m, sizeof(matrix.m));
    GpuView gpu = { .orthographic = camera->orthographic };
    memcpy(gpu.view_projection, matrix.m, sizeof(matrix.m));
    gpu.position[0] = camera->position.x;
    gpu.position[1] = camera->position.y;
    gpu.position[2] = camera->position.z;
    gpu.forward[0] = camera->forward.x;
    gpu.forward[1] = camera->forward.y;
    gpu.forward[2] = camera->forward.z;
    const float pixels = (float)height * vh;
    gpu.lod_scale = camera->orthographic
                        ? pixels / ortho_height
                        : pixels / (2.0f * tanf(camera->fov * R3D_PI / 360.0f));
    set_view(view->camera, &gpu);
}

static void fill_uniform(r3d_view_t *view, const r3d_settings_t *settings, r3d_vec3_t sun,
                         const float sun_color[4], float sun_intensity) {
    r3d_view_uniform_t *u = &view->uniform;
    const r3d_camera_t *camera = &view->view;
    u->camera_position[0] = camera->position.x;
    u->camera_position[1] = camera->position.y;
    u->camera_position[2] = camera->position.z;
    u->camera_position[3] = 1.0f;
    u->camera_direction[0] = camera->forward.x;
    u->camera_direction[1] = camera->forward.y;
    u->camera_direction[2] = camera->forward.z;
    u->sun_direction_intensity[0] = sun.x;
    u->sun_direction_intensity[1] = sun.y;
    u->sun_direction_intensity[2] = sun.z;
    u->sun_direction_intensity[3] = sun_intensity;
    memcpy(u->sun_color, sun_color, sizeof(u->sun_color));
    u->sun_color[3] = 1.0f;
    memcpy(u->ambient_color_intensity, settings->ambient, sizeof(u->ambient_color_intensity));
    memcpy(u->fog_color, settings->fog_color, sizeof(u->fog_color));
    u->fog_parameters[0] = settings->fog_start;
    u->fog_parameters[1] = settings->fog_end;
    u->fog_parameters[2] = settings->fog_end > settings->fog_start ? 1.0f : 0.0f;
    u->fog_parameters[3] = settings->shadows ? 1.0f : 0.0f;
    u->shadow_parameters[0] = settings->shadow_distance;
    u->shadow_parameters[1] = 1.0f / (float)R3D_SHADOW_SIZE;
}

static void surface_free(r3d_surface_t *surface) {
    release_owned(surface);
    free(surface);
}

static void views_update(ecs_iter_t *it) {
    (void)it;
    for (uint32_t i = 0; i < surfaces.count; i++) {
        surfaces.items[i]->seen = false;
        surfaces.items[i]->view_count = 0;
    }
    for (ecs_iter_t cameras = ecs_query_iter(cameras_query); ecs_iter_next(&cameras);) {
        const Camera3d *descs = ecs_field(&cameras, 0);
        const GlobalPosition3d *positions = ecs_field(&cameras, 1);
        const GlobalOrientation3d *orientations = ecs_field(&cameras, 2);
        const ecs_relation_target_t *targets = ecs_targets(&cameras, RendersTo);
        for (uint32_t i = 0; targets && i < cameras.count; i++) {
            const ecs_entity_t target = targets[i].entity;
            if (!target || !ecs_is_alive(target) || !ecs_has(target, GpuSurface)) {
                continue;
            }
            r3d_surface_t *surface = surface_get(target);
            if (!surface || surface->view_count == R3D_MAX_VIEWS) {
                continue;
            }
            surface->seen = true;
            r3d_view_t *view = &surface->views[surface->view_count++];
            view->camera = cameras.entities[i];
            view->desc = descs[ecs_field_is_shared(&cameras, 0) ? 0 : i];
            const GlobalPosition3d p = positions[ecs_field_is_shared(&cameras, 1) ? 0 : i];
            const GlobalOrientation3d q = orientations[ecs_field_is_shared(&cameras, 2) ? 0 : i];
            view->view.camera = view->camera;
            view->view.surface = target;
            view->view.position = (r3d_vec3_t){ p.x, p.y, p.z };
            const Direction3d forward = sispatial_forward_3d(&q);
            view->view.forward = r3d_normalize((r3d_vec3_t){ forward.x, forward.y, forward.z });
        }
    }
    r3d_vec3_t sun;
    float sun_color[4], sun_intensity;
    sun_of(&sun, sun_color, &sun_intensity);
    for (uint32_t s = 0; s < surfaces.count;) {
        r3d_surface_t *surface = surfaces.items[s];
        if (!surface->seen) {
            surface_free(surface);
            surfaces.items[s] = surfaces.items[--surfaces.count];
            continue;
        }
        s++;
        qsort(surface->views, surface->view_count, sizeof(r3d_view_t), view_compare);
        const r3d_settings_t settings = settings_of(surface->views[0].camera);
        for (uint32_t v = 0; v < surface->view_count; v++) {
            const r3d_settings_t own = settings_of(surface->views[v].camera);
            memcpy(surface->views[v].sky, own.sky, sizeof(own.sky));
        }
        const uint64_t signature = signature_of(surface, &settings);
        if (signature != surface->signature &&
            ecs_try_get(surface->surface, GpuSurfaceTargets)) {
            surface->signature = signature;
            build(surface, &settings);
        }
        for (uint32_t v = 0; v < surface->view_count; v++) {
            r3d_view_t *view = &surface->views[v];
            update_view(view, surface->surface);
            fill_uniform(view, &settings, sun, sun_color, sun_intensity);
        }
        if (settings.shadows && surface->cascades[0]) {
            cascades(surface, &settings, sun, &surface->views[0].uniform);
            for (uint32_t v = 1; v < surface->view_count; v++) {
                memcpy(surface->views[v].uniform.light_view_projection,
                       surface->views[0].uniform.light_view_projection,
                       sizeof(surface->views[0].uniform.light_view_projection));
                memcpy(surface->views[v].uniform.shadow_splits,
                       surface->views[0].uniform.shadow_splits,
                       sizeof(surface->views[0].uniform.shadow_splits));
                memcpy(surface->views[v].uniform.shadow_texel,
                       surface->views[0].uniform.shadow_texel,
                       sizeof(surface->views[0].uniform.shadow_texel));
            }
        }
    }
}

bool r3d_camera_for_window(ecs_entity_t window, r3d_camera_t *out) {
    const Window *desc = window && ecs_is_alive(window) ? ecs_try_get(window, Window) : NULL;
    if (!desc) {
        return false;
    }
    for (uint32_t s = 0; s < surfaces.count; s++) {
        const r3d_surface_t *surface = surfaces.items[s];
        const GpuSurface *gpu = ecs_try_get(surface->surface, GpuSurface);
        if (gpu && surface->view_count && strcmp(gpu->window_slot, desc->slot) == 0) {
            *out = surface->views[0].view;
            return true;
        }
    }
    return false;
}

void r3d_views_init(void) {
    cameras_query = ecs_query({
        .components = { ecs_in(Camera3d), ecs_in(GlobalPosition3d), ecs_in(GlobalOrientation3d) },
        .relations = { ecs_rel(RendersTo) },
    });
    lights_query = ecs_query({ .components = { ecs_in(DirectionalLight) } });
    ecs_system({ .name = "Render3dViews", .phase = EcsPreRender, .callback = views_update,
                 .main_thread_only = true, .no_defer = true });
}

void r3d_views_fini(void) {
    for (uint32_t i = 0; i < surfaces.count; i++) {
        free(surfaces.items[i]);
    }
    free(surfaces.items);
    surfaces = (r3d_surfaces_t){ 0 };
}
