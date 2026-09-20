#version 450

layout(location = 0) in vec3 in_vertex_position;
layout(location = 1) in vec3 in_position;
layout(location = 2) in vec3 in_scale;

layout(std140, set = 1, binding = 0) uniform ShadowTransform
{
    mat4 light_view_projection;
} shadow;

layout(std140, set = 1, binding = 1) uniform Material
{
    vec4 size_bloom;
    vec4 color;
} material;

void main()
{
    vec3 world_position = in_vertex_position * in_scale * material.size_bloom.xyz + in_position;
    gl_Position = shadow.light_view_projection * vec4(world_position, 1.0);
}
