#include "util.h"

static ecs_entity_t cube(float x, float y, float z, float size, Color color) {
    ecs_entity_t e = ecs_new();
    ecs_set(e, Position3d, { x, y, z });
    ecs_set(e, Cuboid, { size, size, size });
    ecs_set_cid(e, ecs_id(Color), &color);
    return e;
}

void render_primitives_drawn(void) {
    r3d_test_t t = r3d_test_start(64, 48, 1);
    cube(0, 0, 0, 2, (Color){ 255, 0, 0, 255 });
    ecs_entity_t sphere = ecs_new();
    ecs_set(sphere, Position3d, { -20, 0, 0 });
    ecs_set(sphere, Sphere, { 1 });
    ecs_entity_t cylinder = ecs_new();
    ecs_set(cylinder, Position3d, { 20, 0, 0 });
    ecs_set(cylinder, Cylinder, { 1, 2 });
    r3d_test_frames(4);
    const GpuSurfaceTargets *targets = ecs_get(t.surface, GpuSurfaceTargets);
    test_uint(GpuReady, ecs_get(targets->graph, GpuGraphState)->status);
    const uint32_t center = r3d_test_pixel(&t, 0.5f, 0.5f);
    const uint32_t corner = r3d_test_pixel(&t, 0.02f, 0.02f);
    /* Red lit cube in the middle, blue sky in the corner. */
    test_assert(r3d_channel(center, 0) > 30);
    test_assert(r3d_channel(center, 2) < 20);
    test_assert(r3d_channel(corner, 2) > 200);
    test_assert(r3d_channel(corner, 0) < 20);
    /* The sphere and cylinder are outside the view: culled. */
    test_uint(3, r3d_test_stats()->candidates);
    test_uint(1, r3d_test_stats()->visible);
    r3d_test_stop();
}

void render_camera_order_split_screen(void) {
    r3d_test_t t = r3d_test_start(64, 48, 1);
    Camera3d left = *ecs_get(t.camera, Camera3d);
    left.viewport[2] = 0.5f;
    left.viewport[3] = 1.0f;
    ecs_set_cid(t.camera, ecs_id(Camera3d), &left);
    ecs_entity_t right = ecs_new();
    ecs_set(right, Position3d, { 0, 0, 10 });
    ecs_set(right, Camera3d, { .fov = 60, .near_plane = 0.1f, .far_plane = 100,
                               .viewport = { 0.5f, 0, 0.5f, 1 }, .order = 1 });
    ecs_set(right, Sky, { { 0, 255, 0, 255 } });
    ecs_set(right, Multisampling, { 1 });
    ecs_set(right, BloomSettings, { .enabled = false });
    ecs_relate(right, RendersTo, t.surface);
    r3d_test_frames(4);
    const uint32_t a = r3d_test_pixel(&t, 0.05f, 0.5f);
    const uint32_t b = r3d_test_pixel(&t, 0.95f, 0.5f);
    test_assert(r3d_channel(a, 2) > 200 && r3d_channel(a, 1) < 30);
    test_assert(r3d_channel(b, 1) > 200 && r3d_channel(b, 2) < 30);
    r3d_test_stop();
}

void render_msaa_resolves(void) {
    r3d_test_t t = r3d_test_start(64, 48, 4);
    cube(0, 0, 0, 2, (Color){ 255, 0, 0, 255 });
    r3d_test_frames(4);
    const GpuSurfaceTargets *targets = ecs_get(t.surface, GpuSurfaceTargets);
    test_uint(GpuReady, ecs_get(targets->graph, GpuGraphState)->status);
    test_assert(r3d_channel(r3d_test_pixel(&t, 0.5f, 0.5f), 0) > 30);
    r3d_test_stop();
}

void render_bloom_glows(void) {
    r3d_test_t t = r3d_test_start(64, 48, 1);
    ecs_set(t.camera, BloomSettings, { .enabled = true, .threshold = 0, .intensity = 1 });
    ecs_set(t.camera, Sky, { { 0, 0, 0, 255 } });
    ecs_entity_t glow = cube(0, 0, 0, 1, (Color){ 255, 255, 255, 255 });
    ecs_set(glow, Bloom, { 4.0f });
    r3d_test_frames(4);
    const GpuSurfaceTargets *targets = ecs_get(t.surface, GpuSurfaceTargets);
    test_uint(GpuReady, ecs_get(targets->graph, GpuGraphState)->status);
    /* Next to the cube, the sky is lit by the bloom. */
    const uint32_t beside = r3d_test_pixel(&t, 0.5f + 0.15f, 0.5f);
    test_assert(r3d_channel(beside, 0) > 5);
    ecs_set(t.camera, BloomSettings, { .enabled = false });
    r3d_test_frames(3);
    test_uint(0, r3d_channel(r3d_test_pixel(&t, 0.5f + 0.15f, 0.5f), 0));
    r3d_test_stop();
}

void render_shadows_darken(void) {
    r3d_test_t t = r3d_test_start(96, 96, 1);
    /* Looking down at 45 degrees on a floor; the sun is overhead and a
     * blocker above the floor's center casts its shadow there. */
    ecs_set(t.camera, Position3d, { 0, 10, 10 });
    ecs_set(t.camera, Rotation3d, { -0.785f, 0, 0 });
    ecs_set(t.camera, Shadows, { true, 50 });
    ecs_set(t.camera, AmbientLight, { { 255, 255, 255, 255 }, 0.1f });
    ecs_entity_t floor = cube(0, -1, 0, 1, (Color){ 220, 220, 220, 255 });
    ecs_set(floor, Cuboid, { 30, 0.2f, 30 });
    cube(0, 3, 0, 3, (Color){ 220, 220, 220, 255 });
    r3d_test_frames(5);
    const GpuSurfaceTargets *targets = ecs_get(t.surface, GpuSurfaceTargets);
    test_uint(GpuReady, ecs_get(targets->graph, GpuGraphState)->status);
    const uint32_t shadowed = r3d_test_pixel(&t, 0.5f, 0.52f);
    const uint32_t lit = r3d_test_pixel(&t, 0.9f, 0.52f);
    test_assert(r3d_channel(lit, 0) > 100);
    test_assert(r3d_channel(shadowed, 0) * 2 < r3d_channel(lit, 0));
    /* Without shadows the same point is lit. */
    ecs_set(t.camera, Shadows, { false, 50 });
    r3d_test_frames(3);
    test_assert(r3d_channel(r3d_test_pixel(&t, 0.5f, 0.52f), 0) > 100);
    r3d_test_stop();
}

void render_shadow_cascades_cached(void) {
    r3d_test_t t = r3d_test_start(64, 48, 1);
    ecs_set(t.camera, Shadows, { true, 50 });
    cube(0, 0, 0, 2, (Color){ 255, 0, 0, 255 });
    r3d_test_frames(6);
    /* Nothing moves: the three cascades keep last frame's depth. */
    test_uint(3, r3d_test_stats()->passes_reused);
    /* The camera moves: the cascade in use follows it, the two unused ones
     * (shadow distance 50) stay. */
    ecs_set(t.camera, Position3d, { 5, 0, 10 });
    r3d_test_frames(1);
    test_uint(2, r3d_test_stats()->passes_reused);
    r3d_test_frames(1);
    test_uint(3, r3d_test_stats()->passes_reused);
    r3d_test_stop();
}

void render_graph_regenerated_on_settings(void) {
    r3d_test_t t = r3d_test_start(64, 48, 1);
    r3d_test_frames(4);
    const GpuSurfaceTargets *targets = ecs_get(t.surface, GpuSurfaceTargets);
    const uint32_t revision = ecs_get(targets->graph, GpuGraphState)->revision;
    r3d_test_frames(3);
    test_uint(revision, ecs_get(targets->graph, GpuGraphState)->revision);
    ecs_set(t.camera, BloomSettings, { .enabled = true, .threshold = 0, .intensity = 1 });
    r3d_test_frames(3);
    test_assert(ecs_get(targets->graph, GpuGraphState)->revision > revision);
    test_uint(GpuReady, ecs_get(targets->graph, GpuGraphState)->status);
    r3d_test_stop();
}

void render_still_scene_invariants(void) {
    r3d_test_start(64, 48, 4);
    for (int i = 0; i < 100; i++) {
        ecs_entity_t e = cube((float)(i % 10) - 5, (float)(i / 10) - 5, -5, 0.5f,
                              (Color){ 255, 0, 0, 255 });
        ecs_add(e, Static);
    }
    r3d_test_frames(12);
    for (int i = 0; i < 4; i++) {
        r3d_test_frames(1);
        const GpuStats *stats = r3d_test_stats();
        test_uint(0, stats->instance_uploads);
        test_uint(0, stats->graph_compiles);
        test_uint(0, stats->pipelines_created);
        test_uint(0, stats->created);
        test_uint(0, stats->uploads);
        test_uint(0, stats->tables_extracted);
    }
    r3d_test_stop();
}

void render_mesh_with_material(void) {
    r3d_test_t t = r3d_test_start(64, 48, 1);
    typedef struct {
        float position[3];
        int8_t normal[4];
        float uv[2];
    } vertex_t;
    const vertex_t quad[6] = {
        { { -2, -2, 0 }, { 0, 0, 127, 0 }, { 0, 0 } }, { { -2, 2, 0 }, { 0, 0, 127, 0 }, { 0, 1 } },
        { { 2, -2, 0 }, { 0, 0, 127, 0 }, { 1, 0 } },  { { 2, -2, 0 }, { 0, 0, 127, 0 }, { 1, 0 } },
        { { -2, 2, 0 }, { 0, 0, 127, 0 }, { 0, 1 } },  { { 2, 2, 0 }, { 0, 0, 127, 0 }, { 1, 1 } },
    };
    ecs_entity_t vertices = ecs_new();
    ecs_set(vertices, GpuBuffer, { .size = sizeof(quad), .usage = GpuBufferVertex });
    sigpu_buffer_upload(vertices, 0, quad, sizeof(quad));
    ecs_entity_t geometry = ecs_new();
    ecs_set(geometry, GpuGeometry, {
        .vertices = vertices, .stride = sizeof(vertex_t), .lod_count = 1, .attribute_count = 3,
        .attributes = { { .name = "position", .format = GpuVertexFloat3, .offset = 0 },
                        { .name = "normal", .format = GpuVertexByte4Norm, .offset = 12 },
                        { .name = "uv", .format = GpuVertexFloat2, .offset = 16 } },
        .lods = { { .index_count = 6 } }, .bounds = { 0, 0, 0, 3 } });
    ecs_entity_t material = ecs_new();
    ecs_set(material, StandardMaterial, { .base_color = { 0, 255, 0, 255 } });
    ecs_entity_t mesh = ecs_new();
    ecs_set(mesh, Position3d, { 0, 0, 0 });
    ecs_relate(mesh, DrawGeometry, geometry);
    ecs_relate(mesh, DrawMaterial, material);
    ecs_set(t.camera, AmbientLight, { { 255, 255, 255, 255 }, 1.0f });
    r3d_test_frames(5);
    test_uint(GpuReady, ecs_get(material, GpuResourceState)->status);
    const uint32_t center = r3d_test_pixel(&t, 0.5f, 0.5f);
    test_assert(r3d_channel(center, 1) > 100);
    test_assert(r3d_channel(center, 0) < 30);
    r3d_test_stop();
}
