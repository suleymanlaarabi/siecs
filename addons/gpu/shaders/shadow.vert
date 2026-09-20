#version 450

layout(location = 0) in vec3 in_vertex_position;
layout(location = 1) in vec3 in_position;
layout(location = 2) in vec3 in_size;

layout(std140, set = 1, binding = 0) uniform ShadowTransform
{
    mat4 light_view_projection;
} shadow;

void main()
{
    vec3 world_position = in_vertex_position * in_size + in_position;
    gl_Position = shadow.light_view_projection * vec4(world_position, 1.0);
}
