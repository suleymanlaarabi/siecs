#include "backend/backend.h"

#include <stddef.h>
#include <stdlib.h>

static SDL_GPUGraphicsPipeline *create_main_pipeline(bool rotated, bool shared) {
    const char *vertex_path = shared ? (rotated ? SIGPU_SHADER("primitive_rotated_shared.vert.spv")
                                                : SIGPU_SHADER("primitive_shared.vert.spv"))
                                     : (rotated ? SIGPU_SHADER("primitive_rotated.vert.spv")
                                                : SIGPU_SHADER("primitive.vert.spv"));
    SDL_GPUShader *vertex_shader =
        sigpu_shader_load(vertex_path, SDL_GPU_SHADERSTAGE_VERTEX, 0, shared ? 2 : 1);
    SDL_GPUShader *fragment_shader =
        sigpu_shader_load(SIGPU_SHADER("primitive.frag.spv"), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    SDL_GPUVertexBufferDescription buffers[2] = {
        {
            .slot = 0,
            .pitch = sizeof(sigpu_mesh_vertex_t),
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
            .offset = offsetof(sigpu_mesh_vertex_t, x),
        },
        {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM,
            .offset = offsetof(sigpu_mesh_vertex_t, nx),
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
            .sample_count = SIGPU_GPUTARGETS->sample_count,
        },
        .depth_stencil_state = {
            .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .target_info = {
            .color_target_descriptions = color_targets,
            .num_color_targets = 2,
            .depth_stencil_format = SIGPU_GPUTARGETS->depth_format,
            .has_depth_stencil_target = true,
        },
    };
    SDL_GPUGraphicsPipeline *pipeline =
        SDL_CreateGPUGraphicsPipeline(SIGPU_GPUCONTEXT->device, &info);
    SDL_ReleaseGPUShader(SIGPU_GPUCONTEXT->device, vertex_shader);
    SDL_ReleaseGPUShader(SIGPU_GPUCONTEXT->device, fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline *create_shadow_pipeline(bool rotated, bool shared) {
    const char *vertex_path = shared ? (rotated ? SIGPU_SHADER("shadow_rotated_shared.vert.spv")
                                                : SIGPU_SHADER("shadow_shared.vert.spv"))
                                     : (rotated ? SIGPU_SHADER("shadow_rotated.vert.spv")
                                                : SIGPU_SHADER("shadow.vert.spv"));
    SDL_GPUShader *vertex_shader =
        sigpu_shader_load(vertex_path, SDL_GPU_SHADERSTAGE_VERTEX, 0, shared ? 2 : 1);
    SDL_GPUShader *fragment_shader =
        sigpu_shader_load(SIGPU_SHADER("shadow.frag.spv"), SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
    SDL_GPUVertexBufferDescription buffers[2] = {
        {
            .slot = 0,
            .pitch = sizeof(sigpu_mesh_vertex_t),
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
            .offset = offsetof(sigpu_mesh_vertex_t, x),
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
    SDL_GPUGraphicsPipeline *pipeline =
        SDL_CreateGPUGraphicsPipeline(SIGPU_GPUCONTEXT->device, &info);
    SDL_ReleaseGPUShader(SIGPU_GPUCONTEXT->device, vertex_shader);
    SDL_ReleaseGPUShader(SIGPU_GPUCONTEXT->device, fragment_shader);
    return pipeline;
}

static void create_main_pipelines(void) {
    if (SIGPU_GPUPIPELINES->axis_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(SIGPU_GPUCONTEXT->device, SIGPU_GPUPIPELINES->axis_pipeline);
        SDL_ReleaseGPUGraphicsPipeline(
            SIGPU_GPUCONTEXT->device,
            SIGPU_GPUPIPELINES->rotated_pipeline
        );
        SDL_ReleaseGPUGraphicsPipeline(
            SIGPU_GPUCONTEXT->device,
            SIGPU_GPUPIPELINES->shared_axis_pipeline
        );
        SDL_ReleaseGPUGraphicsPipeline(
            SIGPU_GPUCONTEXT->device,
            SIGPU_GPUPIPELINES->shared_rotated_pipeline
        );
    }

    SIGPU_GPUPIPELINES->axis_pipeline = create_main_pipeline(false, false);
    SIGPU_GPUPIPELINES->rotated_pipeline = create_main_pipeline(true, false);
    SIGPU_GPUPIPELINES->shared_axis_pipeline = create_main_pipeline(false, true);
    SIGPU_GPUPIPELINES->shared_rotated_pipeline = create_main_pipeline(true, true);
}

static void create_shadow_resources(void) {
    SIGPU_GPUTARGETS->shadow_texture = SDL_CreateGPUTexture(
        SIGPU_GPUCONTEXT->device,
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
    SIGPU_GPUTARGETS->shadow_sampler = SDL_CreateGPUSampler(
        SIGPU_GPUCONTEXT->device,
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
    SIGPU_GPUPIPELINES->axis_shadow_pipeline = create_shadow_pipeline(false, false);
    SIGPU_GPUPIPELINES->rotated_shadow_pipeline = create_shadow_pipeline(true, false);
    SIGPU_GPUPIPELINES->shared_axis_shadow_pipeline = create_shadow_pipeline(false, true);
    SIGPU_GPUPIPELINES->shared_rotated_shadow_pipeline = create_shadow_pipeline(true, true);
}

static SDL_GPUGraphicsPipeline *create_fullscreen_pipeline(
    const char *fragment_path,
    Uint32 sampler_count,
    SDL_GPUTextureFormat format
) {
    SDL_GPUShader *vertex_shader =
        sigpu_shader_load(SIGPU_SHADER("fullscreen.vert.spv"), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    SDL_GPUShader *fragment_shader =
        sigpu_shader_load(fragment_path, SDL_GPU_SHADERSTAGE_FRAGMENT, sampler_count, 1);
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
    SDL_GPUGraphicsPipeline *pipeline =
        SDL_CreateGPUGraphicsPipeline(SIGPU_GPUCONTEXT->device, &info);
    SDL_ReleaseGPUShader(SIGPU_GPUCONTEXT->device, vertex_shader);
    SDL_ReleaseGPUShader(SIGPU_GPUCONTEXT->device, fragment_shader);
    return pipeline;
}

static void create_bloom_resources(void) {
    SIGPU_GPUTARGETS->bloom_sampler = SDL_CreateGPUSampler(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUSamplerCreateInfo){
            .min_filter = SDL_GPU_FILTER_LINEAR,
            .mag_filter = SDL_GPU_FILTER_LINEAR,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        }
    );
    SIGPU_GPUPIPELINES->bloom_down_pipeline =
        create_fullscreen_pipeline(SIGPU_SHADER("bloom_down.frag.spv"), 1, SIGPU_HDR_FORMAT);
    SIGPU_GPUPIPELINES->bloom_blur_pipeline =
        create_fullscreen_pipeline(SIGPU_SHADER("bloom_blur.frag.spv"), 1, SIGPU_HDR_FORMAT);
    SIGPU_GPUPIPELINES->bloom_composite_pipeline = create_fullscreen_pipeline(
        SIGPU_SHADER("bloom_composite.frag.spv"),
        3,
        SIGPU_GPUCONTEXT->swapchain_format
    );
}

void sigpu_pipelines_create(void) {
    create_main_pipelines();
    create_shadow_resources();
    create_bloom_resources();
}

void sigpu_main_pipelines_recreate(void) { create_main_pipelines(); }

void sigpu_pipelines_destroy(void) {
    SDL_ReleaseGPUGraphicsPipeline(SIGPU_GPUCONTEXT->device, SIGPU_GPUPIPELINES->axis_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(SIGPU_GPUCONTEXT->device, SIGPU_GPUPIPELINES->rotated_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->shared_axis_pipeline
    );
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->shared_rotated_pipeline
    );
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->axis_shadow_pipeline
    );
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->rotated_shadow_pipeline
    );
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->shared_axis_shadow_pipeline
    );
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->shared_rotated_shadow_pipeline
    );
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->bloom_down_pipeline
    );
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->bloom_blur_pipeline
    );
    SDL_ReleaseGPUGraphicsPipeline(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUPIPELINES->bloom_composite_pipeline
    );
    SDL_ReleaseGPUTexture(SIGPU_GPUCONTEXT->device, SIGPU_GPUTARGETS->shadow_texture);
    SDL_ReleaseGPUSampler(SIGPU_GPUCONTEXT->device, SIGPU_GPUTARGETS->shadow_sampler);
    SDL_ReleaseGPUSampler(SIGPU_GPUCONTEXT->device, SIGPU_GPUTARGETS->bloom_sampler);
}
