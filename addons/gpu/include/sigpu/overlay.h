#ifndef SIGPU_OVERLAY_H
#define SIGPU_OVERLAY_H
#include "sigpu/bake_config.h"
#include <SDL3/SDL_gpu.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    SDL_GPUDevice *device;
    SDL_GPUCommandBuffer *command_buffer;
    SDL_GPUTexture *target;
    SDL_GPUTextureFormat format;
    uint32_t width;
    uint32_t height;
} sigpu_overlay_context_t;
typedef uint32_t sigpu_overlay_id_t;
typedef void (*sigpu_overlay_callback_t)(const sigpu_overlay_context_t *context, void *user_data);
SIECS_PUBLIC_API sigpu_overlay_id_t sigpu_overlay_register(sigpu_overlay_callback_t callback, void *user_data);
SIECS_PUBLIC_API void sigpu_overlay_unregister(sigpu_overlay_id_t id);
#ifdef __cplusplus
}
#endif
#endif
