#ifndef SIGPU_PASSES_INTERNAL_H
#define SIGPU_PASSES_INTERNAL_H
#include "backend/backend.h"
void sigpu_bind_mesh(SDL_GPURenderPass *);
void sigpu_draw_shared_batches(
    SDL_GPURenderPass *,
    SDL_GPUGraphicsPipeline *,
    SDL_GPUBuffer *,
    const sigpu_shared_batch_t *,
    Uint32,
    Uint32,
    bool
);
void sigpu_draw_owned_batches(
    SDL_GPURenderPass *,
    SDL_GPUGraphicsPipeline *,
    SDL_GPUBuffer *,
    const sigpu_owned_batch_t *,
    Uint32,
    Uint32,
    Uint32,
    bool
);
void sigpu_draw_static_chunks(
    SDL_GPURenderPass *,
    SDL_GPUGraphicsPipeline *,
    SDL_GPUGraphicsPipeline *,
    bool,
    Uint32
);
void sigpu_draw_fullscreen(
    SDL_GPUGraphicsPipeline *,
    SDL_GPUTexture *,
    Uint32,
    Uint32,
    const SDL_GPUTextureSamplerBinding *,
    Uint32,
    const float *,
    Uint32
);
#endif
