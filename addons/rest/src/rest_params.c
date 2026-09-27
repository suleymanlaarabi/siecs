#include "rest_internal.h"

ecs_entity_t ecs_rest_request_entity(const sihttp_request_t *req) {
    uint32_t index;
    if (!sihttp_path_param_u32(req, "index", &index) || !index) {
        return 0;
    }
    return ecs_entity_from_index(index);
}

bool ecs_rest_request_component(const sihttp_request_t *req, ecs_component_t *component) {
    uint16_t id;
    if (!sihttp_path_param_u16(req, "component", &id) || !id ||
        id >= ecs_component_count() || !ecs_component_info((ecs_component_t)id)) {
        return false;
    }
    *component = (ecs_component_t)id;
    return true;
}

bool ecs_rest_request_relation(const sihttp_request_t *req, ecs_relation_id_t *relation) {
    uint16_t id;
    if (!sihttp_path_param_u16(req, "relation", &id) || !id ||
        id >= ecs_relation_count() || !ecs_relation_info((ecs_relation_id_t)id)) {
        return false;
    }
    *relation = (ecs_relation_id_t)id;
    return true;
}
