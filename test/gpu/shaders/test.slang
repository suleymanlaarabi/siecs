// Pack of the sigpu tests: a compute entry, custom pass programs
// (fullscreen fill and copy) and feature programs drawing instanced quads,
// with and without a material block.

[__AttributeUsage(_AttributeTargets.Var)]
struct ScopeAttribute {
    string name;
};

struct Params {
    uint count;
    float value;
};

[Scope("pass")] ConstantBuffer<Params> params;
[Scope("pass")] RWStructuredBuffer<float> values;

[shader("compute")]
[numthreads(32, 1, 1)]
void fill(uint3 id: SV_DispatchThreadID) {
    if (id.x < params.count)
        values[id.x] = params.value;
}

// Legacy pair kept for the resource tests.
[Scope("instance")] StructuredBuffer<float4> colors;

struct LegacyOutput {
    float4 position: SV_Position;
    nointerpolation uint instance: INSTANCE;
};

[shader("vertex")]
LegacyOutput vertex_main(float2 corner: CORNER, uint instance: INSTANCE) {
    LegacyOutput output;
    output.position = float4(corner, 0.0, 1.0);
    output.instance = instance;
    return output;
}

[shader("fragment")]
float4 fragment_main(LegacyOutput input) : SV_Target {
    return colors[input.instance];
}

// Custom passes.
struct Fill {
    float4 color;
};

[Scope("pass")] ConstantBuffer<Fill> fill_color;
[Scope("pass")] Sampler2D source;

struct FullscreenOutput {
    float4 position: SV_Position;
    float2 uv: TEXCOORD0;
};

[shader("vertex")]
FullscreenOutput fullscreen(uint id: SV_VulkanVertexID) {
    float2 uv = float2(float((id << 1) & 2), float(id & 2));
    FullscreenOutput output;
    output.uv = uv;
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 solid(FullscreenOutput input) : SV_Target {
    return fill_color.color;
}

[shader("fragment")]
float4 copy(FullscreenOutput input) : SV_Target {
    return source.Sample(input.uv);
}

// Features: quads in normalized device coordinates.
struct QuadInstance {
    float2 offset;
    float2 size;
    float4 color;
};

struct QuadMaterial {
    float4 tint;
    uint flag;
    float unused;
};

struct ViewTint {
    float4 color;
};

[Scope("instance")] StructuredBuffer<QuadInstance> quads;
[Scope("material")] StructuredBuffer<QuadMaterial> quad_materials;
[Scope("material")] Sampler2D albedo;
[Scope("view")] ConstantBuffer<ViewTint> view_tint;

struct QuadOutput {
    float4 position: SV_Position;
    nointerpolation float4 color: COLOR;
    nointerpolation uint material: MATERIAL;
};

QuadOutput quad(float2 corner, uint visible, uint material) {
    QuadInstance instance = quads[visible];
    QuadOutput output;
    output.position = float4(instance.offset + corner * instance.size, 0.5, 1.0);
    output.color = instance.color;
    output.material = material;
    return output;
}

[shader("vertex")]
QuadOutput quad_vertex(float2 corner: CORNER, uint visible: VISIBLE, uint material: MATERIAL) {
    return quad(corner, visible, material);
}

[shader("fragment")]
float4 quad_plain(QuadOutput input) : SV_Target {
    return input.color * view_tint.color;
}

[shader("fragment")]
float4 quad_material(QuadOutput input) : SV_Target {
    QuadMaterial material = quad_materials[input.material];
    float4 texel = albedo.Sample(float2(0.5, 0.5));
    float flag = material.flag != 0 ? 1.0 : 0.0;
    return float4(material.tint.rgb * texel.rgb, flag);
}

[shader("vertex")]
float4 quad_depth(float2 corner: CORNER, uint visible: VISIBLE) : SV_Position {
    QuadInstance instance = quads[visible];
    return float4(instance.offset + corner * instance.size, 0.25, 1.0);
}
