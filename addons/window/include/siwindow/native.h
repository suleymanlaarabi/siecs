#ifndef SIECS_SIWINDOW_NATIVE_H
#define SIECS_SIWINDOW_NATIVE_H
#include <siwindow.h>
#ifdef __cplusplus
extern "C" {
#endif
/*
 * For renderers: the native SDL_Window* of a window entity, or NULL. The window
 * lives until its Window component is removed or the world ends; a renderer
 * releases it from its own ecs_at_fini callback, which runs first, or from a
 * destroy callback when the Window goes away earlier.
 */
SIECS_PUBLIC_API void *siwindow_native(ecs_entity_t window);

/* Called right before siwindow destroys a native window. */
typedef void (*siwindow_destroy_fn)(ecs_entity_t window, void *native);
SIECS_PUBLIC_API void siwindow_on_destroy(siwindow_destroy_fn callback);
#ifdef __cplusplus
}
#endif
#endif
