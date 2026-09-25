#include "render/render_internal.h"
void record_shared(
    sigpu_shared_batch_t **batches,
    Uint32 *count,
    Uint32 *capacity,
    Uint32 first,
    Uint32 end,
    sigpu_shared_material_t material,
    Uint32 mesh,
    Uint32 lod,
    bool rotated
) {
    if (first == end)
        return;
    if (*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 16;
        *batches = SDL_realloc(*batches, *capacity * sizeof(**batches));
    }
    (*batches)[(*count)++] =
        (sigpu_shared_batch_t){ material,       first,        end - first,
                                (uint16_t)mesh, (uint8_t)lod, (uint8_t)rotated };
}
void record_owned(
    sigpu_owned_batch_t **batches,
    Uint32 *count,
    Uint32 *capacity,
    Uint32 first,
    Uint32 end,
    Uint32 mesh
) {
    if (first == end)
        return;
    if (*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 16;
        *batches = SDL_realloc(*batches, *capacity * sizeof(**batches));
    }
    (*batches)[(*count)++] = (sigpu_owned_batch_t){ first, end - first, (uint16_t)mesh };
}
