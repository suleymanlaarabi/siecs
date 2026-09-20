#version 450

layout(location = 0) in vec3 in_vertex_position;
layout(location = 1) in vec4 in_vertex_normal;
layout(location = 2) in vec3 in_position;
layout(location = 3) in vec3 in_size;
layout(location = 4) in vec4 in_color;

layout(location = 5) in float in_bloom;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) flat out vec3 out_normal;
layout(location = 2) flat out vec4 out_color;
layout(location = 3) out vec4 out_light_position;
layout(location = 4) flat out float out_bloom;

layout(std140, set = 1, binding = 0) uniform Transforms
{
    mat4 view_projection;
    mat4 light_view_projection;
} transforms;

void main()
{
    vec3 world_position = in_vertex_position * in_size + in_position;
    gl_Position = transforms.view_projection * vec4(world_position, 1.0);
    out_world_position = world_position;
    out_normal = in_vertex_normal.xyz;
    out_color = in_color;
    out_light_position = transforms.light_view_projection * vec4(world_position, 1.0);
    out_bloom = in_bloom;
}
