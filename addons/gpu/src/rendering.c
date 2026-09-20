#include "rendering.h"
#include "input/input.h"
#include "interaction/interaction.h"
#include "sigpu.h"
#include "sigpu_internal.h"
#include <siecs_spatial.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const int default_multisampling = 4;
// Static renderables are immutable after their first PreRender snapshot.
static const float static_chunk_size = 64.0f;
_Static_assert(sizeof(sigpu_shared_axis_instance_t) == 24, "shared axis instance layout");
_Static_assert(sizeof(sigpu_shared_rotated_instance_t) == 32, "shared rotated instance layout");

typedef enum { Axis, Rotated } static_instance_kind;

typedef union {
    sigpu_axis_instance_t axis;
    sigpu_rotated_instance_t rotated;
} static_instance;

typedef struct {
    int32_t cell_x;
    int32_t cell_y;
    int32_t cell_z;
    float x;
    float y;
    float z;
    float radius;
    static_instance_kind kind;
    static_instance instance;
} static_item;

static static_item *static_items;
static size_t static_items_count, static_items_capacity;
static bool static_cache_ready;
static ecs_system_id_t static_collect_system;
static ecs_system_id_t static_finish_system;

static sigpu_color_t to_sigpu(Color color) {
    return (sigpu_color_t){ color.r, color.g, color.b, color.a };
}

#define DECLARE_FIELD(T)                                                                           \
    typedef struct {                                                                               \
        const T *data;                                                                             \
        ptrdiff_t stride;                                                                          \
    } field_##T
DECLARE_FIELD(GlobalPosition3d);
DECLARE_FIELD(GlobalOrientation3d);
DECLARE_FIELD(GlobalScale3d);
DECLARE_FIELD(Cuboid);
DECLARE_FIELD(Color);
DECLARE_FIELD(Bloom);
#define FIELD(T, it, index)                                                                        \
    ((field_##T){ ecs_field(it, index), ecs_field_is_shared(it, index) ? 0 : 1 })
#define AT(f, index) ((f).data[(index) * (f).stride])

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t w;
} packed_rotation;

static packed_rotation pack_rotation(GlobalOrientation3d orientation) {
    return (packed_rotation){
        (int16_t)(roundf(orientation.x * 32767.0f)),
        (int16_t)(roundf(orientation.y * 32767.0f)),
        (int16_t)(roundf(orientation.z * 32767.0f)),
        (int16_t)(roundf(orientation.w * 32767.0f)),
    };
}

static sigpu_shared_material_t make_shared_material(Cuboid cuboid, Color color, float bloom) {
    return (sigpu_shared_material_t){
        .size_bloom = { cuboid.width, cuboid.height, cuboid.depth, bloom },
        .color = {
            g_sigpu.linear_lut[color.r] / 255.0f,
            g_sigpu.linear_lut[color.g] / 255.0f,
            g_sigpu.linear_lut[color.b] / 255.0f,
            color.a / 255.0f,
        },
    };
}

static sigpu_axis_instance_t make_owned_axis(
    GlobalPosition3d position,
    float width,
    float height,
    float depth,
    Color color,
    float bloom
) {
    return (sigpu_axis_instance_t){
        position.x,
        position.y,
        position.z,
        width,
        height,
        depth,
        g_sigpu.linear_lut[color.r],
        g_sigpu.linear_lut[color.g],
        g_sigpu.linear_lut[color.b],
        color.a,
        bloom,
    };
}

static sigpu_rotated_instance_t make_owned_rotated(
    GlobalPosition3d position,
    GlobalOrientation3d rotation,
    float width,
    float height,
    float depth,
    Color color,
    float bloom
) {
    const packed_rotation packed = pack_rotation(rotation);
    return (sigpu_rotated_instance_t){
        position.x,
        position.y,
        position.z,
        width,
        height,
        depth,
        packed.x,
        packed.y,
        packed.z,
        packed.w,
        g_sigpu.linear_lut[color.r],
        g_sigpu.linear_lut[color.g],
        g_sigpu.linear_lut[color.b],
        color.a,
        bloom,
    };
}

static bool
visible(GlobalPosition3d position, float width, float height, float depth, float aspect) {
    const float radius = 0.5f * sqrtf(width * width + height * height + depth * depth);
    const sigpu_vec3_t center = { position.x, position.y, position.z };
    return sigpu_camera_visible(center, radius, aspect) ||
           (g_sigpu.shadows_enabled && sigpu_shadow_visible(center, radius));
}

static void record_batch(
    sigpu_shared_batch_t **batches,
    Uint32 *count,
    Uint32 *capacity,
    Uint32 first,
    Uint32 end,
    sigpu_shared_material_t material
) {
    if (first == end)
        return;
    if (*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 16;
        *batches = SDL_realloc(*batches, *capacity * sizeof(sigpu_shared_batch_t));
    }
    (*batches)[(*count)++] = (sigpu_shared_batch_t){ material, first, end - first };
}

static void render_shared_cuboids(
    ecs_iter_t *it,
    field_GlobalPosition3d positions,
    field_GlobalOrientation3d rotations,
    field_GlobalScale3d scales,
    field_Cuboid cuboids,
    field_Color colors,
    field_Bloom blooms,
    float aspect
) {
    const Cuboid cuboid = AT(cuboids, 0);
    const Color color = AT(colors, 0);
    const float bloom = blooms.data ? fmaxf(AT(blooms, 0).intensity, 0.0f) : 0.0f;
    const sigpu_shared_material_t material = make_shared_material(cuboid, color, bloom);
    const Uint32 first_axis = g_sigpu.shared_axis_count;
    const Uint32 first_rotated = g_sigpu.shared_rotated_count;

    for (uint32_t index = 0; index < it->count; ++index) {
        const GlobalPosition3d position = AT(positions, index);
        const GlobalOrientation3d rotation = AT(rotations, index);
        const GlobalScale3d scale = AT(scales, index);
        const float width = cuboid.width * scale.x;
        const float height = cuboid.height * scale.y;
        const float depth = cuboid.depth * scale.z;
        if (!visible(position, width, height, depth, aspect)) {
            continue;
        }

        if (rotation.x == 0.0f && rotation.y == 0.0f && rotation.z == 0.0f && rotation.w == 1.0f) {
            if (g_sigpu.shared_axis_count + g_sigpu.owned_axis_count == g_sigpu.axis_capacity) {
                sigpu_axis_instances_grow();
            }
            sigpu_shared_axis_instance_t *instances =
                (sigpu_shared_axis_instance_t *)(g_sigpu.axis_mapped);
            sigpu_shared_axis_instance_t *instance = &instances[g_sigpu.shared_axis_count++];
            *instance = (sigpu_shared_axis_instance_t){ position.x, position.y, position.z,
                                                        scale.x,    scale.y,    scale.z };
            continue;
        }

        if (g_sigpu.shared_rotated_count + g_sigpu.owned_rotated_count ==
            g_sigpu.rotated_capacity) {
            sigpu_rotated_instances_grow();
        }
        sigpu_shared_rotated_instance_t *instances =
            (sigpu_shared_rotated_instance_t *)(g_sigpu.rotated_mapped);
        sigpu_shared_rotated_instance_t *instance = &instances[g_sigpu.shared_rotated_count++];
        const packed_rotation packed = pack_rotation(rotation);
        *instance = (sigpu_shared_rotated_instance_t){
            position.x, position.y, position.z, scale.x,  scale.y,
            scale.z,    packed.x,   packed.y,   packed.z, packed.w,
        };
    }

    g_sigpu.any_bloom =
        g_sigpu.any_bloom || (bloom > 0.0f && (first_axis != g_sigpu.shared_axis_count ||
                                               first_rotated != g_sigpu.shared_rotated_count));

    record_batch(
        &g_sigpu.shared_axis_batches,
        &g_sigpu.shared_axis_batch_count,
        &g_sigpu.shared_axis_batch_capacity,
        first_axis,
        g_sigpu.shared_axis_count,
        material
    );
    record_batch(
        &g_sigpu.shared_rotated_batches,
        &g_sigpu.shared_rotated_batch_count,
        &g_sigpu.shared_rotated_batch_capacity,
        first_rotated,
        g_sigpu.shared_rotated_count,
        material
    );
}

static void render_owned_cuboids(
    ecs_iter_t *it,
    field_GlobalPosition3d positions,
    field_GlobalOrientation3d rotations,
    field_GlobalScale3d scales,
    field_Cuboid cuboids,
    field_Color colors,
    field_Bloom blooms,
    float aspect
) {
    for (uint32_t index = 0; index < it->count; ++index) {
        const GlobalPosition3d position = AT(positions, index);
        const GlobalOrientation3d rotation = AT(rotations, index);
        const float width = AT(cuboids, index).width * AT(scales, index).x;
        const float height = AT(cuboids, index).height * AT(scales, index).y;
        const float depth = AT(cuboids, index).depth * AT(scales, index).z;
        if (!visible(position, width, height, depth, aspect)) {
            continue;
        }

        const float bloom = blooms.data ? fmaxf(AT(blooms, index).intensity, 0.0f) : 0.0f;
        const Color color = AT(colors, index);
        g_sigpu.any_bloom = g_sigpu.any_bloom || bloom > 0.0f;

        if (rotation.x == 0.0f && rotation.y == 0.0f && rotation.z == 0.0f && rotation.w == 1.0f) {
            if (g_sigpu.shared_axis_count + g_sigpu.owned_axis_count == g_sigpu.axis_capacity) {
                sigpu_axis_instances_grow();
            }
            sigpu_axis_instance_t *instances = (sigpu_axis_instance_t *)(g_sigpu.axis_mapped);
            sigpu_axis_instance_t *instance =
                &instances[g_sigpu.axis_capacity - ++g_sigpu.owned_axis_count];
            *instance = make_owned_axis(position, width, height, depth, color, bloom);
            continue;
        }

        if (g_sigpu.shared_rotated_count + g_sigpu.owned_rotated_count ==
            g_sigpu.rotated_capacity) {
            sigpu_rotated_instances_grow();
        }
        sigpu_rotated_instance_t *instances = (sigpu_rotated_instance_t *)(g_sigpu.rotated_mapped);
        sigpu_rotated_instance_t *instance =
            &instances[g_sigpu.rotated_capacity - ++g_sigpu.owned_rotated_count];
        *instance = make_owned_rotated(position, rotation, width, height, depth, color, bloom);
    }
}

static int static_item_less(const void *a, const void *b) {
    const static_item *left = a, *right = b;
    if (left->cell_x != right->cell_x)
        return left->cell_x < right->cell_x ? -1 : 1;
    if (left->cell_y != right->cell_y)
        return left->cell_y < right->cell_y ? -1 : 1;
    if (left->cell_z != right->cell_z)
        return left->cell_z < right->cell_z ? -1 : 1;
    return (int)left->kind - (int)right->kind;
}

static void collect_static_cuboids(ecs_iter_t *it) {
    if (static_cache_ready) {
        return;
    }

    const field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    const field_GlobalOrientation3d rotations = FIELD(GlobalOrientation3d, it, 1);
    const field_GlobalScale3d scales = FIELD(GlobalScale3d, it, 2);
    const field_Cuboid cuboids = FIELD(Cuboid, it, 3);
    const field_Color colors = FIELD(Color, it, 4);
    const field_Bloom blooms = FIELD(Bloom, it, 5);

    for (uint32_t index = 0; index < it->count; index++) {
        const GlobalPosition3d position = AT(positions, index);
        const GlobalOrientation3d rotation = AT(rotations, index);
        const GlobalScale3d scale = AT(scales, index);
        const Cuboid cuboid = AT(cuboids, index);
        const Color color = AT(colors, index);
        const float width = cuboid.width * scale.x;
        const float height = cuboid.height * scale.y;
        const float depth = cuboid.depth * scale.z;
        const float bloom = blooms.data ? fmaxf(AT(blooms, index).intensity, 0.0f) : 0.0f;
        const bool rotated =
            rotation.x != 0.0f || rotation.y != 0.0f || rotation.z != 0.0f || rotation.w != 1.0f;
        static_item item = {
            .cell_x = (int32_t)(floorf(position.x / static_chunk_size)),
            .cell_y = (int32_t)(floorf(position.y / static_chunk_size)),
            .cell_z = (int32_t)(floorf(position.z / static_chunk_size)),
            .x = position.x,
            .y = position.y,
            .z = position.z,
            .radius = 0.5f * sqrtf(width * width + height * height + depth * depth),
        };

        if (rotated) {
            item.kind = Rotated;
            item.instance.rotated =
                make_owned_rotated(position, rotation, width, height, depth, color, bloom);
        } else {
            item.kind = Axis;
            item.instance.axis = make_owned_axis(position, width, height, depth, color, bloom);
        }
        if (static_items_count == static_items_capacity) {
            static_items_capacity = static_items_capacity ? static_items_capacity * 2 : 256;
            static_items = SDL_realloc(static_items, static_items_capacity * sizeof(*static_items));
        }
        static_items[static_items_count++] = item;
    }
}

static void finish_static_cache(ecs_iter_t *it) {
    if (static_cache_ready) {
        return;
    }

    if (static_items_count)
        qsort(static_items, static_items_count, sizeof(*static_items), static_item_less);
    sigpu_axis_instance_t *axis = SDL_malloc(static_items_count * sizeof(*axis));
    size_t axis_count = 0;
    sigpu_rotated_instance_t *rotated = SDL_malloc(static_items_count * sizeof(*rotated));
    size_t rotated_count = 0;
    sigpu_static_chunk_t *chunks = SDL_malloc(static_items_count * sizeof(*chunks));
    size_t chunks_count = 0;

    size_t first = 0;
    while (first < static_items_count) {
        size_t end = first + 1;
        while (end < static_items_count && static_items[end].cell_x == static_items[first].cell_x &&
               static_items[end].cell_y == static_items[first].cell_y &&
               static_items[end].cell_z == static_items[first].cell_z) {
            end++;
        }

        sigpu_static_chunk_t chunk = {
            .axis_first = (Uint32)(axis_count),
            .rotated_first = (Uint32)(rotated_count),
        };
        float min_x = INFINITY;
        float min_y = min_x;
        float min_z = min_x;
        float max_x = -min_x;
        float max_y = -min_x;
        float max_z = -min_x;

        for (size_t index = first; index < end; index++) {
            const static_item item = static_items[index];
            min_x = fminf(min_x, item.x - item.radius);
            min_y = fminf(min_y, item.y - item.radius);
            min_z = fminf(min_z, item.z - item.radius);
            max_x = fmaxf(max_x, item.x + item.radius);
            max_y = fmaxf(max_y, item.y + item.radius);
            max_z = fmaxf(max_z, item.z + item.radius);

            switch (item.kind) {
            case Axis:
                axis[axis_count++] = item.instance.axis;
                chunk.bloom = chunk.bloom || item.instance.axis.bloom > 0.0f;
                break;
            case Rotated:
                rotated[rotated_count++] = item.instance.rotated;
                chunk.bloom = chunk.bloom || item.instance.rotated.bloom > 0.0f;
                break;
            }
        }

        chunk.axis_count = (Uint32)(axis_count)-chunk.axis_first;
        chunk.rotated_count = (Uint32)(rotated_count)-chunk.rotated_first;
        chunk.center = (sigpu_vec3_t){ (min_x + max_x) * 0.5f,
                                       (min_y + max_y) * 0.5f,
                                       (min_z + max_z) * 0.5f };
        const float half_x = (max_x - min_x) * 0.5f;
        const float half_y = (max_y - min_y) * 0.5f;
        const float half_z = (max_z - min_z) * 0.5f;
        chunk.radius = sqrtf(half_x * half_x + half_y * half_y + half_z * half_z);
        chunks[chunks_count++] = chunk;
        first = end;
    }

    sigpu_static_upload_t upload = {
        .axis = axis,
        .axis_count = (Uint32)(axis_count),
        .rotated = rotated,
        .rotated_count = (Uint32)(rotated_count),
        .chunks = chunks,
        .chunk_count = (Uint32)(chunks_count),
    };
    sigpu_static_upload(&upload);
    SDL_free(axis);
    SDL_free(rotated);
    SDL_free(chunks);
    SDL_free(static_items);
    static_items = NULL;
    static_items_count = static_items_capacity = 0;
    static_cache_ready = true;
    ecs_system_disable(static_collect_system);
    ecs_system_disable(static_finish_system);
}

static ecs_system_id_t register_static_cache(ecs_system_id_t camera_system) {
    ecs_system_desc_t collect = {
        .name = "CollectStaticCuboids",
        .query = {
            .components = {
                { .id = ecs_id(GlobalPosition3d), .access = EcsIn },
                { .id = ecs_id(GlobalOrientation3d), .access = EcsIn },
                { .id = ecs_id(GlobalScale3d), .access = EcsIn },
                { .id = ecs_id(Cuboid), .access = EcsIn },
                { .id = ecs_id(Color), .access = EcsIn },
                { .id = ecs_id(Bloom), .access = EcsInOptional },
                { .id = ecs_id(Static), .access = EcsFilter },
            },
        },
        .callback = collect_static_cuboids,
        .phase = EcsPreRender,
        .after = { camera_system },
        .main_thread_only = true,
    };
    static_collect_system = ecs_system_init(&collect);
    ecs_system_desc_t finish = {
        .name = "FinishStaticCuboids",
        .callback = finish_static_cache,
        .phase = EcsPreRender,
        .after = { static_collect_system },
        .main_thread_only = true,
    };
    static_finish_system = ecs_system_init(&finish);
    return static_finish_system;
}

static void render_cuboids(ecs_iter_t *it) {
    const field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    const field_GlobalOrientation3d rotations = FIELD(GlobalOrientation3d, it, 1);
    const field_GlobalScale3d scales = FIELD(GlobalScale3d, it, 2);
    const field_Cuboid cuboids = FIELD(Cuboid, it, 3);
    const field_Color colors = FIELD(Color, it, 4);
    const field_Bloom blooms = FIELD(Bloom, it, 5);
    const float aspect = (float)(g_sigpu.frame_width) / (float)(g_sigpu.frame_height);
    const bool shared = ecs_field_is_shared(it, 3) && ecs_field_is_shared(it, 4) &&
                        (!blooms.data || ecs_field_is_shared(it, 5));

    if (shared) {
        render_shared_cuboids(it, positions, rotations, scales, cuboids, colors, blooms, aspect);
    } else {
        render_owned_cuboids(it, positions, rotations, scales, cuboids, colors, blooms, aspect);
    }
}

static void cull_static_cuboids(ecs_iter_t *it) {
    sigpu_static_cull((float)(g_sigpu.frame_width) / (float)(g_sigpu.frame_height));
}

static void register_render_cuboids() {
    ecs_system_desc_t cull = {
        .name = "CullStaticCuboids",
        .callback = cull_static_cuboids,
        .phase = EcsOnRender,
        .main_thread_only = true,
    };
    const ecs_system_id_t cull_system = ecs_system_init(&cull);
    ecs_system_desc_t system = {
        .name = "RenderCuboids",
        .query = {
            .components = {
                { .id = ecs_id(GlobalPosition3d), .access = EcsIn },
                { .id = ecs_id(GlobalOrientation3d), .access = EcsIn },
                { .id = ecs_id(GlobalScale3d), .access = EcsIn },
                { .id = ecs_id(Cuboid), .access = EcsIn },
                { .id = ecs_id(Color), .access = EcsIn },
                { .id = ecs_id(Bloom), .access = EcsInOptional },
                { .id = ecs_id(Static), .access = EcsNot },
            },
        },
        .callback = render_cuboids,
        .phase = EcsOnRender,
        .after = { cull_system },
        .main_thread_only = true,
    };
    ecs_system_init(&system);
}

static void begin_shadow_bounds(ecs_iter_t *it) {
    if (g_sigpu.shadows_enabled) {
        sigpu_shadow_bounds_begin((float)(g_sigpu.frame_width) / (float)(g_sigpu.frame_height));
    }
}

static void build_static_shadow_bounds(ecs_iter_t *it) {
    if (g_sigpu.shadows_enabled) {
        sigpu_static_shadow_bounds_extend();
    }
}

static void build_shadow_bounds(ecs_iter_t *it) {
    if (!g_sigpu.shadows_enabled) {
        return;
    }

    const field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    const field_GlobalScale3d scales = FIELD(GlobalScale3d, it, 1);
    const field_Cuboid cuboids = FIELD(Cuboid, it, 2);
    for (uint32_t index = 0; index < it->count; ++index) {
        const float width = AT(cuboids, index).width * AT(scales, index).x;
        const float height = AT(cuboids, index).height * AT(scales, index).y;
        const float depth = AT(cuboids, index).depth * AT(scales, index).z;
        sigpu_shadow_bounds_extend(
            (sigpu_vec3_t){ AT(positions, index).x,
                            AT(positions, index).y,
                            AT(positions, index).z },
            0.5f * sqrtf(width * width + height * height + depth * depth)
        );
    }
}

static void end_shadow_bounds(ecs_iter_t *it) {
    if (g_sigpu.shadows_enabled) {
        sigpu_shadow_bounds_end();
    }
}

static void register_shadow_bounds(ecs_system_id_t static_cache_system) {
    ecs_system_desc_t begin = {
        .name = "BeginShadowBounds",
        .callback = begin_shadow_bounds,
        .phase = EcsPreRender,
        .after = { static_cache_system },
        .main_thread_only = true,
    };
    const ecs_system_id_t begin_system = ecs_system_init(&begin);
    ecs_system_desc_t static_build = {
        .name = "BuildStaticShadowBounds",
        .callback = build_static_shadow_bounds,
        .phase = EcsPreRender,
        .after = { begin_system },
        .main_thread_only = true,
    };
    const ecs_system_id_t static_build_system = ecs_system_init(&static_build);
    ecs_system_desc_t build = {
        .name = "BuildShadowBounds",
        .query = {
            .components = {
                { .id = ecs_id(GlobalPosition3d), .access = EcsIn },
                { .id = ecs_id(GlobalScale3d), .access = EcsIn },
                { .id = ecs_id(Cuboid), .access = EcsIn },
                { .id = ecs_id(Static), .access = EcsNot },
            },
        },
        .callback = build_shadow_bounds,
        .phase = EcsPreRender,
        .after = { static_build_system },
        .main_thread_only = true,
    };
    const ecs_system_id_t build_system = ecs_system_init(&build);
    ecs_system_desc_t end = {
        .name = "EndShadowBounds",
        .callback = end_shadow_bounds,
        .phase = EcsPreRender,
        .after = { build_system },
        .main_thread_only = true,
    };
    ecs_system_init(&end);
}

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
}
static void set_multisampling(const void *ptr) {
    const Multisampling *multisampling = ptr;
    sigpu_msaa(multisampling->samples);
}
static void set_bloom(const void *ptr) {
    const BloomSettings *bloom = ptr;
    sigpu_bloom(bloom->enabled, bloom->threshold, bloom->intensity);
}

ECS_COMPONENT_DEFINE(Color, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Cuboid, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Bloom, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Camera);
ECS_MODULE_DEFINE(sigpu);
ECS_RESOURCE_DEFINE(WindowConfig);
ECS_RESOURCE_DEFINE(Sky, .on_set = set_sky);
ECS_RESOURCE_DEFINE(Sun, .on_set = set_sun);
ECS_RESOURCE_DEFINE(AmbientLight, .on_set = set_ambient);
ECS_RESOURCE_DEFINE(Fog, .on_set = set_fog);
ECS_RESOURCE_DEFINE(Shadows, .on_set = set_shadows);
ECS_RESOURCE_DEFINE(Multisampling, .on_set = set_multisampling);
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
    RESOURCE_ENTRY(Multisampling), RESOURCE_ENTRY(BloomSettings),
};
#undef RESOURCE_ENTRY

static void begin_rendering(ecs_iter_t *it) {
    if (!sigpu_begin_frame()) {
        ecs_quit();
    }
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
        sigpu_view_prepare((float)g_sigpu.frame_width / (float)g_sigpu.frame_height);
    }
}

static void end_rendering(ecs_iter_t *it) { sigpu_end_frame(); }

static void fini_rendering(void *data) {
    sigpu_fini();
    SDL_free(static_items);
    static_items = NULL;
    static_items_count = static_items_capacity = 0;
    static_cache_ready = false;
    static_collect_system = static_finish_system = 0;
    /* The source renderer has global GPU state; reset it for a subsequent world. */
    SDL_memset(&g_sigpu, 0, sizeof(g_sigpu));
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
    ECS_COMPONENT_REGISTER(Color, Cuboid, Bloom, Camera);
    for (size_t index = 0; index < sizeof(rendering_resources) / sizeof(*rendering_resources);
         index++) {
        rendering_resource *resource = &rendering_resources[index];
        ecs_resource_register(resource->id, resource->desc);
        resource->type = sireflect_register_struct(resource->reflection);
    }
    ecs_set_resource(
        WindowConfig,
        { .width = config.width, .height = config.height, .title = config.title }
    );
    const WindowConfig *window = ecs_get_resource_read(WindowConfig);
    sigpu_init(window->title, window->width, window->height, config.samples);
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
    ecs_set_resource(BloomSettings, { .enabled = true, .threshold = 0.0f, .intensity = 1.0f });
    const ecs_system_id_t input_system = sigpu_input_system();
    ecs_system(
        {
            .name = "BeginRendering",
            .phase = EcsPreUpdate,
            .after = { input_system },
            .callback = begin_rendering,
            .main_thread_only = true,
            .no_defer = true,
        }
    );
    const ecs_system_id_t camera_system = ecs_system({
        .name = "UpdateCamera", .phase = EcsPreRender,
        .query.components = {
            { .id = ecs_id(GlobalPosition3d), .access = EcsIn },
            { .id = ecs_id(GlobalOrientation3d), .access = EcsIn },
            { .id = ecs_id(Camera), .access = EcsIn },
        },
        .callback = update_camera, .main_thread_only = true,
    });
    sigpu_interaction_init(camera_system);
    const ecs_system_id_t static_cache_system = register_static_cache(camera_system);
    register_shadow_bounds(static_cache_system);
    register_render_cuboids();
    ecs_system(
        { .name = "EndRendering",
          .phase = EcsPostRender,
          .callback = end_rendering,
          .main_thread_only = true,
          .no_defer = true }
    );
    ecs_at_fini({ .callback = fini_rendering });
}

uint16_t sigpu_component_id(const char *name) {
#define COMPONENT_ID(type)                                                                         \
    if (strcmp(name, #type) == 0)                                                                  \
    return ecs_id(type)
    COMPONENT_ID(Color);
    COMPONENT_ID(Cuboid);
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
