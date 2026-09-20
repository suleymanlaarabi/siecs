#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D bloom_half;
layout(set = 2, binding = 2) uniform sampler2D bloom_quarter;

layout(std140, set = 3, binding = 0) uniform BloomComposite
{
    vec4 parameters;
} composite;

vec3 linear_to_srgb(vec3 value)
{
    vec3 low = value * 12.92;
    vec3 high = 1.055 * pow(max(value, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, lessThanEqual(value, vec3(0.0031308)));
}

void main()
{
    vec3 bloom = texture(bloom_half, in_uv).rgb * 0.7 + texture(bloom_quarter, in_uv).rgb * 1.1;
    vec3 color = texture(scene, in_uv).rgb + bloom * composite.parameters.x;
    if (composite.parameters.y > 0.5) {
        color = linear_to_srgb(color);
    }
    out_color = vec4(color, 1.0);
}
