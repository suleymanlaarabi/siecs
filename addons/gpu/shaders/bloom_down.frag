#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D source;

layout(std140, set = 3, binding = 0) uniform BloomDown
{
    vec4 parameters;
} bloom;

vec3 apply_threshold(vec3 color)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return color * (max(luminance - bloom.parameters.x, 0.0) / max(luminance, 1e-4));
}

void main()
{
    vec2 texel = bloom.parameters.zw;
    vec3 color = texture(source, in_uv + vec2(-0.5, -0.5) * texel).rgb;
    color += texture(source, in_uv + vec2(0.5, -0.5) * texel).rgb;
    color += texture(source, in_uv + vec2(-0.5, 0.5) * texel).rgb;
    color += texture(source, in_uv + vec2(0.5, 0.5) * texel).rgb;
    color *= 0.25;

    if (bloom.parameters.y > 0.5) {
        color = apply_threshold(color);
    }

    out_color = vec4(color, 1.0);
}
