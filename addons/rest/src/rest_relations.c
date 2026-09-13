#include "rest_internal.h"
#include <stdint.h>

sijson_value_t ecs_rest_entity_ref_json(ecs_entity_t entity) {
    sijson_value_t reference = sijson_make_object();
    sijson_object_set(reference, "index", sijson_make_number(ecs_entity_id(entity)));
    sijson_object_set(
        reference,
        "generation",
        sijson_make_number(ecs_entity_generation(entity))
    );
    sijson_object_set(reference, "name", sijson_make_string(ecs_entity_name(entity)));
    return reference;
}

sijson_value_t ecs_rest_entity_relation_json(
    ecs_relation_id_t relation,
    ecs_entity_t target
) {
    const ecs_relation_info_t *info = ecs_relation_info(relation);
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(relation));
    sijson_object_set(
        object,
        "name",
        sijson_make_string(info && info->name ? info->name : "")
    );
    sijson_object_set(object, "target", ecs_rest_entity_ref_json(target));
    return object;
}

sijson_value_t ecs_rest_entity_relations_json(ecs_entity_t entity) {
    sijson_value_t relations = sijson_make_array();
    for (uint32_t id = 1; id < ecs_relation_count(); id++) {
        ecs_relation_id_t relation = (ecs_relation_id_t)id;
        const ecs_relation_info_t *info = ecs_relation_info(relation);
        if (!info || !ecs_has_relation_id(entity, relation)) {
            continue;
        }

        ecs_entity_t target = ecs_target_id(entity, relation);
        if (target) {
            sijson_array_push(
                relations,
                ecs_rest_entity_relation_json(relation, target)
            );
        }
    }
    return relations;
}

sihttp_response_t ecs_rest_get_entity_relations(const sihttp_request_t *req) {
    sijson_clean();

    int64_t index = sihttp_param(req, "index");
    ecs_entity_t entity = index > 0 && index <= UINT32_MAX
        ? ecs_entity_from_index((uint32_t)index)
        : 0;
    if (!entity) {
        return ecs_rest_error_response(404, "entity not found");
    }

    return ecs_rest_json_response(200, ecs_rest_entity_relations_json(entity));
}
