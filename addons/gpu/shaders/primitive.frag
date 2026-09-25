#version 450

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) flat in vec4 in_color;
layout(location = 3) in vec4 in_light_position[3];
layout(location = 6) flat in float in_bloom;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_bloom;

layout(set = 2, binding = 0) uniform sampler2DArrayShadow shadow_map;

layout(std140, set = 3, binding = 0) uniform Lighting
{
    vec4 camera_position;
    vec4 camera_direction;
    vec4 sun_direction_intensity;
    vec4 sun_color;
    vec4 ambient_color_intensity;
    vec4 fog_color;
    vec4 fog_parameters;
    vec4 shadow_parameters;
    vec4 shadow_splits;
    vec4 shadow_texel_depth[3];
} lighting;

float sample_shadow(int cascade, vec3 normal)
{
    vec3 projected = in_light_position[cascade].xyz / in_light_position[cascade].w;
    vec2 uv = vec2(projected.x * 0.5 + 0.5, 0.5 - projected.y * 0.5);
    if (projected.z <= 0.0 || projected.z >= 1.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return 1.0;
    }
    float incidence = max(dot(normal, -lighting.sun_direction_intensity.xyz), 0.0);
    float texel_world = lighting.shadow_texel_depth[cascade].x;
    float depth_range = lighting.shadow_texel_depth[cascade].y;
    float world_bias = texel_world * (0.35 + 1.2 * (1.0 - incidence));
    vec2 texel = 1.0 / vec2(textureSize(shadow_map, 0).xy);
    float reference = projected.z - max(world_bias / depth_range, 2.0 / 65535.0);
    float shadow = 0.0;
    const vec2 taps[4] = vec2[4](
        vec2(-0.75, -0.75), vec2(0.75, -0.75),
        vec2(-0.75, 0.75), vec2(0.75, 0.75)
    );
    for (int i = 0; i < 4; i++) {
        vec2 offset = taps[i] * texel;
        shadow += texture(shadow_map, vec4(uv + offset, float(cascade), reference));
    }
    return shadow * 0.25;
}

float shadow_factor(vec3 normal, float camera_distance)
{
    if (lighting.fog_parameters.w < 0.5 || camera_distance >= lighting.shadow_parameters.x)
        return 1.0;
    float view_depth = dot(in_world_position - lighting.camera_position.xyz, lighting.camera_direction.xyz);
    int count = int(lighting.shadow_splits.w);
    int cascade = 0;
    if (count > 1 && view_depth >= lighting.shadow_splits.x) cascade = 1;
    if (count > 2 && view_depth >= lighting.shadow_splits.y) cascade = 2;
    float shadow = sample_shadow(cascade, normal);
    if (cascade + 1 < count) {
        float split = lighting.shadow_splits[cascade];
        float blend = smoothstep(split - 10.0, split, view_depth);
        if (blend > 0.0)
            shadow = mix(shadow, sample_shadow(cascade + 1, normal), blend);
    }
    float fade = smoothstep(
        lighting.shadow_parameters.x * 0.8,
        lighting.shadow_parameters.x,
        camera_distance
    );
    return mix(shadow, 1.0, fade);
}

vec3 final_linear_color()
{
    vec3 normal = normalize(in_normal);
    float camera_distance = distance(in_world_position, lighting.camera_position.xyz);
    float diffuse = max(dot(normal, -lighting.sun_direction_intensity.xyz), 0.0);
    vec3 ambient = lighting.ambient_color_intensity.rgb * lighting.ambient_color_intensity.a;
    float visibility = diffuse > 0.0 ? shadow_factor(normal, camera_distance) : 1.0;
    vec3 sunlight = lighting.sun_color.rgb * lighting.sun_direction_intensity.w * diffuse * visibility;
    vec3 color = in_color.rgb * (ambient + sunlight);

    if (lighting.fog_parameters.z > 0.5) {
        float fog = smoothstep(lighting.fog_parameters.x, lighting.fog_parameters.y, camera_distance);
        color = mix(color, lighting.fog_color.rgb, fog);
    }

    return color;
}

void main()
{
    vec3 lit = final_linear_color();
    vec3 emissive = in_color.rgb * in_bloom;
    out_color = vec4(lit + emissive, in_color.a);
    out_bloom = vec4(emissive, 1.0);
}
