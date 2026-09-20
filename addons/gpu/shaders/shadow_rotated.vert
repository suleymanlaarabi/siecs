#version 450

layout(location = 0) in vec3 in_vertex_position;
layout(location = 1) in vec3 in_position;
layout(location = 2) in vec3 in_size;
layout(location = 3) in vec4 in_rotation;

layout(std140, set = 1, binding = 0) uniform ShadowTransform
{
    mat4 light_view_projection;
} shadow;

vec3 rotate_vector(vec3 value, vec4 rotation)
{
    return value + 2.0 * cross(rotation.xyz, cross(rotation.xyz, value) + rotation.w * value);
}

void main()
{
    vec3 world_position = rotate_vector(in_vertex_position * in_size, in_rotation) + in_position;
    gl_Position = shadow.light_view_projection * vec4(world_position, 1.0);
}
