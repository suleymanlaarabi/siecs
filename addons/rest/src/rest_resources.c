#include "rest_internal.h"

static ecs_resource_t ecs_rest_request_resource(const sihttp_request_t *req) {
    uint16_t id;
    if (!sihttp_path_param_u16(req, "id", &id) || !id || id >= ecs_resource_count())
        return 0;
    const ecs_resource_info_t *info = ecs_resource_info((ecs_resource_t)id);
    if (!info || info->type == SIREFLECT_INVALID_HANDLE)
        return 0;
    return (ecs_resource_t)id;
}

sihttp_response_t ecs_rest_get_resources(const sihttp_request_t *req) {
    (void)req;
    sijson_clean();
    sijson_value_t resources = sijson_make_array();
    for (uint32_t id = 1; id < ecs_resource_count(); id++) {
        const ecs_resource_info_t *info = ecs_resource_info((ecs_resource_t)id);
        if (info && info->type != SIREFLECT_INVALID_HANDLE &&
            ecs_has_resource_rid((ecs_resource_t)id))
            sijson_array_push(
                resources,
                ecs_rest_resource_json((ecs_resource_t)id, ecs_resource_rid((ecs_resource_t)id))
            );
    }
    return sihttp_response_json(200, resources);
}

sihttp_response_t ecs_rest_get_resource(const sihttp_request_t *req) {
    ecs_resource_t id = ecs_rest_request_resource(req);
    if (!id || !ecs_has_resource_rid(id))
        return ecs_rest_error(404, "resource_not_found", "resource not found");
    sijson_clean();
    return sihttp_response_json(200, ecs_rest_resource_json(id, ecs_resource_rid(id)));
}

sihttp_response_t ecs_rest_put_resource(const sihttp_request_t *req) {
    ecs_resource_t id = ecs_rest_request_resource(req);
    if (!id)
        return ecs_rest_error(404, "resource_not_found", "resource not found");
    return ecs_rest_set_resource(id, req->body, false);
}

sihttp_response_t ecs_rest_patch_resource(const sihttp_request_t *req) {
    ecs_resource_t id = ecs_rest_request_resource(req);
    if (!id)
        return ecs_rest_error(404, "resource_not_found", "resource not found");
    return ecs_rest_set_resource(id, req->body, true);
}
