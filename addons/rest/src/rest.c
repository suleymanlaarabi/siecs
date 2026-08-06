#include "rest_internal.h"
#include "siecs_rest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rest_state_on_remove(const void *ptr);
static sihttp_response_t rest_health(const sihttp_request_t *req);

ECS_RESOURCE_DEFINE(SiecsRestState, .on_remove = rest_state_on_remove);
ECS_MODULE_DEFINE(SiecsRest);

static void rest_fail(const char *operation) {
    const char *error = sihttp_error();
    fprintf(stderr, "siecs_rest: %s%s%s\n", operation, error ? ": " : "", error ? error : "");
    abort();
}

static void rest_state_on_remove(const void *ptr) {
    const SiecsRestState *state = ptr;
    if (state->server) {
        sihttp_server_stop(state->server);
        sihttp_server_fini(state->server);
    }
    ecs_id(SiecsRestState) = 0;
}

static void rest_poll(ecs_iter_t *it) {
    (void)it;
    SiecsRestState *state = ecs_try_get_resource(SiecsRestState);
    if (state && state->server) {
        sihttp_server_poll(state->server);
    }
}

static sihttp_server_t *rest_server_create(const SiecsRest_props_t *props) {
    sihttp_server_t *server = sihttp_server({
        .port = props->port,
        .backlog = props->backlog,
        .max_requests_per_poll = props->max_requests_per_poll,
    });
    if (!server) {
        rest_fail("create server");
    }

    if (sihttp_server_listen(server, props->host, (uint16_t)props->port) != 0) {
        sihttp_server_fini(server);
        rest_fail("listen server");
    }

    if (sihttp_server_start(server) != 0) {
        sihttp_server_fini(server);
        rest_fail("start server");
    }
    return server;
}

void SiecsRest_import(const SiecsRest_props_t *props) {
    const SiecsRest_props_t defaults = {
        .host = NULL,
        .port = 4040,
        .backlog = 0,
        .max_requests_per_poll = 0,
    };
    SiecsRest_props_t config = props ? *props : defaults;
    if (config.port == 0) {
        config.port = defaults.port;
    }

    ecs_id(SiecsRestState) = 0;
    ECS_RESOURCE_REGISTER(SiecsRestState);

    sihttp_server_t *server = rest_server_create(&config);
    ecs_set_resource(SiecsRestState, { .server = server });
    sihttp_get(server, "/schema", ecs_rest_get_schema);
    sihttp_get(server, "/entities", ecs_rest_get_entities);
    sihttp_post(server, "/entities", ecs_rest_post_entities);
    sihttp_get(server, "/entities/:index/children", ecs_rest_get_entity_children);
    sihttp_get(server, "/health", rest_health);
    sihttp_put(server, "/entities/:index/components/:component", ecs_rest_put_entity_component);
    sihttp_get(server, "/entities/:index", ecs_rest_get_entity);

    ecs_system({
        .name = "SiecsRestPoll",
        .callback = rest_poll,
        .phase = EcsPostRender,
    });
}

static sihttp_response_t rest_health(const sihttp_request_t *req) {
    (void)req;
    sihttp_response_t response = { 0 };
    response.body = malloc(3);
    if (response.body) {
        memcpy(response.body, "OK", 3);
    }
    return response;
}
