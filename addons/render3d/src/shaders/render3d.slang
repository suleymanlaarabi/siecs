// Programs of sirender3d: lit primitives and meshes (opaque phase, with a
// depth-only shadow variant), sky, bloom and composite passes.

[__AttributeUsage(_AttributeTargets.Var)]
struct ScopeAttribute {
    string name;
};

struct View3d {
    float4x4 view_projection;
    float4x4 light_view_projection0;
    float4x4 light_view_projection1;
    float4x4 light_view_projection2;
    float4 camera_position;
    float4 camera_direction;
    float4 sun_direction_intensity;
    float4 sun_color;
    float4 ambient_color_intensity;
    float4 fog_color;
    // start, end, fog enabled, shadows enabled
    float4 fog_parameters;
    // shadow distance, 1 / shadow map size
    float4 shadow_parameters;
    // far split of cascades 0..2, cascade count
    float4 shadow_splits;
    // texel world size, depth range
    float4 shadow_texel0;
    float4 shadow_texel1;
    float4 shadow_texel2;
};

struct ShadowView {
    float4x4 view_projection;
};

struct PrimitiveInstance {
    float px, py, pz;
    float sx, sy, sz;
    uint q01, q23;
    uint color;
    float bloom;
};

struct MeshInstance {
    float px, py, pz;
    float sx, sy, sz;
    uint q01, q23;
};

struct StandardMaterialBlock {
    float4 base_color;
    float emissive;
    float padding0, padding1, padding2;
};

[Scope("view")] ConstantBuffer<View3d> view3d;
[Scope("view")] ConstantBuffer<ShadowView> shadow_view;
[Scope("view")] Sampler2DArrayShadow shadow_map;
[Scope("instance")] StructuredBuffer<PrimitiveInstance> primitives;
[Scope("instance")] StructuredBuffer<MeshInstance> meshes;
[Scope("material")] StructuredBuffer<StandardMaterialBlock> standard_materials;
[Scope("material")] Sampler2D albedo;

float4 unpack_rotation(uint q01, uint q23) {
    int4 q = int4(int(q01 << 16) >> 16, int(q01) >> 16, int(q23 << 16) >> 16, int(q23) >> 16);
    return float4(q) / 32767.0;
}

float4 unpack_color(uint c) {
    return float4(float(c & 255u), float((c >> 8) & 255u), float((c >> 16) & 255u),
                  float(c >> 24)) / 255.0;
}

float3 rotate_vector(float3 value, float4 rotation) {
    return value + 2.0 * cross(rotation.xyz, cross(rotation.xyz, value) + rotation.w * value);
}

struct SceneOutput {
    float4 position: SV_Position;
    float3 world: WORLD;
    float3 normal: NORMAL;
    float2 uv: TEXCOORD0;
    nointerpolation float4 color: COLOR;
    float4 light0: LIGHT0;
    float4 light1: LIGHT1;
    float4 light2: LIGHT2;
    nointerpolation float bloom: BLOOM;
    nointerpolation uint material: MATERIAL;
};

float4 light_position(float4x4 matrix, float3 world, float3 normal, float texel, float incidence) {
    float3 receiver = world + normal * (texel * 1.5 * (1.0 - incidence));
    return mul(matrix, float4(receiver, 1.0));
}

SceneOutput scene_vertex(float3 world, float3 normal, float4 color, float bloom) {
    SceneOutput output;
    output.position = mul(view3d.view_projection, float4(world, 1.0));
    output.world = world;
    output.normal = normal;
    output.uv = float2(0.0, 0.0);
    output.color = color;
    float incidence = max(dot(normal, -view3d.sun_direction_intensity.xyz), 0.0);
    output.light0 = light_position(view3d.light_view_projection0, world, normal,
                                   view3d.shadow_texel0.x, incidence);
    output.light1 = light_position(view3d.light_view_projection1, world, normal,
                                   view3d.shadow_texel1.x, incidence);
    output.light2 = light_position(view3d.light_view_projection2, world, normal,
                                   view3d.shadow_texel2.x, incidence);
    output.bloom = bloom;
    output.material = 0;
    return output;
}

float3 primitive_world(PrimitiveInstance instance, float3 position, out float3 size,
                       out float4 rotation) {
    size = float3(instance.sx, instance.sy, instance.sz);
    rotation = unpack_rotation(instance.q01, instance.q23);
    return rotate_vector(position * size, rotation) + float3(instance.px, instance.py, instance.pz);
}

[shader("vertex")]
SceneOutput primitive_vertex(float3 position: POSITION, float4 normal: NORMAL, uint visible: VISIBLE) {
    PrimitiveInstance instance = primitives[visible];
    float3 size;
    float4 rotation;
    float3 world = primitive_world(instance, position, size, rotation);
    float3 n = rotate_vector(normalize(normal.xyz / max(abs(size), float3(0.00001))), rotation);
    return scene_vertex(world, n, unpack_color(instance.color), instance.bloom);
}

[shader("vertex")]
float4 primitive_shadow(float3 position: POSITION, uint visible: VISIBLE) : SV_Position {
    float3 size;
    float4 rotation;
    float3 world = primitive_world(primitives[visible], position, size, rotation);
    return mul(shadow_view.view_projection, float4(world, 1.0));
}

float3 mesh_world(MeshInstance instance, float3 position, out float3 size, out float4 rotation) {
    size = float3(instance.sx, instance.sy, instance.sz);
    rotation = unpack_rotation(instance.q01, instance.q23);
    return rotate_vector(position * size, rotation) + float3(instance.px, instance.py, instance.pz);
}

[shader("vertex")]
SceneOutput mesh_vertex(float3 position: POSITION, float4 normal: NORMAL, float2 uv: TEXCOORD,
                        uint visible: VISIBLE, uint material: MATERIAL) {
    MeshInstance instance = meshes[visible];
    float3 size;
    float4 rotation;
    float3 world = mesh_world(instance, position, size, rotation);
    float3 n = rotate_vector(normalize(normal.xyz / max(abs(size), float3(0.00001))), rotation);
    SceneOutput output = scene_vertex(world, n, float4(1.0), 0.0);
    output.uv = uv;
    output.material = material;
    return output;
}

[shader("vertex")]
float4 mesh_shadow(float3 position: POSITION, uint visible: VISIBLE) : SV_Position {
    float3 size;
    float4 rotation;
    float3 world = mesh_world(meshes[visible], position, size, rotation);
    return mul(shadow_view.view_projection, float4(world, 1.0));
}

float sample_shadow(SceneOutput input, int cascade, float3 normal) {
    float4 light = cascade == 0 ? input.light0 : cascade == 1 ? input.light1 : input.light2;
    float4 texel_depth = cascade == 0   ? view3d.shadow_texel0
                         : cascade == 1 ? view3d.shadow_texel1
                                        : view3d.shadow_texel2;
    float3 projected = light.xyz / light.w;
    float2 uv = float2(projected.x * 0.5 + 0.5, 0.5 - projected.y * 0.5);
    if (projected.z <= 0.0 || projected.z >= 1.0 || any(uv < float2(0.0)) || any(uv > float2(1.0)))
        return 1.0;
    float incidence = max(dot(normal, -view3d.sun_direction_intensity.xyz), 0.0);
    float world_bias = texel_depth.x * (0.35 + 1.2 * (1.0 - incidence));
    float2 texel = float2(view3d.shadow_parameters.y);
    float reference = projected.z - max(world_bias / texel_depth.y, 2.0 / 65535.0);
    float shadow = 0.0;
    shadow += shadow_map.SampleCmpLevelZero(float3(uv + float2(-0.75, -0.75) * texel, float(cascade)), reference);
    shadow += shadow_map.SampleCmpLevelZero(float3(uv + float2(0.75, -0.75) * texel, float(cascade)), reference);
    shadow += shadow_map.SampleCmpLevelZero(float3(uv + float2(-0.75, 0.75) * texel, float(cascade)), reference);
    shadow += shadow_map.SampleCmpLevelZero(float3(uv + float2(0.75, 0.75) * texel, float(cascade)), reference);
    return shadow * 0.25;
}

float shadow_factor(SceneOutput input, float3 normal, float camera_distance) {
    if (view3d.fog_parameters.w < 0.5 || camera_distance >= view3d.shadow_parameters.x)
        return 1.0;
    float view_depth = dot(input.world - view3d.camera_position.xyz, view3d.camera_direction.xyz);
    int count = int(view3d.shadow_splits.w);
    int cascade = 0;
    if (count > 1 && view_depth >= view3d.shadow_splits.x)
        cascade = 1;
    if (count > 2 && view_depth >= view3d.shadow_splits.y)
        cascade = 2;
    float shadow = sample_shadow(input, cascade, normal);
    if (cascade + 1 < count) {
        float split = cascade == 0 ? view3d.shadow_splits.x : view3d.shadow_splits.y;
        float blend = smoothstep(split - 10.0, split, view_depth);
        if (blend > 0.0)
            shadow = lerp(shadow, sample_shadow(input, cascade + 1, normal), blend);
    }
    float fade = smoothstep(view3d.shadow_parameters.x * 0.8, view3d.shadow_parameters.x,
                            camera_distance);
    return lerp(shadow, 1.0, fade);
}

float3 lit(SceneOutput input, float3 base) {
    float3 normal = normalize(input.normal);
    float camera_distance = distance(input.world, view3d.camera_position.xyz);
    float diffuse = max(dot(normal, -view3d.sun_direction_intensity.xyz), 0.0);
    float3 ambient = view3d.ambient_color_intensity.rgb * view3d.ambient_color_intensity.a;
    float visibility = diffuse > 0.0 ? shadow_factor(input, normal, camera_distance) : 1.0;
    float3 sunlight = view3d.sun_color.rgb * view3d.sun_direction_intensity.w * diffuse * visibility;
    float3 color = base * (ambient + sunlight);
    if (view3d.fog_parameters.z > 0.5) {
        float fog = smoothstep(view3d.fog_parameters.x, view3d.fog_parameters.y, camera_distance);
        color = lerp(color, view3d.fog_color.rgb, fog);
    }
    return color;
}

struct SceneTargets {
    float4 color: SV_Target0;
    float4 bloom: SV_Target1;
};

[shader("fragment")]
SceneTargets primitive_fragment(SceneOutput input) {
    float3 emissive = input.color.rgb * input.bloom;
    SceneTargets targets;
    targets.color = float4(lit(input, input.color.rgb) + emissive, input.color.a);
    targets.bloom = float4(emissive, 1.0);
    return targets;
}

[shader("fragment")]
SceneTargets mesh_fragment(SceneOutput input) {
    StandardMaterialBlock material = standard_materials[input.material];
    float4 base = material.base_color * albedo.Sample(input.uv);
    float3 emissive = base.rgb * material.emissive;
    SceneTargets targets;
    targets.color = float4(lit(input, base.rgb) + emissive, base.a);
    targets.bloom = float4(emissive, 1.0);
    return targets;
}

// Fullscreen passes.

struct SkyParams {
    float4 color;
};

struct BloomDown {
    // threshold, apply threshold, texel x, texel y
    float4 parameters;
};

struct BloomBlur {
    // offset x, offset y
    float4 parameters;
};

struct Composite {
    // bloom intensity
    float4 parameters;
};

[Scope("pass")] ConstantBuffer<SkyParams> sky;
[Scope("pass")] ConstantBuffer<BloomDown> bloom_down;
[Scope("pass")] ConstantBuffer<BloomBlur> bloom_blur;
[Scope("pass")] ConstantBuffer<Composite> composite;
[Scope("pass")] Sampler2D source;
[Scope("pass")] Sampler2D scene;
[Scope("pass")] Sampler2D bloom_half;
[Scope("pass")] Sampler2D bloom_quarter;

struct FullscreenOutput {
    float4 position: SV_Position;
    float2 uv: TEXCOORD0;
};

[shader("vertex")]
FullscreenOutput fullscreen(uint id: SV_VulkanVertexID) {
    float2 uv = float2(float((id << 1) & 2), float(id & 2));
    FullscreenOutput output;
    output.uv = uv;
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
    return output;
}

struct SkyTargets {
    float4 color: SV_Target0;
    float4 bloom: SV_Target1;
};

[shader("fragment")]
SkyTargets sky_fragment(FullscreenOutput input) {
    SkyTargets targets;
    targets.color = sky.color;
    targets.bloom = float4(0.0, 0.0, 0.0, 1.0);
    return targets;
}

[shader("fragment")]
float4 bloom_down_fragment(FullscreenOutput input) : SV_Target {
    float2 texel = bloom_down.parameters.zw;
    float3 color = source.Sample(input.uv + float2(-0.5, -0.5) * texel).rgb;
    color += source.Sample(input.uv + float2(0.5, -0.5) * texel).rgb;
    color += source.Sample(input.uv + float2(-0.5, 0.5) * texel).rgb;
    color += source.Sample(input.uv + float2(0.5, 0.5) * texel).rgb;
    color *= 0.25;
    if (bloom_down.parameters.y > 0.5) {
        float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
        color *= max(luminance - bloom_down.parameters.x, 0.0) / max(luminance, 1e-4);
    }
    return float4(color, 1.0);
}

[shader("fragment")]
float4 bloom_blur_fragment(FullscreenOutput input) : SV_Target {
    float2 offset = bloom_blur.parameters.xy;
    float3 color = source.Sample(input.uv).rgb * 0.227027;
    color += source.Sample(input.uv + offset).rgb * 0.1945946;
    color += source.Sample(input.uv - offset).rgb * 0.1945946;
    color += source.Sample(input.uv + offset * 2.0).rgb * 0.1216216;
    color += source.Sample(input.uv - offset * 2.0).rgb * 0.1216216;
    color += source.Sample(input.uv + offset * 3.0).rgb * 0.054054;
    color += source.Sample(input.uv - offset * 3.0).rgb * 0.054054;
    color += source.Sample(input.uv + offset * 4.0).rgb * 0.016216;
    color += source.Sample(input.uv - offset * 4.0).rgb * 0.016216;
    return float4(color, 1.0);
}

[shader("fragment")]
float4 composite_fragment(FullscreenOutput input) : SV_Target {
    float3 bloom = bloom_half.Sample(input.uv).rgb * 0.7 + bloom_quarter.Sample(input.uv).rgb * 1.1;
    return float4(scene.Sample(input.uv).rgb + bloom * composite.parameters.x, 1.0);
}

[shader("fragment")]
float4 composite_plain(FullscreenOutput input) : SV_Target {
    return float4(scene.Sample(input.uv).rgb, 1.0);
}

// Depth-only passes still bind a fragment shader.
[shader("fragment")]
void depth_only() {}
