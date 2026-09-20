#version 450

layout(location = 0) in vec3 in_world_position;
layout(location = 1) flat in vec3 in_normal;
layout(location = 2) flat in vec4 in_color;
layout(location = 3) in vec4 in_light_position;

layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2DShadow shadow_map;

layout(std140, set = 3, binding = 0) uniform Lighting
{
    vec4 camera_position;
    vec4 sun_direction_intensity;
    vec4 sun_color;
    vec4 ambient_color_intensity;
    vec4 fog_color;
    vec4 fog_parameters;
    vec4 shadow_parameters;
} lighting;

float shadow_factor(vec3 normal, float camera_distance)
{
    if (lighting.fog_parameters.w < 0.5) {
        return 1.0;
    }

    vec3 projected = in_light_position.xyz / in_light_position.w;
    vec2 uv = vec2(projected.x * 0.5 + 0.5, 0.5 - projected.y * 0.5);

    if (projected.z <= 0.0 || projected.z >= 1.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return 1.0;
    }

    float incidence = max(dot(normal, -lighting.sun_direction_intensity.xyz), 0.0);
    float reference = projected.z - mix(0.0008, 0.00008, incidence);
    vec2 texel = 1.0 / vec2(textureSize(shadow_map, 0));
    float shadow = 0.0;
    shadow += texture(shadow_map, vec3(uv + vec2(-0.75, -0.75) * texel, reference));
    shadow += texture(shadow_map, vec3(uv + vec2(0.75, -0.75) * texel, reference));
    shadow += texture(shadow_map, vec3(uv + vec2(-0.75, 0.75) * texel, reference));
    shadow += texture(shadow_map, vec3(uv + vec2(0.75, 0.75) * texel, reference));
    shadow *= 0.25;
    float fade = smoothstep(
        lighting.shadow_parameters.x * 0.8,
        lighting.shadow_parameters.x,
        camera_distance
    );
    return mix(shadow, 1.0, fade);
}

vec3 linear_to_srgb(vec3 value)
{
    vec3 low = value * 12.92;
    vec3 high = 1.055 * pow(max(value, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, lessThanEqual(value, vec3(0.0031308)));
}

void main()
{
    vec3 normal = normalize(in_normal);
    float camera_distance = distance(in_world_position, lighting.camera_position.xyz);
    float diffuse = max(dot(normal, -lighting.sun_direction_intensity.xyz), 0.0);
    vec3 ambient = lighting.ambient_color_intensity.rgb * lighting.ambient_color_intensity.a;
    vec3 sunlight = lighting.sun_color.rgb * lighting.sun_direction_intensity.w * diffuse * shadow_factor(normal, camera_distance);
    vec3 color = in_color.rgb * (ambient + sunlight);

    if (lighting.fog_parameters.z > 0.5) {
        float fog = smoothstep(lighting.fog_parameters.x, lighting.fog_parameters.y, camera_distance);
        color = mix(color, lighting.fog_color.rgb, fog);
    }

    out_color = vec4(linear_to_srgb(color), in_color.a);
}
