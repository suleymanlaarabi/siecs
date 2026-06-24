#include "addons/rest_internal.h"
#include "id.h"
#include <stdint.h>

sijson_value_t ecs_rest_entity_json(ecs_world_t *world, ecs_entity_t entity) {
    sijson_value_t object = sijson_make_object();

    const char *name = NULL;

    if (ecs_has(world, entity, Name)) {
        name = ecs_get(world, entity, Name)->value;
    } else {
        name = siformat("(%d, %d)", ecs_first(entity), ecs_second(entity));
    }

    sijson_object_set(object, "name", sijson_make_string(name));
    sijson_object_set(object, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(object, "generation", sijson_make_number(ecs_second(entity)));

    if (ecs_has_cid(world, entity, ecs_source(ChildOf))) {
        sijson_object_set(object, "hasChildren", sijson_make_bool(true));
    }

    return object;
}

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;

    int64_t id = sihttp_param(req, "id");

    sijson_clean();

    sijson_value_t array = sijson_make_array();

    if (id != 0) {
        ecs_query_each(world, it, i, { ecs_id(ChildOf) }) {
            sijson_array_push(array, ecs_rest_entity_json(world, it.entities[i]));
        }
    } else {
        ecs_query_each(world, it, i, { ecs_source(ChildOf) }, { ecs_id(ChildOf), EcsNot }) {
            sijson_array_push(array, ecs_rest_entity_json(world, it.entities[i]));
        }
        ecs_query_each(world, it, i, { ecs_source(ChildOf), EcsNot }, { ecs_id(ChildOf), EcsNot }) {
            sijson_array_push(array, ecs_rest_entity_json(world, it.entities[i]));
        }
    }

    return ecs_rest_json_response(200, array);
}
