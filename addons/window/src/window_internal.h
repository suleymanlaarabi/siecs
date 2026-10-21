#ifndef SIWINDOW_INTERNAL_H
#define SIWINDOW_INTERNAL_H
#include <SDL3/SDL.h>
#include <siwindow/native.h>

#define SIWINDOW_CAPACITY 8

typedef struct {
    ecs_entity_t entity;
    SDL_Window *native;
    SDL_WindowID id;
    char slot[32];
    int32_t width;
    int32_t height;
    uint8_t mode;
    bool text_input;
} siwindow_slot_t;

extern siwindow_props_t siwindow_config;

siwindow_slot_t *siwindow_slot_by_id(SDL_WindowID id);
WindowState siwindow_read_state(const siwindow_slot_t *slot);
void siwindow_refresh_state(const siwindow_slot_t *slot);
void siwindow_emit(const siwindow_slot_t *slot, uint32_t kind, const char *text);
void siwindow_request_close(const siwindow_slot_t *slot);
void siwindow_pump_register(void);

#endif
