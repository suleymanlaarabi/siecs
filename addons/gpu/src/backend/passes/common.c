#include "backend/passes/passes.h"
bool sigpu_no_instances(void) {
    return SIGPU_RENDERQUEUE->shared_axis_count == 0 &&
           SIGPU_RENDERQUEUE->shared_rotated_count == 0 &&
           SIGPU_RENDERQUEUE->owned_axis_count == 0 && SIGPU_RENDERQUEUE->owned_rotated_count == 0;
}

void sigpu_bind_mesh(SDL_GPURenderPass *pass) {
    SDL_GPUBufferBinding vertex_binding = { .buffer = SIGPU_GPUCONTEXT->vertex_buffer };
    SDL_GPUBufferBinding index_binding = { .buffer = SIGPU_GPUCONTEXT->index_buffer };
    SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
    SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
}

static void bind_and_draw(
    SDL_GPURenderPass *pass,
    SDL_GPUGraphicsPipeline *pipeline,
    SDL_GPUBuffer *instance_buffer,
    Uint32 offset,
    Uint32 count,
    Uint32 mesh_id
) {
    if (count == 0) {
        return;
    }

    SDL_GPUBufferBinding instance_binding = { .buffer = instance_buffer, .offset = offset };
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    SDL_BindGPUVertexBuffers(pass, 1, &instance_binding, 1);
    const sigpu_mesh_t *mesh = &SIGPU_GPUCONTEXT->meshes[mesh_id];
    SDL_DrawGPUIndexedPrimitives(
        pass,
        mesh->index_count,
        count,
        mesh->first_index,
        mesh->vertex_offset,
        0
    );
    SIGPU_RENDERSTATS->draw_calls++;
    SIGPU_RENDERSTATS->drawn_instances += count;
}

void sigpu_draw_shared_batches(
    SDL_GPURenderPass *pass,
    SDL_GPUGraphicsPipeline *pipeline,
    SDL_GPUBuffer *buffer,
    const sigpu_shared_batch_t *batches,
    Uint32 batch_count,
    Uint32 stride,
    bool shadow
) {
    for (Uint32 index = 0; index < batch_count; index++) {
        const sigpu_shared_batch_t *batch = &batches[index];
        SDL_PushGPUVertexUniformData(
            SIGPU_FRAMECONTEXT->command_buffer,
            1,
            &batch->material,
            sizeof(batch->material)
        );
        Uint32 mesh = batch->mesh;
        if (shadow && (mesh == SIGPU_MESH_CYLINDER_HIGH || mesh == SIGPU_MESH_SPHERE_HIGH))
            mesh--;
        bind_and_draw(pass, pipeline, buffer, batch->first * stride, batch->count, mesh);
    }
}

void sigpu_draw_owned_batches(
    SDL_GPURenderPass *pass,
    SDL_GPUGraphicsPipeline *pipeline,
    SDL_GPUBuffer *buffer,
    const sigpu_owned_batch_t *batches,
    Uint32 count,
    Uint32 capacity,
    Uint32 stride,
    bool shadow
) {
    for (Uint32 i = 0; i < count; i++) {
        const sigpu_owned_batch_t *batch = &batches[i];
        Uint32 mesh = batch->mesh;
        if (shadow && (mesh == SIGPU_MESH_CYLINDER_HIGH || mesh == SIGPU_MESH_SPHERE_HIGH))
            mesh--;
        bind_and_draw(
            pass,
            pipeline,
            buffer,
            (capacity - batch->first - batch->count) * stride,
            batch->count,
            mesh
        );
    }
}
void sigpu_draw_static_chunks(
    SDL_GPURenderPass *pass,
    SDL_GPUGraphicsPipeline *axis_pipeline,
    SDL_GPUGraphicsPipeline *rotated_pipeline,
    bool shadow
) {
    for (Uint32 i = 0; i < SIGPU_STATICRENDERCACHE->static_chunk_count; i++) {
        const sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[i];
        if (!(shadow ? chunk->shadow_visible : chunk->camera_visible))
            continue;
        for (Uint32 primitive = 0; primitive < SIGPU_PRIMITIVE_COUNT; primitive++) {
            Uint32 lod = primitive == SIGPU_PRIMITIVE_CUBE ? 0
                                                           : sigpu_primitive_lod(
                                                                 chunk->primitive_center[primitive],
                                                                 chunk->primitive_radius[primitive]
                                                             );
            if (shadow && lod > 1)
                lod = 1;
            Uint32 mesh = primitive == SIGPU_PRIMITIVE_CUBE
                              ? SIGPU_MESH_CUBE
                              : (primitive == SIGPU_PRIMITIVE_CYLINDER ? SIGPU_MESH_CYLINDER_LOW
                                                                       : SIGPU_MESH_SPHERE_LOW) +
                                    lod;
            if (chunk->axis[primitive].count)
                bind_and_draw(
                    pass,
                    axis_pipeline,
                    chunk->axis_buffer[primitive],
                    0,
                    chunk->axis[primitive].count,
                    mesh
                );
            if (chunk->rotated[primitive].count)
                bind_and_draw(
                    pass,
                    rotated_pipeline,
                    chunk->rotated_buffer[primitive],
                    0,
                    chunk->rotated[primitive].count,
                    mesh
                );
        }
    }
}

void sigpu_draw_fullscreen(
    SDL_GPUGraphicsPipeline *pipeline,
    SDL_GPUTexture *target,
    Uint32 width,
    Uint32 height,
    const SDL_GPUTextureSamplerBinding *samplers,
    Uint32 sampler_count,
    const float *uniform,
    Uint32 uniform_size
) {
    SDL_GPUColorTargetInfo color_target = {
        .texture = target,
        .load_op = SDL_GPU_LOADOP_DONT_CARE,
        .store_op = SDL_GPU_STOREOP_STORE,
        .cycle = true,
    };
    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(SIGPU_FRAMECONTEXT->command_buffer, &color_target, 1, NULL);
    SDL_SetGPUViewport(
        pass,
        &(SDL_GPUViewport){
            .w = (float)width,
            .h = (float)height,
            .min_depth = 0.0f,
            .max_depth = 1.0f,
        }
    );
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    SDL_BindGPUFragmentSamplers(pass, 0, samplers, sampler_count);
    SDL_PushGPUFragmentUniformData(SIGPU_FRAMECONTEXT->command_buffer, 0, uniform, uniform_size);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}
