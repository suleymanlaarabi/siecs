#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D source;

layout(std140, set = 3, binding = 0) uniform BloomBlur
{
    vec4 parameters;
} blur;

void main()
{
    vec2 offset = blur.parameters.xy;
    vec3 color = texture(source, in_uv).rgb * 0.227027;
    color += texture(source, in_uv + offset).rgb * 0.1945946;
    color += texture(source, in_uv - offset).rgb * 0.1945946;
    color += texture(source, in_uv + offset * 2.0).rgb * 0.1216216;
    color += texture(source, in_uv - offset * 2.0).rgb * 0.1216216;
    color += texture(source, in_uv + offset * 3.0).rgb * 0.054054;
    color += texture(source, in_uv - offset * 3.0).rgb * 0.054054;
    color += texture(source, in_uv + offset * 4.0).rgb * 0.016216;
    color += texture(source, in_uv - offset * 4.0).rgb * 0.016216;
    out_color = vec4(color, 1.0);
}
