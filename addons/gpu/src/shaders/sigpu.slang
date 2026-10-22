// Internal programs of sigpu: the present pass samples a surface's color
// target into its swapchain, encoding to sRGB when the swapchain does not.

[__AttributeUsage(_AttributeTargets.Var)]
struct ScopeAttribute {
    string name;
};

struct PresentParams {
    uint encode;
    uint padding0;
    uint padding1;
    uint padding2;
};

[Scope("pass")] ConstantBuffer<PresentParams> present_params;
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

float3 encode_srgb(float3 value) {
    float3 low = value * 12.92;
    float3 high = 1.055 * pow(max(value, float3(0.0)), float3(1.0 / 2.4)) - 0.055;
    return select(value <= float3(0.0031308), low, high);
}

[shader("fragment")]
float4 present(FullscreenOutput input) : SV_Target {
    float3 color = source.Sample(input.uv).rgb;
    if (present_params.encode != 0)
        color = encode_srgb(color);
    return float4(color, 1.0);
}
