#include "sigpu_internal.h"
#include "sigpu_shaders.generated.h"

#include <stddef.h>
#include <stdlib.h>

static SDL_GPUShader *load_shader(
    const char *path,
    SDL_GPUShaderStage stage,
    Uint32 sampler_count,
    Uint32 uniform_count
) {
    const sigpu_embedded_shader_t *embedded = sigpu_embedded_shader_find(path);
    if (!embedded) {
        SDL_LogCritical(
            SDL_LOG_CATEGORY_APPLICATION,
            "sigpu: embedded shader '%s' is missing; run tools/embed_shaders.sh",
            path
        );
        abort();
    }
    SDL_GPUShaderCreateInfo info = { .code_size = embedded->size,
                                     .code = embedded->code,
                                     .entrypoint = "main",
                                     .format = SDL_GPU_SHADERFORMAT_SPIRV,
                                     .stage = stage,
                                     .num_samplers = sampler_count,
                                     .num_uniform_buffers = uniform_count };
    SDL_GPUShader *shader = SDL_CreateGPUShader(g_sigpu.device, &info);
    if (!shader) {
        SDL_LogCritical(
            SDL_LOG_CATEGORY_APPLICATION,
            "sigpu: unable to create shader '%s': %s",
            path,
            SDL_GetError()
        );
        abort();
    }
    return shader;
}

static SDL_GPUGraphicsPipeline *create_main_pipeline(bool rotated, bool shared) {
    const char *vertex_path =
        shared ? (rotated ? SIGPU_SHADER("cube_rotated_shared.vert.spv")
                          : SIGPU_SHADER("cube_shared.vert.spv"))
               : (rotated ? SIGPU_SHADER("cube_rotated.vert.spv") : SIGPU_SHADER("cube.vert.spv"));
    SDL_GPUShader *vertex_shader =
        load_shader(vertex_path, SDL_GPU_SHADERSTAGE_VERTEX, 0, shared ? 2 : 1);
    SDL_GPUShader *fragment_shader =
        load_shader(SIGPU_SHADER("cube.frag.spv"), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    SDL_GPUVertexBufferDescription buffers[2] = {
        {
            .slot = 0,
            .pitch = sizeof(sigpu_cube_vertex_t),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        },
        {
            .slot = 1,
            .pitch = shared ? (rotated ? sizeof(sigpu_shared_rotated_instance_t)
                                       : sizeof(sigpu_shared_axis_instance_t))
                            : (rotated ? sizeof(sigpu_rotated_instance_t)
                                       : sizeof(sigpu_axis_instance_t)),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
        },
    };
    SDL_GPUVertexAttribute attributes[7] = {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(sigpu_cube_vertex_t, x),
        },
        {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM,
            .offset = offsetof(sigpu_cube_vertex_t, nx),
        },
        {
            .location = 2,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = rotated ? offsetof(sigpu_rotated_instance_t, x)
                              : offsetof(sigpu_axis_instance_t, x),
        },
        {
            .location = 3,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = rotated ? offsetof(sigpu_rotated_instance_t, width)
                              : offsetof(sigpu_axis_instance_t, width),
        },
        {
            .location = 4,
            .buffer_slot = 1,
            .format = shared ? SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM
                             : SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
            .offset = shared ? offsetof(sigpu_shared_rotated_instance_t, qx)
                             : (rotated ? offsetof(sigpu_rotated_instance_t, r)
                                        : offsetof(sigpu_axis_instance_t, r)),
        },
        {
            .location = 5,
            .buffer_slot = 1,
            .format = rotated ? SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM
                              : SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
            .offset = rotated ? offsetof(sigpu_rotated_instance_t, qx)
                              : offsetof(sigpu_axis_instance_t, bloom),
        },
        {
            .location = 6,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
            .offset = offsetof(sigpu_rotated_instance_t, bloom),
        },
    };
    SDL_GPUColorTargetDescription color_targets[2] = {
        { .format = SIGPU_HDR_FORMAT },
        { .format = SIGPU_HDR_FORMAT },
    };
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = {
            .vertex_buffer_descriptions = buffers,
            .num_vertex_buffers = 2,
            .vertex_attributes = attributes,
            .num_vertex_attributes = shared ? (rotated ? 5 : 4) : (rotated ? 7 : 6),
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .enable_depth_clip = true,
        },
        .multisample_state = {
            .sample_count = g_sigpu.sample_count,
        },
        .depth_stencil_state = {
            .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .target_info = {
            .color_target_descriptions = color_targets,
            .num_color_targets = 2,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .has_depth_stencil_target = true,
        },
    };
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(g_sigpu.device, &info);
    SDL_ReleaseGPUShader(g_sigpu.device, vertex_shader);
    SDL_ReleaseGPUShader(g_sigpu.device, fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline *create_shadow_pipeline(bool rotated, bool shared) {
    const char *vertex_path = shared ? (rotated ? SIGPU_SHADER("shadow_rotated_shared.vert.spv")
                                                : SIGPU_SHADER("shadow_shared.vert.spv"))
                                     : (rotated ? SIGPU_SHADER("shadow_rotated.vert.spv")
                                                : SIGPU_SHADER("shadow.vert.spv"));
    SDL_GPUShader *vertex_shader =
        load_shader(vertex_path, SDL_GPU_SHADERSTAGE_VERTEX, 0, shared ? 2 : 1);
    SDL_GPUShader *fragment_shader =
        load_shader(SIGPU_SHADER("shadow.frag.spv"), SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
    SDL_GPUVertexBufferDescription buffers[2] = {
        {
            .slot = 0,
            .pitch = sizeof(sigpu_cube_vertex_t),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        },
        {
            .slot = 1,
            .pitch = shared ? (rotated ? sizeof(sigpu_shared_rotated_instance_t)
                                       : sizeof(sigpu_shared_axis_instance_t))
                            : (rotated ? sizeof(sigpu_rotated_instance_t)
                                       : sizeof(sigpu_axis_instance_t)),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
        }
    };
    SDL_GPUVertexAttribute attributes[4] = {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(sigpu_cube_vertex_t, x),
        },
        {
            .location = 1,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = rotated ? offsetof(sigpu_rotated_instance_t, x)
                              : offsetof(sigpu_axis_instance_t, x),
        },
        {
            .location = 2,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = rotated ? offsetof(sigpu_rotated_instance_t, width)
                              : offsetof(sigpu_axis_instance_t, width),
        },
        {
            .location = 3,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM,
            .offset = offsetof(sigpu_rotated_instance_t, qx),
        }
    };
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = {
            .vertex_buffer_descriptions = buffers,
            .num_vertex_buffers = 2,
            .vertex_attributes = attributes,
            .num_vertex_attributes = rotated ? 4 : 3,
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .depth_bias_constant_factor = 1.25f,
            .depth_bias_slope_factor = 1.75f,
            .enable_depth_bias = true,
            .enable_depth_clip = true,
        },
        .multisample_state = {
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
        },
        .depth_stencil_state = {
            .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .target_info = {
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .has_depth_stencil_target = true,
        },
    };
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(g_sigpu.device, &info);
    SDL_ReleaseGPUShader(g_sigpu.device, vertex_shader);
    SDL_ReleaseGPUShader(g_sigpu.device, fragment_shader);
    return pipeline;
}

static void create_main_pipelines(void) {
    if (g_sigpu.axis_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.axis_pipeline);
        SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.rotated_pipeline);
        SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.shared_axis_pipeline);
        SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.shared_rotated_pipeline);
    }

    g_sigpu.axis_pipeline = create_main_pipeline(false, false);
    g_sigpu.rotated_pipeline = create_main_pipeline(true, false);
    g_sigpu.shared_axis_pipeline = create_main_pipeline(false, true);
    g_sigpu.shared_rotated_pipeline = create_main_pipeline(true, true);
}

static void create_shadow_resources(void) {
    g_sigpu.shadow_texture = SDL_CreateGPUTexture(
        g_sigpu.device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width = SIGPU_SHADOW_SIZE,
            .height = SIGPU_SHADOW_SIZE,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
        }
    );
    g_sigpu.shadow_sampler = SDL_CreateGPUSampler(
        g_sigpu.device,
        &(SDL_GPUSamplerCreateInfo){
            .min_filter = SDL_GPU_FILTER_LINEAR,
            .mag_filter = SDL_GPU_FILTER_LINEAR,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
            .enable_compare = true,
        }
    );
    g_sigpu.axis_shadow_pipeline = create_shadow_pipeline(false, false);
    g_sigpu.rotated_shadow_pipeline = create_shadow_pipeline(true, false);
    g_sigpu.shared_axis_shadow_pipeline = create_shadow_pipeline(false, true);
    g_sigpu.shared_rotated_shadow_pipeline = create_shadow_pipeline(true, true);
}

static SDL_GPUGraphicsPipeline *create_fullscreen_pipeline(
    const char *fragment_path,
    Uint32 sampler_count,
    SDL_GPUTextureFormat format
) {
    SDL_GPUShader *vertex_shader =
        load_shader(SIGPU_SHADER("fullscreen.vert.spv"), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    SDL_GPUShader *fragment_shader =
        load_shader(fragment_path, SDL_GPU_SHADERSTAGE_FRAGMENT, sampler_count, 1);
    SDL_GPUColorTargetDescription color_target = { .format = format };
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        },
        .target_info = {
            .color_target_descriptions = &color_target,
            .num_color_targets = 1,
        },
    };
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(g_sigpu.device, &info);
    SDL_ReleaseGPUShader(g_sigpu.device, vertex_shader);
    SDL_ReleaseGPUShader(g_sigpu.device, fragment_shader);
    return pipeline;
}

static void create_bloom_resources(void) {
    g_sigpu.bloom_sampler = SDL_CreateGPUSampler(
        g_sigpu.device,
        &(SDL_GPUSamplerCreateInfo){
            .min_filter = SDL_GPU_FILTER_LINEAR,
            .mag_filter = SDL_GPU_FILTER_LINEAR,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        }
    );
    g_sigpu.bloom_down_pipeline =
        create_fullscreen_pipeline(SIGPU_SHADER("bloom_down.frag.spv"), 1, SIGPU_HDR_FORMAT);
    g_sigpu.bloom_blur_pipeline =
        create_fullscreen_pipeline(SIGPU_SHADER("bloom_blur.frag.spv"), 1, SIGPU_HDR_FORMAT);
    g_sigpu.bloom_composite_pipeline = create_fullscreen_pipeline(
        SIGPU_SHADER("bloom_composite.frag.spv"),
        3,
        g_sigpu.swapchain_format
    );
}

void sigpu_pipelines_create(void) {
    create_main_pipelines();
    create_shadow_resources();
    create_bloom_resources();
}

void sigpu_main_pipelines_recreate(void) { create_main_pipelines(); }

void sigpu_pipelines_destroy(void) {
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.axis_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.rotated_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.shared_axis_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.shared_rotated_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.axis_shadow_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.rotated_shadow_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.shared_axis_shadow_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.shared_rotated_shadow_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.bloom_down_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.bloom_blur_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(g_sigpu.device, g_sigpu.bloom_composite_pipeline);
    SDL_ReleaseGPUTexture(g_sigpu.device, g_sigpu.shadow_texture);
    SDL_ReleaseGPUSampler(g_sigpu.device, g_sigpu.shadow_sampler);
    SDL_ReleaseGPUSampler(g_sigpu.device, g_sigpu.bloom_sampler);
}
