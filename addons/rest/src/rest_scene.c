#include "rest_internal.h"
#include <string.h>

sihttp_response_t ecs_rest_get_scene(const sihttp_request_t *req) {
    (void)req;
    sijson_clean();

    void *data = NULL;
    size_t size = 0;
    if (!ecs_save_memory(&data, &size))
        return ecs_rest_error(500, "failed_to_save_scene", "failed to save scene");

    return sihttp_response_take_binary(200, data, size);
}

sihttp_response_t ecs_rest_post_scene(const sihttp_request_t *req) {
    sijson_clean();
    if (!req || !req->body || req->body_size == 0)
        return ecs_rest_error(400, "invalid_scene", "invalid scene");
    const char *content_type = sihttp_header(req, "Content-Type");
    if (content_type && strcmp(content_type, "application/octet-stream") != 0)
        return ecs_rest_error(415, "unsupported_content_type", "unsupported content type");
    if (!ecs_scene_validate(req->body, req->body_size))
        return ecs_rest_error(400, "invalid_scene", "invalid scene");
    if (!ecs_load_memory(req->body, req->body_size))
        return ecs_rest_error(500, "failed_to_load_scene", "failed to load scene");

    return sihttp_response_empty(204);
}
