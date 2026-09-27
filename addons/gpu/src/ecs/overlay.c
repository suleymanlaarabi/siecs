#include "render/render_internal.h"
#include <sigpu/overlay.h>

typedef struct {
    sigpu_overlay_callback_t callback;
    void *user_data;
    sigpu_overlay_id_t id;
} overlay_slot_t;
static overlay_slot_t *slots;
static uint32_t count, capacity;
static sigpu_overlay_id_t next_id = 1;

sigpu_overlay_id_t sigpu_overlay_register(sigpu_overlay_callback_t callback, void *user_data) {
    if (count == capacity) {
        uint32_t next = capacity ? capacity * 2 : 8;
        void *memory = SDL_realloc(slots, next * sizeof(*slots));
        if (!memory)
            return 0;
        slots = memory;
        capacity = next;
    }
    sigpu_overlay_id_t id = next_id++;
    slots[count++] = (overlay_slot_t){ callback, user_data, id };
    return id;
}

void sigpu_overlay_unregister(sigpu_overlay_id_t id) {
    for (uint32_t i = 0; i < count; i++) {
        if (slots[i].id == id) {
            SDL_memmove(slots + i, slots + i + 1, (count - i - 1) * sizeof(*slots));
            count--;
            return;
        }
    }
}

void sigpu_overlay_run(void) {
    if (!count || !SIGPU_FRAMECONTEXT->swapchain)
        return;
    sigpu_overlay_context_t context = {
        .device = SIGPU_GPUCONTEXT->device,
        .command_buffer = SIGPU_FRAMECONTEXT->command_buffer,
        .target = SIGPU_FRAMECONTEXT->swapchain,
        .format = SIGPU_GPUCONTEXT->swapchain_format,
        .width = SIGPU_FRAMECONTEXT->frame_width,
        .height = SIGPU_FRAMECONTEXT->frame_height,
    };
    for (uint32_t i = 0; i < count; i++)
        slots[i].callback(&context, slots[i].user_data);
}

void sigpu_overlay_reset(void) {
    SDL_free(slots);
    slots = NULL;
    count = capacity = 0;
    next_id = 1;
}
