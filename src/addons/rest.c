#include "rest_internal.h"
#ifndef SIHTTP_H
#include "sihttp.h"
#endif
#include <string.h>

sihttp_response_t health(const sihttp_request_t *) {
    return sihttp_response({ .body = strdup("OK") });
}

void init_rest() {
    ecs_world.server = sihttp_server(
        {
            .port = 4040,
            .state = NULL,
        }
    );

    sihttp_get(ecs_world.server, "/schema", ecs_rest_get_schema);
    sihttp_get(ecs_world.server, "/entities", ecs_rest_get_entities);
    sihttp_post(ecs_world.server, "/entities", ecs_rest_post_entities);
    sihttp_get(ecs_world.server, "/entities/:index/children", ecs_rest_get_entity_children);
    sihttp_get(ecs_world.server, "/health", health);
    sihttp_put(
        ecs_world.server,
        "/entities/:index/components/:component",
        ecs_rest_put_entity_component
    );
    sihttp_get(ecs_world.server, "/entities/:index", ecs_rest_get_entity);

    if (ecs_world.features.rest) {
        sihttp_server_start(ecs_world.server);
    }
}
