#include "backend/backend.h"
#include "sigpu_shaders.generated.h"
#include <stdlib.h>
SDL_GPUShader *sigpu_shader_load(
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
    SDL_GPUShader *shader = SDL_CreateGPUShader(SIGPU_GPUCONTEXT->device, &info);
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
