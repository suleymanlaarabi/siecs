#include "rest_internal.h"
#include <stddef.h>
#include <stdint.h>

static bool ecs_rest_path_uint_segment(
    const sihttp_request_t *req,
    size_t segment,
    uint32_t limit,
    uint32_t *result
) {
    if (!req || !req->path) {
        return false;
    }

    const char *cursor = req->path;
    if (*cursor == '/') {
        cursor++;
    }

    for (size_t current = 0;; current++) {
        const char *end = cursor;
        while (*end && *end != '/' && *end != '?') {
            end++;
        }

        if (current == segment) {
            if (cursor == end) {
                return false;
            }

            uint32_t value = 0;
            for (const char *digit = cursor; digit < end; digit++) {
                if (*digit < '0' || *digit > '9') {
                    return false;
                }

                uint32_t number = (uint32_t)(*digit - '0');
                if (value > (limit - number) / 10) {
                    return false;
                }
                value = value * 10 + number;
            }

            *result = value;
            return true;
        }

        if (*end != '/') {
            return false;
        }
        cursor = end + 1;
    }
}

static ecs_entity_t ecs_rest_request_entity(const sihttp_request_t *req) {
    uint32_t index = 0;
    return ecs_rest_path_uint_segment(req, 1, UINT32_MAX, &index) && index
        ? ecs_entity_from_index(index)
        : 0;
}

static const ecs_relation_info_t *ecs_rest_request_relation(
    const sihttp_request_t *req,
    ecs_relation_id_t *relation
) {
    uint32_t value = 0;
    if (
        !ecs_rest_path_uint_segment(req, 3, UINT16_MAX, &value) || !value ||
        value >= ecs_relation_count()
    ) {
        return NULL;
    }

    *relation = (ecs_relation_id_t)value;
    return ecs_relation_info(*relation);
}

bool ecs_rest_relation_would_cycle(
    ecs_entity_t source,
    ecs_relation_id_t relation,
    ecs_entity_t target
) {
    ecs_entity_t current = target;
    ecs_entity_t slow = target;
    ecs_entity_t fast = target;
    while (current) {
        if (current == source) {
            return true;
        }

        current = ecs_target_id(current, relation);
        slow = slow ? ecs_target_id(slow, relation) : 0;
        fast = fast ? ecs_target_id(fast, relation) : 0;
        fast = fast ? ecs_target_id(fast, relation) : 0;
        if (slow && slow == fast) {
            return true;
        }
    }
    return false;
}

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

    ecs_entity_t entity = ecs_rest_request_entity(req);
    if (!entity) {
        return ecs_rest_error_response(404, "entity not found");
    }

    return ecs_rest_json_response(200, ecs_rest_entity_relations_json(entity));
}

sihttp_response_t ecs_rest_put_entity_relation(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t source = ecs_rest_request_entity(req);
    if (!source) {
        return ecs_rest_error_response(404, "entity not found");
    }

    ecs_relation_id_t relation = 0;
    const ecs_relation_info_t *info = ecs_rest_request_relation(req, &relation);
    if (!info) {
        return ecs_rest_error_response(404, "relation not found");
    }

    sijson_value_t body = req->body ? sijson_parse(req->body) : NULL;
    sijson_value_t target_value = body && sijson_type(body) == SIJSON_OBJECT
        ? sijson_object_get(body, "target")
        : NULL;
    if (
        !body || sijson_type(body) != SIJSON_OBJECT || sijson_object_len(body) != 1 ||
        !target_value || sijson_type(target_value) != SIJSON_NUMBER
    ) {
        return ecs_rest_error_response(400, "invalid json body");
    }

    double target_number = sijson_number(target_value);
    if (
        !(target_number >= 1 && target_number <= UINT32_MAX) ||
        target_number != (double)(uint32_t)target_number
    ) {
        return ecs_rest_error_response(400, "invalid json body");
    }

    ecs_entity_t target = ecs_entity_from_index((uint32_t)target_number);
    if (!target) {
        return ecs_rest_error_response(404, "target not found");
    }

    if (
        info->desc.acyclic &&
        ecs_rest_relation_would_cycle(source, relation, target)
    ) {
        return ecs_rest_error_response(409, "relation would create a cycle");
    }

    ecs_relate_id(source, relation, target);
    return ecs_rest_json_response(
        200,
        ecs_rest_entity_relation_json(relation, target)
    );
}

sihttp_response_t ecs_rest_delete_entity_relation(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t source = ecs_rest_request_entity(req);
    if (!source) {
        return ecs_rest_error_response(404, "entity not found");
    }

    ecs_relation_id_t relation = 0;
    if (!ecs_rest_request_relation(req, &relation)) {
        return ecs_rest_error_response(404, "relation not found");
    }

    ecs_unrelate_id(source, relation);
    sihttp_response_t response = { 0 };
    response.status = 204;
    return response;
}
