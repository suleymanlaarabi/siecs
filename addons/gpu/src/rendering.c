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
_Static_assert(sizeof(sigpu_axis_instance_t) == 32, "owned axis instance layout");
_Static_assert(sizeof(sigpu_rotated_instance_t) == 40, "owned rotated instance layout");
_Static_assert(sizeof(sigpu_shared_axis_instance_t) == 24, "shared axis instance layout");
_Static_assert(sizeof(sigpu_shared_rotated_instance_t) == 32, "shared rotated instance layout");

typedef enum { Axis, Rotated } static_instance_kind;
typedef union {
    sigpu_axis_instance_t axis;
    sigpu_rotated_instance_t rotated;
} static_instance;
typedef struct {
    int32_t cell_x, cell_y, cell_z;
    float x, y, z, radius;
    sigpu_primitive_t primitive;
    static_instance_kind kind;
    static_instance instance;
} static_item;
static static_item *static_items;
static size_t static_items_count, static_items_capacity;
static bool static_cache_ready;
static ecs_system_id_t static_collect_systems[SIGPU_PRIMITIVE_COUNT];
static ecs_system_id_t static_finish_system;

static void invalidate_static_cache(ecs_observer_event_t *event) {
    if (!static_cache_ready)
        return;
    if (event->event == EcsOnSet && event->component != ecs_id(Position3d) &&
        event->component != ecs_id(Rotation3d) && event->component != ecs_id(Scale3d) &&
        event->component != ecs_id(Cuboid) && event->component != ecs_id(Cylinder) &&
        event->component != ecs_id(Sphere) && event->component != ecs_id(Color) &&
        event->component != ecs_id(Bloom))
        return;
    static_cache_ready = false;
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++)
        ecs_system_enable(static_collect_systems[p]);
    ecs_system_enable(static_finish_system);
}

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
DECLARE_FIELD(Cylinder);
DECLARE_FIELD(Sphere);
DECLARE_FIELD(Color);
DECLARE_FIELD(Bloom);
#define FIELD(T, it, index)                                                                        \
    ((field_##T){ ecs_field(it, index), ecs_field_is_shared(it, index) ? 0 : 1 })
#define AT(f, index) ((f).data[(index) * (f).stride])

typedef struct {
    float x, y, z;
} primitive_size_t;
typedef struct {
    int16_t x, y, z, w;
} packed_rotation;
static packed_rotation pack_rotation(GlobalOrientation3d q) {
    return (packed_rotation){ (int16_t)roundf(q.x * 32767.0f),
                              (int16_t)roundf(q.y * 32767.0f),
                              (int16_t)roundf(q.z * 32767.0f),
                              (int16_t)roundf(q.w * 32767.0f) };
}
static bool is_rotated(GlobalOrientation3d q) {
    return q.x != 0.0f || q.y != 0.0f || q.z != 0.0f || q.w != 1.0f;
}
static primitive_size_t primitive_size(sigpu_primitive_t primitive, const void *component) {
    switch (primitive) {
    case SIGPU_PRIMITIVE_CUBE: {
        const Cuboid *c = component;
        return (primitive_size_t){ c->width, c->height, c->depth };
    }
    case SIGPU_PRIMITIVE_CYLINDER: {
        const Cylinder *c = component;
        return (primitive_size_t){ c->radius * 2, c->height, c->radius * 2 };
    }
    case SIGPU_PRIMITIVE_SPHERE: {
        const Sphere *s = component;
        return (primitive_size_t){ s->radius * 2, s->radius * 2, s->radius * 2 };
    }
    default:
        return (primitive_size_t){ 0, 0, 0 };
    }
}
static primitive_size_t scaled_size(primitive_size_t size, GlobalScale3d scale) {
    return (primitive_size_t){ size.x * scale.x, size.y * scale.y, size.z * scale.z };
}
static float primitive_radius(sigpu_primitive_t primitive, primitive_size_t size) {
    float x = fabsf(size.x) * 0.5f, y = fabsf(size.y) * 0.5f, z = fabsf(size.z) * 0.5f;
    if (primitive == SIGPU_PRIMITIVE_SPHERE)
        return fmaxf(x, fmaxf(y, z));
    if (primitive == SIGPU_PRIMITIVE_CYLINDER)
        return hypotf(fmaxf(x, z), y);
    return sqrtf(x * x + y * y + z * z);
}
static Uint32 primitive_mesh(sigpu_primitive_t primitive, Uint32 lod) {
    if (primitive == SIGPU_PRIMITIVE_CUBE)
        return SIGPU_MESH_CUBE;
    return (primitive == SIGPU_PRIMITIVE_CYLINDER ? SIGPU_MESH_CYLINDER_LOW
                                                  : SIGPU_MESH_SPHERE_LOW) +
           lod;
}

static sigpu_shared_material_t
make_shared_material(primitive_size_t size, Color color, float bloom) {
    return (sigpu_shared_material_t){ .size_bloom = { size.x, size.y, size.z, bloom },
                                      .color = { g_sigpu.linear_lut[color.r] / 255.0f,
                                                 g_sigpu.linear_lut[color.g] / 255.0f,
                                                 g_sigpu.linear_lut[color.b] / 255.0f,
                                                 color.a / 255.0f } };
}
static sigpu_axis_instance_t
make_owned_axis(GlobalPosition3d p, primitive_size_t s, Color c, float bloom) {
    return (sigpu_axis_instance_t){ p.x,
                                    p.y,
                                    p.z,
                                    s.x,
                                    s.y,
                                    s.z,
                                    g_sigpu.linear_lut[c.r],
                                    g_sigpu.linear_lut[c.g],
                                    g_sigpu.linear_lut[c.b],
                                    c.a,
                                    bloom };
}
static sigpu_rotated_instance_t make_owned_rotated(
    GlobalPosition3d p,
    GlobalOrientation3d q,
    primitive_size_t s,
    Color c,
    float bloom
) {
    packed_rotation r = pack_rotation(q);
    return (sigpu_rotated_instance_t){ p.x,
                                       p.y,
                                       p.z,
                                       s.x,
                                       s.y,
                                       s.z,
                                       r.x,
                                       r.y,
                                       r.z,
                                       r.w,
                                       g_sigpu.linear_lut[c.r],
                                       g_sigpu.linear_lut[c.g],
                                       g_sigpu.linear_lut[c.b],
                                       c.a,
                                       bloom };
}

static bool visible(GlobalPosition3d p, float radius, float aspect) {
    sigpu_vec3_t center = { p.x, p.y, p.z };
    return sigpu_camera_visible(center, radius, aspect) ||
           (g_sigpu.shadows_enabled && sigpu_shadow_visible(center, radius));
}

static void record_shared(
    sigpu_shared_batch_t **batches,
    Uint32 *count,
    Uint32 *capacity,
    Uint32 first,
    Uint32 end,
    sigpu_shared_material_t material,
    Uint32 mesh,
    Uint32 lod,
    bool rotated
) {
    if (first == end)
        return;
    if (*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 16;
        *batches = SDL_realloc(*batches, *capacity * sizeof(**batches));
    }
    (*batches)[(*count)++] =
        (sigpu_shared_batch_t){ material,       first,        end - first,
                                (uint16_t)mesh, (uint8_t)lod, (uint8_t)rotated };
}
static void record_owned(
    sigpu_owned_batch_t **batches,
    Uint32 *count,
    Uint32 *capacity,
    Uint32 first,
    Uint32 end,
    Uint32 mesh
) {
    if (first == end)
        return;
    if (*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 16;
        *batches = SDL_realloc(*batches, *capacity * sizeof(**batches));
    }
    (*batches)[(*count)++] = (sigpu_owned_batch_t){ first, end - first, (uint16_t)mesh };
}

static const void *
primitive_field(ecs_iter_t *it, sigpu_primitive_t primitive, uint32_t index, ptrdiff_t stride) {
    switch (primitive) {
    case SIGPU_PRIMITIVE_CUBE:
        return &((const Cuboid *)ecs_field(it, 3))[index * stride];
    case SIGPU_PRIMITIVE_CYLINDER:
        return &((const Cylinder *)ecs_field(it, 3))[index * stride];
    case SIGPU_PRIMITIVE_SPHERE:
        return &((const Sphere *)ecs_field(it, 3))[index * stride];
    default:
        return NULL;
    }
}

static void render_shared_primitives(
    ecs_iter_t *it,
    sigpu_primitive_t primitive,
    field_GlobalPosition3d positions,
    field_GlobalOrientation3d rotations,
    field_GlobalScale3d scales,
    field_Color colors,
    field_Bloom blooms,
    float aspect
) {
    primitive_size_t base = primitive_size(primitive, primitive_field(it, primitive, 0, 0));
    Color color = AT(colors, 0);
    float bloom = blooms.data ? fmaxf(AT(blooms, 0).intensity, 0.0f) : 0.0f;
    sigpu_shared_material_t material = make_shared_material(base, color, bloom);
    Uint32 max_lod = primitive == SIGPU_PRIMITIVE_CUBE ? 0 : 2;
    for (Uint32 lod = 0; lod <= max_lod; lod++) {
        Uint32 first_axis = g_sigpu.shared_axis_count, first_rotated = g_sigpu.shared_rotated_count;
        for (uint32_t index = 0; index < it->count; index++) {
            GlobalPosition3d p = AT(positions, index);
            GlobalScale3d scale = AT(scales, index);
            primitive_size_t size = scaled_size(base, scale);
            float radius = primitive_radius(primitive, size);
            if (!visible(p, radius, aspect) ||
                (primitive != SIGPU_PRIMITIVE_CUBE &&
                 sigpu_primitive_lod((sigpu_vec3_t){ p.x, p.y, p.z }, radius) != lod))
                continue;
            GlobalOrientation3d q = AT(rotations, index);
            if (!is_rotated(q)) {
                if (g_sigpu.shared_axis_count + g_sigpu.owned_axis_count == g_sigpu.axis_capacity)
                    sigpu_axis_instances_grow();
                sigpu_shared_axis_instance_t *dst =
                    (sigpu_shared_axis_instance_t *)g_sigpu.axis_mapped;
                dst[g_sigpu.shared_axis_count++] =
                    (sigpu_shared_axis_instance_t){ p.x, p.y, p.z, scale.x, scale.y, scale.z };
            } else {
                if (g_sigpu.shared_rotated_count + g_sigpu.owned_rotated_count ==
                    g_sigpu.rotated_capacity)
                    sigpu_rotated_instances_grow();
                packed_rotation r = pack_rotation(q);
                sigpu_shared_rotated_instance_t *dst =
                    (sigpu_shared_rotated_instance_t *)g_sigpu.rotated_mapped;
                dst[g_sigpu.shared_rotated_count++] =
                    (sigpu_shared_rotated_instance_t){ p.x,     p.y, p.z, scale.x, scale.y,
                                                       scale.z, r.x, r.y, r.z,     r.w };
            }
        }
        Uint32 mesh = primitive_mesh(primitive, lod);
        record_shared(
            &g_sigpu.shared_axis_batches,
            &g_sigpu.shared_axis_batch_count,
            &g_sigpu.shared_axis_batch_capacity,
            first_axis,
            g_sigpu.shared_axis_count,
            material,
            mesh,
            lod,
            false
        );
        record_shared(
            &g_sigpu.shared_rotated_batches,
            &g_sigpu.shared_rotated_batch_count,
            &g_sigpu.shared_rotated_batch_capacity,
            first_rotated,
            g_sigpu.shared_rotated_count,
            material,
            mesh,
            lod,
            true
        );
        g_sigpu.any_bloom |= bloom > 0.0f && (first_axis != g_sigpu.shared_axis_count ||
                                              first_rotated != g_sigpu.shared_rotated_count);
    }
}

static void render_owned_primitives(
    ecs_iter_t *it,
    sigpu_primitive_t primitive,
    field_GlobalPosition3d positions,
    field_GlobalOrientation3d rotations,
    field_GlobalScale3d scales,
    field_Color colors,
    field_Bloom blooms,
    float aspect
) {
    ptrdiff_t shape_stride = ecs_field_is_shared(it, 3) ? 0 : 1;
    Uint32 max_lod = primitive == SIGPU_PRIMITIVE_CUBE ? 0 : 2;
    for (Uint32 lod = 0; lod <= max_lod; lod++) {
        Uint32 first_axis = g_sigpu.owned_axis_count, first_rotated = g_sigpu.owned_rotated_count;
        for (uint32_t index = 0; index < it->count; index++) {
            GlobalPosition3d p = AT(positions, index);
            primitive_size_t size = scaled_size(
                primitive_size(primitive, primitive_field(it, primitive, index, shape_stride)),
                AT(scales, index)
            );
            float radius = primitive_radius(primitive, size);
            if (!visible(p, radius, aspect) ||
                (primitive != SIGPU_PRIMITIVE_CUBE &&
                 sigpu_primitive_lod((sigpu_vec3_t){ p.x, p.y, p.z }, radius) != lod))
                continue;
            float bloom = blooms.data ? fmaxf(AT(blooms, index).intensity, 0.0f) : 0.0f;
            Color color = AT(colors, index);
            g_sigpu.any_bloom |= bloom > 0.0f;
            GlobalOrientation3d q = AT(rotations, index);
            if (!is_rotated(q)) {
                if (g_sigpu.shared_axis_count + g_sigpu.owned_axis_count == g_sigpu.axis_capacity)
                    sigpu_axis_instances_grow();
                sigpu_axis_instance_t *dst = (sigpu_axis_instance_t *)g_sigpu.axis_mapped;
                dst[g_sigpu.axis_capacity - ++g_sigpu.owned_axis_count] =
                    make_owned_axis(p, size, color, bloom);
            } else {
                if (g_sigpu.shared_rotated_count + g_sigpu.owned_rotated_count ==
                    g_sigpu.rotated_capacity)
                    sigpu_rotated_instances_grow();
                sigpu_rotated_instance_t *dst = (sigpu_rotated_instance_t *)g_sigpu.rotated_mapped;
                dst[g_sigpu.rotated_capacity - ++g_sigpu.owned_rotated_count] =
                    make_owned_rotated(p, q, size, color, bloom);
            }
        }
        Uint32 mesh = primitive_mesh(primitive, lod);
        record_owned(
            &g_sigpu.owned_axis_batches,
            &g_sigpu.owned_axis_batch_count,
            &g_sigpu.owned_axis_batch_capacity,
            first_axis,
            g_sigpu.owned_axis_count,
            mesh
        );
        record_owned(
            &g_sigpu.owned_rotated_batches,
            &g_sigpu.owned_rotated_batch_count,
            &g_sigpu.owned_rotated_batch_capacity,
            first_rotated,
            g_sigpu.owned_rotated_count,
            mesh
        );
    }
}
static int static_item_less(const void *a, const void *b) {
    const static_item *x = a, *y = b;
    if (x->cell_x != y->cell_x)
        return x->cell_x < y->cell_x ? -1 : 1;
    if (x->cell_y != y->cell_y)
        return x->cell_y < y->cell_y ? -1 : 1;
    if (x->cell_z != y->cell_z)
        return x->cell_z < y->cell_z ? -1 : 1;
    if (x->primitive != y->primitive)
        return (int)x->primitive - (int)y->primitive;
    return (int)x->kind - (int)y->kind;
}
static void collect_static_primitives(ecs_iter_t *it, sigpu_primitive_t primitive) {
    if (static_cache_ready)
        return;
    field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    field_GlobalOrientation3d rotations = FIELD(GlobalOrientation3d, it, 1);
    field_GlobalScale3d scales = FIELD(GlobalScale3d, it, 2);
    field_Color colors = FIELD(Color, it, 4);
    field_Bloom blooms = FIELD(Bloom, it, 5);
    ptrdiff_t shape_stride = ecs_field_is_shared(it, 3) ? 0 : 1;
    for (uint32_t index = 0; index < it->count; index++) {
        GlobalPosition3d p = AT(positions, index);
        GlobalOrientation3d q = AT(rotations, index);
        primitive_size_t size = scaled_size(
            primitive_size(primitive, primitive_field(it, primitive, index, shape_stride)),
            AT(scales, index)
        );
        Color color = AT(colors, index);
        float bloom = blooms.data ? fmaxf(AT(blooms, index).intensity, 0.0f) : 0.0f;
        static_item item = { .cell_x = (int32_t)floorf(p.x / SIGPU_STATIC_CHUNK_SIZE),
                             .cell_y = (int32_t)floorf(p.y / SIGPU_STATIC_CHUNK_SIZE),
                             .cell_z = (int32_t)floorf(p.z / SIGPU_STATIC_CHUNK_SIZE),
                             .x = p.x,
                             .y = p.y,
                             .z = p.z,
                             .radius = primitive_radius(primitive, size),
                             .primitive = primitive,
                             .kind = is_rotated(q) ? Rotated : Axis };
        if (item.kind == Rotated)
            item.instance.rotated = make_owned_rotated(p, q, size, color, bloom);
        else
            item.instance.axis = make_owned_axis(p, size, color, bloom);
        if (static_items_count == static_items_capacity) {
            static_items_capacity = static_items_capacity ? static_items_capacity * 2 : 256;
            static_items = SDL_realloc(static_items, static_items_capacity * sizeof(*static_items));
        }
        static_items[static_items_count++] = item;
    }
}
static void collect_static_cuboids(ecs_iter_t *it) {
    collect_static_primitives(it, SIGPU_PRIMITIVE_CUBE);
}
static void collect_static_cylinders(ecs_iter_t *it) {
    collect_static_primitives(it, SIGPU_PRIMITIVE_CYLINDER);
}
static void collect_static_spheres(ecs_iter_t *it) {
    collect_static_primitives(it, SIGPU_PRIMITIVE_SPHERE);
}
static void finish_static_cache(ecs_iter_t *it) {
    if (static_cache_ready)
        return;
    if (static_items_count)
        qsort(static_items, static_items_count, sizeof(*static_items), static_item_less);
    sigpu_static_upload_t upload = { 0 };
    sigpu_axis_instance_t *axis[SIGPU_PRIMITIVE_COUNT] = { 0 };
    sigpu_rotated_instance_t *rotated[SIGPU_PRIMITIVE_COUNT] = { 0 };
    Uint32 axis_count[SIGPU_PRIMITIVE_COUNT] = { 0 }, rotated_count[SIGPU_PRIMITIVE_COUNT] = { 0 };
    size_t axis_capacity[SIGPU_PRIMITIVE_COUNT] = { 0 },
           rotated_capacity[SIGPU_PRIMITIVE_COUNT] = { 0 };
    for (size_t i = 0; i < static_items_count; i++) {
        static_item *item = &static_items[i];
        if (item->kind == Axis)
            axis_capacity[item->primitive]++;
        else
            rotated_capacity[item->primitive]++;
    }
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        axis[p] = SDL_malloc(axis_capacity[p] * sizeof(sigpu_axis_instance_t));
        rotated[p] = SDL_malloc(rotated_capacity[p] * sizeof(sigpu_rotated_instance_t));
    }
    sigpu_static_chunk_t *chunks = SDL_malloc(static_items_count * sizeof(*chunks));
    size_t chunk_count = 0;
    for (size_t first = 0; first < static_items_count;) {
        size_t end = first + 1;
        while (end < static_items_count && static_items[end].cell_x == static_items[first].cell_x &&
               static_items[end].cell_y == static_items[first].cell_y &&
               static_items[end].cell_z == static_items[first].cell_z)
            end++;
        sigpu_static_chunk_t chunk = { 0 };
        for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
            chunk.axis[p].first = axis_count[p];
            chunk.rotated[p].first = rotated_count[p];
        }
        float min_x = INFINITY, min_y = INFINITY, min_z = INFINITY;
        float max_x = -INFINITY, max_y = -INFINITY, max_z = -INFINITY;
        for (size_t i = first; i < end; i++) {
            static_item *item = &static_items[i];
            Uint32 p = item->primitive;
            min_x = fminf(min_x, item->x - item->radius);
            max_x = fmaxf(max_x, item->x + item->radius);
            min_y = fminf(min_y, item->y - item->radius);
            max_y = fmaxf(max_y, item->y + item->radius);
            min_z = fminf(min_z, item->z - item->radius);
            max_z = fmaxf(max_z, item->z + item->radius);
            chunk.primitive_radius[p] = fmaxf(chunk.primitive_radius[p], item->radius);
            chunk.primitive_center[p].x += item->x;
            chunk.primitive_center[p].y += item->y;
            chunk.primitive_center[p].z += item->z;
            if (item->kind == Axis) {
                axis[p][axis_count[p]++] = item->instance.axis;
                chunk.bloom |= item->instance.axis.bloom > 0.0f;
            } else {
                rotated[p][rotated_count[p]++] = item->instance.rotated;
                chunk.bloom |= item->instance.rotated.bloom > 0.0f;
            }
        }
        for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
            chunk.axis[p].count = axis_count[p] - chunk.axis[p].first;
            chunk.rotated[p].count = rotated_count[p] - chunk.rotated[p].first;
            Uint32 count = chunk.axis[p].count + chunk.rotated[p].count;
            if (count) {
                chunk.primitive_center[p].x /= count;
                chunk.primitive_center[p].y /= count;
                chunk.primitive_center[p].z /= count;
            }
        }
        chunk.center = (sigpu_vec3_t){ (min_x + max_x) * 0.5f,
                                       (min_y + max_y) * 0.5f,
                                       (min_z + max_z) * 0.5f };
        chunk.radius =
            0.5f * sqrtf(
                       (max_x - min_x) * (max_x - min_x) + (max_y - min_y) * (max_y - min_y) +
                       (max_z - min_z) * (max_z - min_z)
                   );
        chunks[chunk_count++] = chunk;
        first = end;
    }
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        upload.axis[p] = axis[p];
        upload.axis_count[p] = axis_count[p];
        upload.rotated[p] = rotated[p];
        upload.rotated_count[p] = rotated_count[p];
    }
    upload.chunks = chunks;
    upload.chunk_count = (Uint32)chunk_count;
    sigpu_static_upload(&upload);
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        SDL_free(axis[p]);
        SDL_free(rotated[p]);
    }
    SDL_free(chunks);
    SDL_free(static_items);
    static_items = NULL;
    static_items_count = static_items_capacity = 0;
    static_cache_ready = true;
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++)
        ecs_system_disable(static_collect_systems[p]);
    ecs_system_disable(static_finish_system);
}
static ecs_system_id_t register_static_cache(ecs_system_id_t camera_system) {
    uint16_t shapes[SIGPU_PRIMITIVE_COUNT] = { ecs_id(Cuboid), ecs_id(Cylinder), ecs_id(Sphere) };
    void (*callbacks[SIGPU_PRIMITIVE_COUNT])(ecs_iter_t *) = { collect_static_cuboids,
                                                               collect_static_cylinders,
                                                               collect_static_spheres };
    const char *names[SIGPU_PRIMITIVE_COUNT] = { "CollectStaticCuboids",
                                                 "CollectStaticCylinders",
                                                 "CollectStaticSpheres" };
    ecs_system_id_t previous = camera_system;
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        ecs_system_desc_t collect={.name=names[p],.query={.components={
            {.id=ecs_id(GlobalPosition3d),.access=EcsIn},
            {.id=ecs_id(GlobalOrientation3d),.access=EcsIn},
            {.id=ecs_id(GlobalScale3d),.access=EcsIn},
            {.id=shapes[p],.access=EcsIn},
            {.id=ecs_id(Color),.access=EcsIn},
            {.id=ecs_id(Bloom),.access=EcsInOptional},
            {.id=ecs_id(Static3dReady),.access=EcsFilter},
        }},.callback=callbacks[p],.phase=EcsPreRender,.after={previous},.main_thread_only=true};
        previous = static_collect_systems[p] = ecs_system_init(&collect);
    }
    ecs_system_desc_t finish = { .name = "FinishStaticPrimitives",
                                 .callback = finish_static_cache,
                                 .phase = EcsPreRender,
                                 .after = { previous },
                                 .main_thread_only = true };
    return static_finish_system = ecs_system_init(&finish);
}
static void render_primitives(ecs_iter_t *it, sigpu_primitive_t primitive) {
    Uint64 start = g_sigpu.profile_enabled ? SDL_GetTicksNS() : 0;
    field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    field_GlobalOrientation3d rotations = FIELD(GlobalOrientation3d, it, 1);
    field_GlobalScale3d scales = FIELD(GlobalScale3d, it, 2);
    field_Color colors = FIELD(Color, it, 4);
    field_Bloom blooms = FIELD(Bloom, it, 5);
    float aspect = (float)g_sigpu.frame_width / g_sigpu.frame_height;
    bool shared = ecs_field_is_shared(it, 3) && ecs_field_is_shared(it, 4) &&
                  (!blooms.data || ecs_field_is_shared(it, 5));
    if (shared)
        render_shared_primitives(
            it,
            primitive,
            positions,
            rotations,
            scales,
            colors,
            blooms,
            aspect
        );
    else
        render_owned_primitives(
            it,
            primitive,
            positions,
            rotations,
            scales,
            colors,
            blooms,
            aspect
        );
    if (g_sigpu.profile_enabled)
        g_sigpu.collect_ns += SDL_GetTicksNS() - start;
}
static void render_cuboids(ecs_iter_t *it) { render_primitives(it, SIGPU_PRIMITIVE_CUBE); }
static void render_cylinders(ecs_iter_t *it) { render_primitives(it, SIGPU_PRIMITIVE_CYLINDER); }
static void render_spheres(ecs_iter_t *it) { render_primitives(it, SIGPU_PRIMITIVE_SPHERE); }
static void cull_static_primitives(ecs_iter_t *it) {
    Uint64 start = g_sigpu.profile_enabled ? SDL_GetTicksNS() : 0;
    sigpu_static_cull((float)g_sigpu.frame_width / g_sigpu.frame_height);
    if (g_sigpu.profile_enabled)
        g_sigpu.cull_ns += SDL_GetTicksNS() - start;
}
static void register_render_primitives(void) {
    ecs_system_desc_t cull = { .name = "CullStaticPrimitives",
                               .callback = cull_static_primitives,
                               .phase = EcsOnRender,
                               .main_thread_only = true };
    ecs_system_id_t previous = ecs_system_init(&cull);
    uint16_t shapes[SIGPU_PRIMITIVE_COUNT] = { ecs_id(Cuboid), ecs_id(Cylinder), ecs_id(Sphere) };
    void (*callbacks[SIGPU_PRIMITIVE_COUNT])(ecs_iter_t *) = { render_cuboids,
                                                               render_cylinders,
                                                               render_spheres };
    const char *names[SIGPU_PRIMITIVE_COUNT] = { "RenderCuboids",
                                                 "RenderCylinders",
                                                 "RenderSpheres" };
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        ecs_system_desc_t system={.name=names[p],.query={.components={
            {.id=ecs_id(GlobalPosition3d),.access=EcsIn},
            {.id=ecs_id(GlobalOrientation3d),.access=EcsIn},
            {.id=ecs_id(GlobalScale3d),.access=EcsIn},
            {.id=shapes[p],.access=EcsIn},
            {.id=ecs_id(Color),.access=EcsIn},
            {.id=ecs_id(Bloom),.access=EcsInOptional},
            {.id=ecs_id(Static3dReady),.access=EcsNot},
        }},.callback=callbacks[p],.phase=EcsOnRender,.after={previous},.main_thread_only=true};
        previous = ecs_system_init(&system);
    }
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

static void build_primitive_shadow_bounds(ecs_iter_t *it, sigpu_primitive_t primitive) {
    if (!g_sigpu.shadows_enabled)
        return;
    field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    field_GlobalScale3d scales = FIELD(GlobalScale3d, it, 1);
    ptrdiff_t stride = ecs_field_is_shared(it, 2) ? 0 : 1;
    for (uint32_t i = 0; i < it->count; i++) {
        GlobalPosition3d p = AT(positions, i);
        primitive_size_t size = scaled_size(
            primitive_size(
                primitive,
                primitive == SIGPU_PRIMITIVE_CUBE
                    ? (const void *)&((const Cuboid *)ecs_field(it, 2))[i * stride]
                : primitive == SIGPU_PRIMITIVE_CYLINDER
                    ? (const void *)&((const Cylinder *)ecs_field(it, 2))[i * stride]
                    : (const void *)&((const Sphere *)ecs_field(it, 2))[i * stride]
            ),
            AT(scales, i)
        );
        sigpu_shadow_bounds_extend(
            (sigpu_vec3_t){ p.x, p.y, p.z },
            primitive_radius(primitive, size)
        );
    }
}
static void build_cuboid_shadow_bounds(ecs_iter_t *it) {
    build_primitive_shadow_bounds(it, SIGPU_PRIMITIVE_CUBE);
}
static void build_cylinder_shadow_bounds(ecs_iter_t *it) {
    build_primitive_shadow_bounds(it, SIGPU_PRIMITIVE_CYLINDER);
}
static void build_sphere_shadow_bounds(ecs_iter_t *it) {
    build_primitive_shadow_bounds(it, SIGPU_PRIMITIVE_SPHERE);
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
    uint16_t shapes[SIGPU_PRIMITIVE_COUNT] = { ecs_id(Cuboid), ecs_id(Cylinder), ecs_id(Sphere) };
    void (*callbacks[SIGPU_PRIMITIVE_COUNT])(ecs_iter_t *) = { build_cuboid_shadow_bounds,
                                                               build_cylinder_shadow_bounds,
                                                               build_sphere_shadow_bounds };
    const char *names[SIGPU_PRIMITIVE_COUNT] = { "BuildCuboidShadowBounds",
                                                 "BuildCylinderShadowBounds",
                                                 "BuildSphereShadowBounds" };
    ecs_system_id_t build_system = static_build_system;
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        ecs_system_desc_t build={.name=names[p],.query={.components={
            {.id=ecs_id(GlobalPosition3d),.access=EcsIn},
            {.id=ecs_id(GlobalScale3d),.access=EcsIn},
            {.id=shapes[p],.access=EcsIn},
            {.id=ecs_id(Static3dReady),.access=EcsNot},
        }},.callback=callbacks[p],.phase=EcsPreRender,.after={build_system},.main_thread_only=true};
        build_system = ecs_system_init(&build);
    }
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
static void set_camera_clip(const void *ptr) {
    const CameraClip *clip = ptr;
    if (clip->near_plane > 0.0f && clip->far_plane > clip->near_plane) {
        g_sigpu.camera.near_plane = clip->near_plane;
        g_sigpu.camera.far_plane = clip->far_plane;
    }
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
ECS_COMPONENT_DEFINE(Cylinder, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Sphere, .inheritance = EcsInheritShared);
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
    SDL_memset(static_collect_systems, 0, sizeof(static_collect_systems));
    static_finish_system = 0;
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
    ECS_COMPONENT_REGISTER(Color, Cuboid, Cylinder, Sphere, Bloom, Camera);
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
    ecs_set_resource(CameraClip, { .near_plane = 0.1f, .far_plane = 1000.0f });
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
    ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .components = { ecs_filter(Static3dReady), ecs_in_optional(Abstract) } },
            .callback = invalidate_static_cache,
        }
    );
    ecs_observer(
        {
            .on = EcsOnAdd,
            .query = { .components = { ecs_filter(Static3dReady), ecs_in_optional(Abstract) } },
            .callback = invalidate_static_cache,
        }
    );
    ecs_observer(
        {
            .on = EcsOnRemove,
            .query = { .components = { ecs_filter(Static3dReady), ecs_in_optional(Abstract) } },
            .callback = invalidate_static_cache,
        }
    );
    ecs_observer(
        {
            .on = EcsOnRelationSet,
            .query = { .components = { ecs_filter(Static3dReady), ecs_in_optional(Abstract) } },
            .callback = invalidate_static_cache,
        }
    );
    ecs_observer(
        {
            .on = EcsOnRelationRemove,
            .query = { .components = { ecs_filter(Static3dReady), ecs_in_optional(Abstract) } },
            .callback = invalidate_static_cache,
        }
    );
    register_shadow_bounds(static_cache_system);
    register_render_primitives();
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
