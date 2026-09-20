#include "sigpu_internal.h"

sigpu_state_t g_sigpu;

static SDL_FColor linear_color(sigpu_color_t color) {
    return (SDL_FColor){
        g_sigpu.linear_lut[color.r] / 255.0f,
        g_sigpu.linear_lut[color.g] / 255.0f,
        g_sigpu.linear_lut[color.b] / 255.0f,
        color.a / 255.0f,
    };
}

void sigpu_init(const char *title, int width, int height, int samples) {
    for (int index = 0; index < 256; index++) {
        float srgb = index / 255.0f;
        float linear = srgb <= 0.04045f ? srgb / 12.92f : powf((srgb + 0.055f) / 1.055f, 2.4f);
        g_sigpu.linear_lut[index] = (uint8_t)roundf(linear * 255.0f);
    }

    SDL_Init(SDL_INIT_VIDEO);
    g_sigpu.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    g_sigpu.window = SDL_CreateWindow(
        title,
        width,
        height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    SDL_ClaimWindowForGPUDevice(g_sigpu.device, g_sigpu.window);
    g_sigpu.linear_swapchain = SDL_WindowSupportsGPUSwapchainComposition(
        g_sigpu.device,
        g_sigpu.window,
        SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR
    );
    SDL_SetGPUSwapchainParameters(
        g_sigpu.device,
        g_sigpu.window,
        g_sigpu.linear_swapchain ? SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR
                                 : SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
        SDL_GPU_PRESENTMODE_VSYNC
    );
    g_sigpu.swapchain_format = SDL_GetGPUSwapchainTextureFormat(g_sigpu.device, g_sigpu.window);
    sigpu_resources_create(samples);

    sigpu_camera(0.0f, 2.0f, -6.0f, 0.0f, 0.0f, 0.0f, 60.0f);
    g_sigpu.light_view_projection = sigpu_mat4_identity();
}

void sigpu_camera(
    float x,
    float y,
    float z,
    float target_x,
    float target_y,
    float target_z,
    float fov
) {
    g_sigpu.camera.position = (sigpu_vec3_t){ x, y, z };
    g_sigpu.camera.target = (sigpu_vec3_t){ target_x, target_y, target_z };
    g_sigpu.camera.fov = fminf(fmaxf(fov, 1.0f), 179.0f);
    g_sigpu.camera.near_plane = 0.1f;
    g_sigpu.camera.far_plane = 1000.0f;
}

void sigpu_sky(sigpu_color_t color) {
    g_sigpu.sky_linear = linear_color(color);
    g_sigpu.sky_srgb = (SDL_FColor){
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f,
    };
}

void sigpu_sun(float dx, float dy, float dz, sigpu_color_t color, float intensity) {
    g_sigpu.sun_direction = sigpu_vec3_normalize((sigpu_vec3_t){ dx, dy, dz });
    g_sigpu.sun_color = linear_color(color);
    g_sigpu.sun_intensity = intensity;
}

void sigpu_ambient(sigpu_color_t color, float intensity) {
    g_sigpu.ambient_color = linear_color(color);
    g_sigpu.ambient_intensity = intensity;
}

void sigpu_fog(sigpu_color_t color, float start, float end) {
    g_sigpu.fog_color = linear_color(color);
    g_sigpu.fog_start = start;
    g_sigpu.fog_end = end;
    g_sigpu.fog_enabled = end > start;
}

void sigpu_shadows(bool enabled, float distance) {
    g_sigpu.shadows_enabled = enabled;
    g_sigpu.shadow_distance = distance;
}

void sigpu_bloom(bool enabled, float threshold, float intensity) {
    g_sigpu.bloom_enabled = enabled;
    g_sigpu.bloom_threshold = threshold;
    g_sigpu.bloom_intensity = intensity;
}

void sigpu_msaa(int samples) { sigpu_sample_count_set(samples); }

bool sigpu_begin_frame(void) {
    g_sigpu.shared_axis_count = 0;
    g_sigpu.shared_rotated_count = 0;
    g_sigpu.owned_axis_count = 0;
    g_sigpu.owned_rotated_count = 0;
    g_sigpu.shared_axis_batch_count = 0;
    g_sigpu.shared_rotated_batch_count = 0;
    g_sigpu.any_bloom = false;
    g_sigpu.command_buffer = SDL_AcquireGPUCommandBuffer(g_sigpu.device);
    g_sigpu.swapchain = NULL;
    SDL_WaitAndAcquireGPUSwapchainTexture(
        g_sigpu.command_buffer,
        g_sigpu.window,
        &g_sigpu.swapchain,
        &g_sigpu.frame_width,
        &g_sigpu.frame_height
    );
    g_sigpu.axis_mapped = SDL_MapGPUTransferBuffer(g_sigpu.device, g_sigpu.axis_transfer, true);
    g_sigpu.rotated_mapped =
        SDL_MapGPUTransferBuffer(g_sigpu.device, g_sigpu.rotated_transfer, true);
    return true;
}

bool sigpu_pointer_ray(float window_x, float window_y, sigpu_ray_t *out) {
    int logical_width, logical_height;
    if (!out || !g_sigpu.window || !g_sigpu.frame_width || !g_sigpu.frame_height ||
        !SDL_GetWindowSize(g_sigpu.window, &logical_width, &logical_height) || logical_width <= 0 ||
        logical_height <= 0) {
        return false;
    }

    /* Events use logical coordinates; convert through the current framebuffer
     * dimensions so DPI/resize changes use exactly the rendered aspect. */
    const float pixel_x = window_x * (float)g_sigpu.frame_width / (float)logical_width;
    const float pixel_y = window_y * (float)g_sigpu.frame_height / (float)logical_height;
    const float ndc_x = pixel_x * 2.0f / (float)g_sigpu.frame_width - 1.0f;
    const float ndc_y = 1.0f - pixel_y * 2.0f / (float)g_sigpu.frame_height;
    const sigpu_vec3_t forward =
        sigpu_vec3_normalize(sigpu_vec3_sub(g_sigpu.camera.target, g_sigpu.camera.position));
    const sigpu_vec3_t right =
        sigpu_vec3_normalize(sigpu_vec3_cross((sigpu_vec3_t){ 0.0f, 1.0f, 0.0f }, forward));
    const sigpu_vec3_t up = sigpu_vec3_cross(forward, right);
    const float tangent = tanf(g_sigpu.camera.fov * SIGPU_PI / 360.0f);
    const float aspect = (float)g_sigpu.frame_width / (float)g_sigpu.frame_height;
    const sigpu_vec3_t direction = sigpu_vec3_normalize(sigpu_vec3_add(
        forward,
        sigpu_vec3_add(
            sigpu_vec3_scale(right, ndc_x * tangent * aspect),
            sigpu_vec3_scale(up, ndc_y * tangent)
        )
    ));
    *out = (sigpu_ray_t){
        g_sigpu.camera.position.x,
        g_sigpu.camera.position.y,
        g_sigpu.camera.position.z,
        direction.x,
        direction.y,
        direction.z,
    };
    return true;
}

void sigpu_end_frame(void) {
    if (!g_sigpu.swapchain) {
        SDL_SubmitGPUCommandBuffer(g_sigpu.command_buffer);
        g_sigpu.command_buffer = NULL;
        return;
    }

    sigpu_frame_targets_prepare();
    sigpu_passes_draw();
    SDL_SubmitGPUCommandBuffer(g_sigpu.command_buffer);
    g_sigpu.command_buffer = NULL;
    g_sigpu.swapchain = NULL;
}

void sigpu_fini(void) {
    SDL_WaitForGPUIdle(g_sigpu.device);
    sigpu_resources_destroy();
    SDL_free(g_sigpu.shared_axis_batches);
    SDL_free(g_sigpu.shared_rotated_batches);
    SDL_ReleaseWindowFromGPUDevice(g_sigpu.device, g_sigpu.window);
    SDL_DestroyWindow(g_sigpu.window);
    SDL_DestroyGPUDevice(g_sigpu.device);
    SDL_Quit();
}
