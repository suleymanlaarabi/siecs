#include "rest_internal.h"
#include <stdint.h>

static ecs_query_id_t rest_entity_query(ecs_query_relation_term_t relation) {
    return ecs_query({
        .components = {
            ecs_in_optional(Abstract),
            ecs_in_optional(Disabled),
        },
        .relations = { relation },
    });
}

static void rest_append_entity(sijson_value_t array, ecs_entity_t entity) {
    if (ecs_is_alive(entity)) {
        sijson_array_push(array, ecs_rest_entity_json(entity));
    }
}

static bool rest_entity_has_children(ecs_entity_t parent) {
    ecs_query_id_t query = rest_entity_query(ecs_rel(ChildOf));
    ecs_iter_t it = ecs_query_iter(query);
    while (ecs_iter_next(&it)) {
        const ecs_relation_target_t *targets = ecs_targets(&it, ChildOf);
        for (uint32_t i = 0; i < it.count; i++) {
            if (targets[i].entity == parent) {
                ecs_query_fini(query);
                return true;
            }
        }
    }
    ecs_query_fini(query);
    return false;
}

sijson_value_t ecs_rest_entity_json(ecs_entity_t entity) {
    sijson_value_t object = sijson_make_object();

    sijson_object_set(object, "name", sijson_make_string(ecs_entity_name(entity)));
    sijson_object_set(object, "index", sijson_make_number(ecs_entity_id(entity)));
    sijson_object_set(object, "generation", sijson_make_number(ecs_entity_generation(entity)));
    sijson_object_set(object, "hasChildren", sijson_make_bool(rest_entity_has_children(entity)));
    return object;
}

sijson_value_t ecs_rest_entity_children_json(ecs_entity_t entity) {
    sijson_value_t children = sijson_make_array();
    ecs_query_id_t query = rest_entity_query(ecs_rel(ChildOf));
    ecs_iter_t it = ecs_query_iter(query);
    while (ecs_iter_next(&it)) {
        const ecs_relation_target_t *targets = ecs_targets(&it, ChildOf);
        for (uint32_t i = 0; i < it.count; i++) {
            ecs_entity_t child = it.entities[i];
            if (targets[i].entity == entity) {
                rest_append_entity(children, child);
            }
        }
    }
    ecs_query_fini(query);
    return children;
}

sijson_value_t ecs_rest_entity_detail_json(ecs_entity_t entity) {
    sijson_value_t detail = sijson_make_object();

    sijson_object_set(detail, "name", sijson_make_string(ecs_entity_name(entity)));
    sijson_object_set(detail, "index", sijson_make_number(ecs_entity_id(entity)));
    sijson_object_set(
        detail,
        "generation",
        sijson_make_number(ecs_entity_generation(entity))
    );

    ecs_entity_t parent = ecs_target(entity, ChildOf);
    if (parent) {
        sijson_object_set(detail, "parent", ecs_rest_entity_json(parent));
    }

    sijson_value_t components = sijson_make_array();
    for (uint32_t id = 1; id < ecs_component_count(); id++) {
        ecs_component_t component = (ecs_component_t)id;
        const ecs_component_info_t *info = ecs_component_info(component);
        if (info && info->reflection && ecs_has_cid_owned(entity, component)) {
            sijson_array_push(
                components,
                ecs_rest_entity_component_json(component, ecs_get_cid(entity, component))
            );
        }
    }

    ecs_entity_t base = ecs_entity_base(entity);
    if (base) {
        sijson_object_set(detail, "isA", ecs_rest_entity_detail_json(base));
    }

    sijson_object_set(detail, "children", ecs_rest_entity_children_json(entity));
    sijson_object_set(detail, "components", components);
    return detail;
}

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req) {
    (void)req;
    sijson_clean();

    sijson_value_t array = sijson_make_array();
    ecs_query_id_t query = rest_entity_query(ecs_not_rel(ChildOf));
    ecs_iter_t it = ecs_query_iter(query);
    while (ecs_iter_next(&it)) {
        for (uint32_t i = 0; i < it.count; i++) {
            rest_append_entity(array, it.entities[i]);
        }
    }
    ecs_query_fini(query);
    return ecs_rest_json_response(200, array);
}

sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req) {
    sijson_clean();

    int64_t index = sihttp_param(req, "index");
    ecs_entity_t entity = index > 0 && index <= UINT32_MAX
        ? ecs_entity_from_index((uint32_t)index)
        : 0;
    if (!entity) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_detail_json(entity));
}

sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req) {
    sijson_clean();

    int64_t index = sihttp_param(req, "index");
    ecs_entity_t entity = index > 0 && index <= UINT32_MAX
        ? ecs_entity_from_index((uint32_t)index)
        : 0;
    if (!entity) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_children_json(entity));
}

sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req) {
    int64_t component = sihttp_param(req, "component");
    int64_t index = sihttp_param(req, "index");
    ecs_entity_t entity = index > 0 && index <= UINT32_MAX
        ? ecs_entity_from_index((uint32_t)index)
        : 0;
    if (!entity) {
        sijson_clean();
        return ecs_rest_error_response(404, "entity not found");
    }
    if (component <= 0 || component >= ecs_component_count() || component > UINT16_MAX) {
        sijson_clean();
        return ecs_rest_error_response(404, "component not found");
    }

    return ecs_rest_set_entity_component(entity, (ecs_component_t)component, req->body);
}

sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req) {
    (void)req;
    ecs_entity_t entity = ecs_new();
    sihttp_response_t response = { 0 };
    response.body = sijson_stringify(ecs_rest_entity_json(entity));
    return response;
}
