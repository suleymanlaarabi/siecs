// siui features: rectangles with per-side borders and text quads sampling
// the glyph atlas. Positions are logical pixels; colors are sRGB bytes,
// decoded to linear for the sRGB color target. Clipping is per instance.

[__AttributeUsage(_AttributeTargets.Var)]
struct ScopeAttribute {
    string name;
};

struct UiView {
    float width;
    float height;
    float padding0;
    float padding1;
};

struct RectInstance {
    float x, y, width, height;
    float clip_x, clip_y, clip_width, clip_height;
    float top, right, bottom, left;
    uint background;
    uint top_color, right_color, bottom_color, left_color;
};

struct TextInstance {
    float x, y, width, height;
    float clip_x, clip_y, clip_width, clip_height;
    float u0, v0, u1, v1;
    uint color;
};

[Scope("view")] ConstantBuffer<UiView> ui_view;
[Scope("instance")] StructuredBuffer<RectInstance> ui_rects;
[Scope("instance")] StructuredBuffer<TextInstance> ui_texts;
[Scope("pass")] Sampler2D atlas;

float3 decode(float3 value) {
    float3 low = value / 12.92;
    float3 high = pow((value + 0.055) / 1.055, float3(2.4));
    return select(value <= float3(0.04045), low, high);
}

float4 unpack_color(uint c) {
    float4 color = float4(float(c & 255u), float((c >> 8) & 255u), float((c >> 16) & 255u),
                          float(c >> 24)) / 255.0;
    return float4(decode(color.rgb), color.a);
}

float4 to_clip(float2 pixel) {
    return float4(pixel.x * 2.0 / ui_view.width - 1.0, 1.0 - pixel.y * 2.0 / ui_view.height, 0.0, 1.0);
}

bool clipped(float2 pixel, float4 clip) {
    return pixel.x < clip.x || pixel.y < clip.y || pixel.x >= clip.x + clip.z ||
           pixel.y >= clip.y + clip.w;
}

struct RectOutput {
    float4 position: SV_Position;
    float2 pixel: PIXEL;
    float2 local: LOCAL;
    nointerpolation uint index: INDEX;
};

[shader("vertex")]
RectOutput rect_vertex(float2 corner: CORNER, uint visible: VISIBLE) {
    RectInstance rect = ui_rects[visible];
    RectOutput output;
    output.local = corner * float2(rect.width, rect.height);
    output.pixel = float2(rect.x, rect.y) + output.local;
    output.position = to_clip(output.pixel);
    output.index = visible;
    return output;
}

[shader("fragment")]
float4 rect_fragment(RectOutput input) : SV_Target {
    RectInstance rect = ui_rects[input.index];
    if (clipped(input.pixel, float4(rect.clip_x, rect.clip_y, rect.clip_width, rect.clip_height)))
        discard;
    uint color = rect.background;
    if (input.local.y < rect.top)
        color = rect.top_color;
    else if (input.local.y >= rect.height - rect.bottom)
        color = rect.bottom_color;
    else if (input.local.x < rect.left)
        color = rect.left_color;
    else if (input.local.x >= rect.width - rect.right)
        color = rect.right_color;
    return unpack_color(color);
}

struct TextOutput {
    float4 position: SV_Position;
    float2 pixel: PIXEL;
    float2 uv: TEXCOORD0;
    nointerpolation uint index: INDEX;
};

[shader("vertex")]
TextOutput text_vertex(float2 corner: CORNER, uint visible: VISIBLE) {
    TextInstance text = ui_texts[visible];
    TextOutput output;
    output.pixel = float2(text.x, text.y) + corner * float2(text.width, text.height);
    output.position = to_clip(output.pixel);
    output.uv = lerp(float2(text.u0, text.v0), float2(text.u1, text.v1), corner);
    output.index = visible;
    return output;
}

[shader("fragment")]
float4 text_fragment(TextOutput input) : SV_Target {
    TextInstance text = ui_texts[input.index];
    if (clipped(input.pixel, float4(text.clip_x, text.clip_y, text.clip_width, text.clip_height)))
        discard;
    float4 color = unpack_color(text.color);
    return float4(color.rgb, color.a * atlas.Sample(input.uv).r);
}
