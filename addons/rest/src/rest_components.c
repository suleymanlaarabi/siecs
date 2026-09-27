#include "rest_internal.h"

static bool
rest_request_reflected_component(const sihttp_request_t *req, ecs_component_t *component) {
    return ecs_rest_request_component(req, component) &&
           ecs_rest_entity_component_is_reflected(*component);
}

sihttp_response_t ecs_rest_post_entity_component(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = ecs_rest_request_entity(req);
    if (!entity) {
        return sihttp_response_json_error(404, "entity not found");
    }

    ecs_component_t component = 0;
    if (!rest_request_reflected_component(req, &component)) {
        return sihttp_response_json_error(404, "component not found");
    }
    if (ecs_has_cid(entity, component)) {
        return sihttp_response_json_error(409, "entity already has component");
    }

    sijson_value_t body = req->body ? sijson_parse(req->body) : NULL;
    if (!body || sijson_type(body) != SIJSON_OBJECT) {
        return sihttp_response_json_error(400, "invalid json body");
    }

    size_t body_len = sijson_object_len(body);
    if (body_len == 0) {
        ecs_add_cid(entity, component);
    } else {
        sijson_value_t value = body_len == 1 ? sijson_object_get(body, "value") : NULL;
        if (!value) {
            return sihttp_response_json_error(400, "invalid json body");
        }

        void *decoded = NULL;
        if (!ecs_rest_decode_component_value(component, value, &decoded)) {
            return sihttp_response_json_error(400, "invalid component value");
        }
        ecs_set_cid(entity, component, decoded);
    }

    return sihttp_response_json(
        201,
        ecs_rest_entity_component_json(component, ecs_get_cid(entity, component))
    );
}

sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = ecs_rest_request_entity(req);
    if (!entity) {
        return sihttp_response_json_error(404, "entity not found");
    }

    ecs_component_t component = 0;
    if (!rest_request_reflected_component(req, &component)) {
        return sihttp_response_json_error(404, "component not found");
    }

    return ecs_rest_set_entity_component(entity, component, req->body);
}

sihttp_response_t ecs_rest_delete_entity_component(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = ecs_rest_request_entity(req);
    if (!entity) {
        return sihttp_response_json_error(404, "entity not found");
    }

    ecs_component_t component = 0;
    if (!rest_request_reflected_component(req, &component)) {
        return sihttp_response_json_error(404, "component not found");
    }
    if (!ecs_has_cid_owned(entity, component)) {
        return sihttp_response_json_error(404, "entity component not found");
    }

    ecs_remove_cid(entity, component);
    return sihttp_response_empty(204);
}
