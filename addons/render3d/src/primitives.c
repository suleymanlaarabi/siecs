#include "render3d_internal.h"

/*
 * Built-in geometries (RFC section 12.2): one vertex and one index buffer
 * hold the cube and three levels of cylinder and sphere; each primitive is
 * a GpuGeometry whose LODs are ranges of those buffers.
 */

typedef struct {
    float x, y, z;
    int8_t nx, ny, nz, nw;
} r3d_vertex_t;

_Static_assert(sizeof(r3d_vertex_t) == 16, "primitive vertex layout");

static const r3d_vertex_t cube_vertices[24] = {
    { -0.5f, -0.5f, -0.5f, 0, 0, -127, 0 }, { 0.5f, -0.5f, -0.5f, 0, 0, -127, 0 },
    { 0.5f, 0.5f, -0.5f, 0, 0, -127, 0 },   { -0.5f, 0.5f, -0.5f, 0, 0, -127, 0 },
    { 0.5f, -0.5f, 0.5f, 0, 0, 127, 0 },    { -0.5f, -0.5f, 0.5f, 0, 0, 127, 0 },
    { -0.5f, 0.5f, 0.5f, 0, 0, 127, 0 },    { 0.5f, 0.5f, 0.5f, 0, 0, 127, 0 },
    { -0.5f, -0.5f, 0.5f, -127, 0, 0, 0 },  { -0.5f, -0.5f, -0.5f, -127, 0, 0, 0 },
    { -0.5f, 0.5f, -0.5f, -127, 0, 0, 0 },  { -0.5f, 0.5f, 0.5f, -127, 0, 0, 0 },
    { 0.5f, -0.5f, -0.5f, 127, 0, 0, 0 },   { 0.5f, -0.5f, 0.5f, 127, 0, 0, 0 },
    { 0.5f, 0.5f, 0.5f, 127, 0, 0, 0 },     { 0.5f, 0.5f, -0.5f, 127, 0, 0, 0 },
    { -0.5f, 0.5f, -0.5f, 0, 127, 0, 0 },   { 0.5f, 0.5f, -0.5f, 0, 127, 0, 0 },
    { 0.5f, 0.5f, 0.5f, 0, 127, 0, 0 },     { -0.5f, 0.5f, 0.5f, 0, 127, 0, 0 },
    { -0.5f, -0.5f, 0.5f, 0, -127, 0, 0 },  { 0.5f, -0.5f, 0.5f, 0, -127, 0, 0 },
    { 0.5f, -0.5f, -0.5f, 0, -127, 0, 0 },  { -0.5f, -0.5f, -0.5f, 0, -127, 0, 0 }
};

static const uint16_t cube_indices[36] = { 0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,
                                           8,  9,  10, 10, 11, 8,  12, 13, 14, 14, 15, 12,
                                           16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20 };

typedef struct {
    r3d_vertex_t *vertices;
    uint16_t *indices;
    uint32_t vertex_count, index_count;
} builder_t;

static void add_vertex(builder_t *b, float x, float y, float z, float nx, float ny, float nz) {
    b->vertices[b->vertex_count++] = (r3d_vertex_t){
        x, y, z, (int8_t)lroundf(nx * 127.0f), (int8_t)lroundf(ny * 127.0f),
        (int8_t)lroundf(nz * 127.0f), 0 };
}

/* Wound like the cube, for back-face culling. */
static void add_triangle(builder_t *b, uint16_t a, uint16_t c, uint16_t d) {
    b->indices[b->index_count++] = a;
    b->indices[b->index_count++] = d;
    b->indices[b->index_count++] = c;
}

static void add_cylinder(builder_t *b, uint32_t segments) {
    const uint16_t side = (uint16_t)b->vertex_count;
    for (uint32_t i = 0; i <= segments; i++) {
        const float angle = 2.0f * R3D_PI * (float)i / (float)segments;
        const float x = cosf(angle), z = sinf(angle);
        add_vertex(b, x * 0.5f, -0.5f, z * 0.5f, x, 0, z);
        add_vertex(b, x * 0.5f, 0.5f, z * 0.5f, x, 0, z);
    }
    for (uint32_t i = 0; i < segments; i++) {
        const uint16_t n = side + (uint16_t)(2 * i);
        add_triangle(b, n, n + 1, n + 3);
        add_triangle(b, n, n + 3, n + 2);
    }
    for (int cap = 0; cap < 2; cap++) {
        const float y = cap == 0 ? 0.5f : -0.5f;
        const float ny = cap == 0 ? 1.0f : -1.0f;
        const uint16_t center = (uint16_t)b->vertex_count;
        add_vertex(b, 0, y, 0, 0, ny, 0);
        for (uint32_t i = 0; i < segments; i++) {
            const float angle = 2.0f * R3D_PI * (float)i / (float)segments;
            add_vertex(b, cosf(angle) * 0.5f, y, sinf(angle) * 0.5f, 0, ny, 0);
        }
        for (uint32_t i = 0; i < segments; i++) {
            const uint16_t a = center + 1 + (uint16_t)i;
            const uint16_t c = center + 1 + (uint16_t)((i + 1) % segments);
            if (cap == 0) {
                add_triangle(b, center, c, a);
            } else {
                add_triangle(b, center, a, c);
            }
        }
    }
}

static void add_sphere(builder_t *b, uint32_t slices, uint32_t stacks) {
    const uint16_t base = (uint16_t)b->vertex_count;
    for (uint32_t row = 0; row <= stacks; row++) {
        const float phi = R3D_PI * (float)row / (float)stacks;
        const float y = cosf(phi), ring = sinf(phi);
        for (uint32_t col = 0; col <= slices; col++) {
            const float angle = 2.0f * R3D_PI * (float)col / (float)slices;
            const float x = ring * cosf(angle), z = ring * sinf(angle);
            add_vertex(b, x * 0.5f, y * 0.5f, z * 0.5f, x, y, z);
        }
    }
    for (uint32_t row = 0; row < stacks; row++) {
        for (uint32_t col = 0; col < slices; col++) {
            const uint16_t a = base + (uint16_t)(row * (slices + 1) + col);
            const uint16_t c = a + (uint16_t)(slices + 1);
            if (row > 0) {
                add_triangle(b, a, a + 1, c);
            }
            if (row + 1 < stacks) {
                add_triangle(b, a + 1, c + 1, c);
            }
        }
    }
}

static ecs_entity_t named(const char *name) { return ecs_new_named(0, name); }

/* LOD ranges by pixel size of the projected radius, as in sigpu v1. */
static const float lod_pixels[3] = { 8.0f, 32.0f, 0.0f };

static void geometries_create(void) {
    builder_t b = { .vertices = malloc(2048 * sizeof(r3d_vertex_t)),
                    .indices = malloc(8192 * sizeof(uint16_t)) };
    GpuGeometryLod lods[7];
    for (uint32_t mesh = 0; mesh < 7; mesh++) {
        const uint32_t first = b.index_count;
        const int32_t vertex_offset = (int32_t)b.vertex_count;
        if (mesh == 0) {
            memcpy(b.vertices, cube_vertices, sizeof(cube_vertices));
            memcpy(b.indices, cube_indices, sizeof(cube_indices));
            b.vertex_count = 24;
            b.index_count = 36;
        } else if (mesh <= 3) {
            static const uint32_t segments[3] = { 8, 12, 24 };
            add_cylinder(&b, segments[mesh - 1]);
        } else {
            static const uint32_t slices[3] = { 8, 12, 32 }, stacks[3] = { 4, 6, 16 };
            add_sphere(&b, slices[mesh - 4], stacks[mesh - 4]);
        }
        /* Indices are relative to the mesh's vertex offset. */
        if (mesh) {
            for (uint32_t i = first; i < b.index_count; i++) {
                b.indices[i] -= (uint16_t)vertex_offset;
            }
        }
        lods[mesh] = (GpuGeometryLod){ .first_index = first, .index_count = b.index_count - first,
                                       .vertex_offset = vertex_offset,
                                       .max_pixels = mesh ? lod_pixels[(mesh - 1) % 3] : 0.0f };
    }
    const ecs_entity_t vertices = named("sirender3d::primitives::vertices");
    ecs_set(vertices, GpuBuffer, { .size = b.vertex_count * (uint32_t)sizeof(r3d_vertex_t),
                                   .usage = GpuBufferVertex });
    sigpu_buffer_upload(vertices, 0, b.vertices, b.vertex_count * (uint32_t)sizeof(r3d_vertex_t));
    const uint32_t index_bytes = (b.index_count * (uint32_t)sizeof(uint16_t) + 3u) & ~3u;
    const ecs_entity_t indices = named("sirender3d::primitives::indices");
    ecs_set(indices, GpuBuffer, { .size = index_bytes, .usage = GpuBufferIndex });
    sigpu_buffer_upload(indices, 0, b.indices, b.index_count * (uint32_t)sizeof(uint16_t));
    free(b.vertices);
    free(b.indices);
    const char *names[3] = { "sirender3d::primitives::cube", "sirender3d::primitives::cylinder",
                             "sirender3d::primitives::sphere" };
    for (uint32_t kind = 0; kind < 3; kind++) {
        GpuGeometry geometry = {
            .vertices = vertices,
            .indices = indices,
            .stride = sizeof(r3d_vertex_t),
            .index_size = 2,
            .lod_count = kind == 0 ? 1 : 3,
            .attribute_count = 2,
            .attributes = { { .name = "position", .format = GpuVertexFloat3, .offset = 0 },
                            { .name = "normal", .format = GpuVertexByte4Norm, .offset = 12 } },
            .bounds = { 0, 0, 0, 0.87f },
        };
        for (uint32_t l = 0; l < geometry.lod_count; l++) {
            geometry.lods[l] = lods[kind == 0 ? 0 : 1 + (kind - 1) * 3 + l];
        }
        r3d.geometries[kind] = named(names[kind]);
        ecs_set_cid(r3d.geometries[kind], ecs_id(GpuGeometry), &geometry);
    }
}

/* ----------------------------------------------------------------------
 * Extraction
 * ---------------------------------------------------------------------- */

typedef struct {
    const void *data;
    ptrdiff_t stride;
} field_t;

static field_t field(ecs_iter_t *it, uint16_t index, size_t size) {
    return (field_t){ ecs_field(it, index), ecs_field_is_shared(it, index) ? 0 : (ptrdiff_t)size };
}

#define AT(type, f, i) (*(const type *)((const uint8_t *)(f).data + (i) * (f).stride))

static uint32_t pack_color(const Color *color) {
    return color ? (uint32_t)color->r | (uint32_t)color->g << 8 | (uint32_t)color->b << 16 |
                       (uint32_t)color->a << 24
                 : 0xffffffffu;
}

/* Size of the shape in world units before rotation, and its bounding radius. */
static void shape_size(uint32_t kind, const void *shape, const GlobalScale3d *scale, float out[3],
                       float *radius) {
    if (kind == Render3dCuboids) {
        const Cuboid *c = shape;
        out[0] = c->width * scale->x;
        out[1] = c->height * scale->y;
        out[2] = c->depth * scale->z;
        *radius = 0.5f * sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    } else if (kind == Render3dCylinders) {
        const Cylinder *c = shape;
        out[0] = 2.0f * c->radius * scale->x;
        out[1] = c->height * scale->y;
        out[2] = 2.0f * c->radius * scale->z;
        *radius = hypotf(0.5f * fmaxf(fabsf(out[0]), fabsf(out[2])), 0.5f * fabsf(out[1]));
    } else {
        const Sphere *c = shape;
        out[0] = 2.0f * c->radius * scale->x;
        out[1] = 2.0f * c->radius * scale->y;
        out[2] = 2.0f * c->radius * scale->z;
        *radius = 0.5f * fmaxf(fabsf(out[0]), fmaxf(fabsf(out[1]), fabsf(out[2])));
    }
}

static void extract_primitives(ecs_iter_t *it, uint32_t kind, size_t shape_bytes) {
    const field_t positions = field(it, 0, sizeof(GlobalPosition3d));
    const field_t orientations = field(it, 1, sizeof(GlobalOrientation3d));
    const field_t scales = field(it, 2, sizeof(GlobalScale3d));
    const field_t shapes = field(it, 3, shape_bytes);
    const field_t colors = field(it, 4, sizeof(Color));
    const field_t blooms = field(it, 5, sizeof(Bloom));
    r3d_primitive_t *out = sigpu_instances(it);
    GpuSphere *bounds = sigpu_bounds(it);
    if (!out) {
        return;
    }
    for (uint32_t i = 0; i < it->count; i++) {
        const GlobalPosition3d p = AT(GlobalPosition3d, positions, i);
        const GlobalScale3d scale = AT(GlobalScale3d, scales, i);
        float size[3], radius;
        shape_size(kind, (const uint8_t *)shapes.data + i * shapes.stride, &scale, size, &radius);
        r3d_primitive_t *instance = &out[i];
        instance->position[0] = p.x;
        instance->position[1] = p.y;
        instance->position[2] = p.z;
        memcpy(instance->scale, size, sizeof(size));
        r3d_pack_rotation(&AT(GlobalOrientation3d, orientations, i), instance->rotation);
        instance->color = pack_color(colors.data ? &AT(Color, colors, i) : NULL);
        instance->bloom = blooms.data ? AT(Bloom, blooms, i).intensity : 0.0f;
        bounds[i] = (GpuSphere){ p.x, p.y, p.z, radius };
    }
}

static void extract_cuboids(ecs_iter_t *it) {
    extract_primitives(it, Render3dCuboids, sizeof(Cuboid));
}
static void extract_spheres(ecs_iter_t *it) {
    extract_primitives(it, Render3dSpheres, sizeof(Sphere));
}
static void extract_cylinders(ecs_iter_t *it) {
    extract_primitives(it, Render3dCylinders, sizeof(Cylinder));
}

static void extract_meshes(ecs_iter_t *it) {
    const field_t positions = field(it, 0, sizeof(GlobalPosition3d));
    const field_t orientations = field(it, 1, sizeof(GlobalOrientation3d));
    const field_t scales = field(it, 2, sizeof(GlobalScale3d));
    r3d_mesh_t *out = sigpu_instances(it);
    GpuSphere *bounds = sigpu_bounds(it);
    if (!out) {
        return;
    }
    const ecs_entity_t geometry_entity = ecs_target_shared(it, DrawGeometry);
    const GpuGeometry *geometry =
        geometry_entity ? ecs_try_get(geometry_entity, GpuGeometry) : NULL;
    const float local[4] = { geometry ? geometry->bounds[0] : 0, geometry ? geometry->bounds[1] : 0,
                             geometry ? geometry->bounds[2] : 0,
                             geometry ? geometry->bounds[3] : 1 };
    for (uint32_t i = 0; i < it->count; i++) {
        const GlobalPosition3d p = AT(GlobalPosition3d, positions, i);
        const GlobalScale3d s = AT(GlobalScale3d, scales, i);
        r3d_mesh_t *instance = &out[i];
        instance->position[0] = p.x;
        instance->position[1] = p.y;
        instance->position[2] = p.z;
        instance->scale[0] = s.x;
        instance->scale[1] = s.y;
        instance->scale[2] = s.z;
        r3d_pack_rotation(&AT(GlobalOrientation3d, orientations, i), instance->rotation);
        const float scale = fmaxf(fabsf(s.x), fmaxf(fabsf(s.y), fabsf(s.z)));
        /* The local center is ignored for rotation: the radius covers it. */
        const float offset = sqrtf(local[0] * local[0] + local[1] * local[1] + local[2] * local[2]);
        bounds[i] = (GpuSphere){ p.x, p.y, p.z, (local[3] + offset) * scale };
    }
}

static ecs_entity_t primitive_feature(const char *name, ecs_component_t shape, uint32_t kind,
                                      void (*extract)(ecs_iter_t *)) {
    return sigpu_feature(&(GpuFeatureDesc){
        .name = name,
        .instance = ecs_id(Render3dPrimitiveInstance),
        .query = { .components = {
                       ecs_in_changed(GlobalPosition3d),
                       ecs_in_changed(GlobalOrientation3d),
                       ecs_in_changed(GlobalScale3d),
                       { shape, EcsIn | ECS_TERM_CHANGED },
                       { ecs_id(Color), EcsInOptional | ECS_TERM_CHANGED },
                       { ecs_id(Bloom), EcsInOptional | ECS_TERM_CHANGED },
                   } },
        .extract = extract,
        .geometry = r3d.geometries[kind == Render3dCuboids ? 0 : kind == Render3dSpheres ? 2 : 1],
        .program = r3d.primitive_program,
        .culling = GpuCullSphere,
        .ordering = GpuOrderBatched,
    });
}

void r3d_primitives_init(void) {
    geometries_create();
    r3d.features[Render3dCuboids] = primitive_feature("sirender3d::cuboids", ecs_id(Cuboid),
                                                      Render3dCuboids, extract_cuboids);
    r3d.features[Render3dSpheres] = primitive_feature("sirender3d::spheres", ecs_id(Sphere),
                                                      Render3dSpheres, extract_spheres);
    r3d.features[Render3dCylinders] = primitive_feature("sirender3d::cylinders", ecs_id(Cylinder),
                                                        Render3dCylinders, extract_cylinders);
    r3d.default_material = named("sirender3d::materials::default");
    ecs_set(r3d.default_material, StandardMaterial, { .base_color = { 255, 255, 255, 255 } });
    r3d.features[Render3dMeshes] = sigpu_feature(&(GpuFeatureDesc){
        .name = "sirender3d::meshes",
        .instance = ecs_id(Render3dMeshInstance),
        .query = { .components = { ecs_in_changed(GlobalPosition3d),
                                   ecs_in_changed(GlobalOrientation3d),
                                   ecs_in_changed(GlobalScale3d) },
                   .relations = { ecs_rel(DrawGeometry), ecs_rel_opt(DrawMaterial) } },
        .extract = extract_meshes,
        .material = r3d.default_material,
        .program = r3d.mesh_program,
        .culling = GpuCullSphere,
        .ordering = GpuOrderBatched,
    });
}

ecs_entity_t sirender3d_feature(uint32_t kind) { return kind < 4 ? r3d.features[kind] : 0; }
