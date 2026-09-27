#include "rest_internal.h"
#include <string.h>

sihttp_response_t ecs_rest_get_scene(const sihttp_request_t *req) {
    (void)req;
    sijson_clean();

    void *data = NULL;
    size_t size = 0;
    if (!ecs_save_memory(&data, &size))
        return sihttp_response_json_error(500, "failed to save scene");

    return sihttp_response_take_binary(200, data, size);
}

sihttp_response_t ecs_rest_post_scene(const sihttp_request_t *req) {
    sijson_clean();
    if (!req || !req->body || req->body_size == 0)
        return sihttp_response_json_error(400, "invalid scene");
    const char *content_type = sihttp_header(req, "Content-Type");
    if (content_type && strcmp(content_type, "application/octet-stream") != 0)
        return sihttp_response_json_error(415, "unsupported content type");
    if (!ecs_scene_validate(req->body, req->body_size))
        return sihttp_response_json_error(400, "invalid scene");
    if (!ecs_load_memory(req->body, req->body_size))
        return sihttp_response_json_error(500, "failed to load scene");

    return sihttp_response_empty(204);
}
