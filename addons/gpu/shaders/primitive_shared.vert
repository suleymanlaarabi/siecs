#version 450

layout(location = 0) in vec3 in_vertex_position;
layout(location = 1) in vec4 in_vertex_normal;
layout(location = 2) in vec3 in_position;
layout(location = 3) in vec3 in_scale;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) flat out vec4 out_color;
layout(location = 3) out vec4 out_light_position[3];
layout(location = 6) flat out float out_bloom;

layout(std140, set = 1, binding = 0) uniform Transforms
{
    mat4 view_projection;
    mat4 light_view_projection[3];
    vec4 shadow_texel_world;
    vec4 sun_direction;
} transforms;

layout(std140, set = 1, binding = 1) uniform Material
{
    vec4 size_bloom;
    vec4 color;
} material;

void main()
{
    vec3 size = in_scale * material.size_bloom.xyz;
    vec3 world_position = in_vertex_position * size + in_position;
    gl_Position = transforms.view_projection * vec4(world_position, 1.0);
    out_world_position = world_position;
    out_normal = normalize(in_vertex_normal.xyz / max(abs(size), vec3(0.00001)));
    out_color = material.color;
    float incidence = max(dot(out_normal, -transforms.sun_direction.xyz), 0.0);
    for (int i = 0; i < 3; i++) {
        vec3 receiver = world_position +
            out_normal * (transforms.shadow_texel_world[i] * 1.5 * (1.0 - incidence));
        out_light_position[i] = transforms.light_view_projection[i] * vec4(receiver, 1.0);
    }
    out_bloom = material.size_bloom.w;
}
