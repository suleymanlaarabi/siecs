#include "datastructure/vec.h"
#include "id.h"
#include "siecs.h"
#include "sihttp.h"
#include "sijson.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sijson_value_t entity_json(ecs_world_t *world, ecs_entity_t entity) {
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

sihttp_response_t get_entities(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;

    int64_t id = sihttp_param(req, "id");

    sijson_clean();

    sijson_value_t array = sijson_make_array();

    const uint64_t count = world->entity_index.entities.size;
    const ecs_entity_record_t *records = world->entity_index.entities.data;

    if (id != 0) {
        ecs_entity_t entity = ecs_entity(id, records[id].generation);

        if (!ecs_has_cid(world, entity, ecs_source(ChildOf))) {
            return sihttp_response(
                {
                    .status = 200,
                    .body = strdup("[]"),
                    .content_type = SIHTTP_CONTENT_JSON,
                }
            );
        };

        const RelationSource *source = ecs_get_cid(world, entity, ecs_source(ChildOf));

        ecs_vec_iter(&source->entities, ecs_entity_t, child, {
            sijson_array_push(array, entity_json(world, *child));
        });
    } else {
        for (uint64_t i = 1; i < count; i++) {
            if (records[i].table_id == UINT16_MAX) {
                continue;
            }

            ecs_entity_t entity = ecs_entity(i, records[i].generation);

            if (ecs_has(world, entity, ChildOf)) {
                continue;
            }

            sijson_array_push(array, entity_json(world, entity));
        }
    }

    return sihttp_response(
        {
            .status = 200,
            .body = sijson_stringify(array),
            .content_type = SIHTTP_CONTENT_JSON,
        }
    );
}

void init_rest(ecs_world_t *world) {
    sihttp_app_state_t *state = malloc(sizeof(sihttp_app_state_t));

    state->world = world;

    world->server = sihttp_server(
        {
            .port = 4040,
            .state = state,
        }
    );

    sihttp_get(world->server, "/entities", get_entities);
    sihttp_get(world->server, "/entities/:id", get_entities);

    if (world->features.rest) {
        sihttp_server_start(world->server);
    }
}
