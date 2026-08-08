#include "rest_internal.h"
#include "siecs_rest.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rest_state_on_remove(const void *ptr);
static sihttp_response_t rest_health(const sihttp_request_t *req);

ECS_RESOURCE_DEFINE(SiecsRestState, .on_remove = rest_state_on_remove);
ECS_MODULE_DEFINE(sirest);

static void rest_fail(const sirest_props_t *props, int error_number) {
    const char *error = sihttp_error();
    const char *host = props->host && props->host[0] ? props->host : "0.0.0.0";

    if (error_number) {
        fprintf(
            stderr,
            "sirest: failed to start REST server on %s:%d:\n%s (errno=%d)\n",
            host,
            props->port,
            error ? error : "unknown error",
            error_number
        );
    } else {
        fprintf(
            stderr,
            "sirest: failed to start REST server on %s:%d:\n%s\n",
            host,
            props->port,
            error ? error : "unknown error"
        );
    }
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

static sihttp_server_t *rest_server_create(const sirest_props_t *props) {
    sihttp_server_t *server = sihttp_server(
        {
            .port = props->port,
            .backlog = props->backlog,
            .max_requests_per_poll = props->max_requests_per_poll,
        }
    );
    if (!server) {
        rest_fail(props, errno);
    }

    int listen_result = sihttp_server_listen(server, props->host, (uint16_t)props->port);
    int listen_errno = errno;
    if (listen_result != 0) {
        sihttp_server_fini(server);
        rest_fail(props, listen_errno);
    }

    if (sihttp_server_start(server) != 0) {
        int start_errno = errno;
        sihttp_server_fini(server);
        rest_fail(props, start_errno);
    }
    return server;
}

void sirest_import(const sirest_props_t *props) {
    const sirest_props_t defaults = {
        .host = NULL,
        .port = 4040,
        .backlog = 0,
        .max_requests_per_poll = 0,
    };
    sirest_props_t config = props ? *props : defaults;
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

    ecs_system(
        {
            .name = "SiecsRestPoll",
            .callback = rest_poll,
            .phase = EcsPostRender,
        }
    );
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
