#include "rest_internal.h"

sihttp_response_t ecs_rest_get_scene(const sihttp_request_t *req) {
    (void)req;

    void *data = NULL;
    size_t size = 0;
    if (!ecs_save_memory(&data, &size))
        return ecs_rest_error_response(500, "failed to save scene");

    return ecs_rest_binary_response(data, size);
}

sihttp_response_t ecs_rest_post_scene(const sihttp_request_t *req) {
    SiecsRestState *state = ecs_try_get_resource(SiecsRestState);
    if (!req || !req->body || req->body_size == 0)
        return ecs_rest_error_response(400, "invalid scene");
    if (state && state->max_scene_bytes && req->body_size > state->max_scene_bytes)
        return ecs_rest_error_response(413, "scene too large");
    if (!ecs_scene_validate(req->body, req->body_size))
        return ecs_rest_error_response(400, "invalid scene");
    if (!ecs_load_memory(req->body, req->body_size))
        return ecs_rest_error_response(500, "failed to load scene");

    return (sihttp_response_t){ .status = 204 };
}
