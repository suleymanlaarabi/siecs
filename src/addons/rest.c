#include "addons/rest_internal.h"
#include "sihttp.h"
#include <stdlib.h>

void init_rest(ecs_world_t *world) {
    sihttp_app_state_t *state = malloc(sizeof(sihttp_app_state_t));

    state->world = world;

    world->server = sihttp_server(
        {
            .port = 4040,
            .state = state,
        }
    );

    sihttp_get(world->server, "/entities", ecs_rest_get_entities);
    sihttp_get(world->server, "/entities/:id", ecs_rest_get_entities);
    sihttp_get(world->server, "/schema", ecs_rest_get_schema);

    if (world->features.rest) {
        sihttp_server_start(world->server);
    }
}
