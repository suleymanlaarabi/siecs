#include "rest_internal.h"
#include "siecs_rest.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rest_state_on_remove(const void *ptr);
static sihttp_response_t rest_health(const sihttp_request_t *req);
static void rest_register_routes(sihttp_server_t *server);

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
            .max_body_bytes = props->max_scene_bytes,
        }
    );
    if (!server) {
        rest_fail(props, errno);
    }

    rest_register_routes(server);
    if (!props->in_process) {
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
    }
    return server;
}

void sirest_import(const sirest_props_t *props) {
    const sirest_props_t defaults = {
        .host = NULL,
        .port = 4040,
        .backlog = 0,
        .max_requests_per_poll = 0,
        .max_scene_bytes = 64u * 1024u * 1024u,
        .in_process = false,
    };
    sirest_props_t config = props ? *props : defaults;
    if (config.port == 0) {
        config.port = defaults.port;
    }
    if (config.max_scene_bytes == 0) {
        config.max_scene_bytes = defaults.max_scene_bytes;
    }

    ecs_id(SiecsRestState) = 0;
    ECS_RESOURCE_REGISTER(SiecsRestState);

    sihttp_server_t *server = rest_server_create(&config);
    ecs_set_resource(SiecsRestState, {
        .server = server,
        .max_scene_bytes = config.max_scene_bytes,
    });
    if (!config.in_process) {
        ecs_system(
            {
                .name = "SiecsRestPoll",
                .query = {
                    .resources = {
                        ecs_inout(SiecsRestState),
                    },
                },
                .callback = rest_poll,
                .phase = EcsPostRender,
                .no_defer = true,
            }
        );
    }
}

static void rest_register_routes(sihttp_server_t *server) {
    sihttp_get(server, "/schema", ecs_rest_get_schema);
    sihttp_get(server, "/scene", ecs_rest_get_scene);
    sihttp_post(server, "/scene", ecs_rest_post_scene);
    sihttp_post(server, "/modules", ecs_rest_post_modules);
    sihttp_get(server, "/entities", ecs_rest_get_entities);
    sihttp_get(server, "/entities/all", ecs_rest_get_all_entities);
    sihttp_post(server, "/entities", ecs_rest_post_entities);
    sihttp_get(server, "/entities/:index/children", ecs_rest_get_entity_children);
    sihttp_get(server, "/entities/:index/relations", ecs_rest_get_entity_relations);
    sihttp_put(server, "/entities/:index/relations/:relation", ecs_rest_put_entity_relation);
    sihttp_delete(server, "/entities/:index/relations/:relation", ecs_rest_delete_entity_relation);
    sihttp_get(server, "/health", rest_health);
    sihttp_post(server, "/entities/:index/components/:component", ecs_rest_post_entity_component);
    sihttp_put(server, "/entities/:index/components/:component", ecs_rest_put_entity_component);
    sihttp_delete(server, "/entities/:index/components/:component", ecs_rest_delete_entity_component);
    sihttp_get(server, "/entities/:index", ecs_rest_get_entity);
}

sihttp_response_t sirest_dispatch(
    sihttp_method_t method,
    const char *path,
    const char *body
) {
    SiecsRestState *state = ecs_try_get_resource(SiecsRestState);

    return sihttp_server_dispatch(state->server, method, path, body);
}

sihttp_response_t sirest_dispatch_bytes(
    sihttp_method_t method,
    const char *path,
    const void *data,
    size_t size
) {
    SiecsRestState *state = ecs_try_get_resource(SiecsRestState);
    return sihttp_server_dispatch_bytes(state->server, method, path, data, size);
}

static sihttp_response_t rest_health(const sihttp_request_t *req) {
    (void)req;
    sihttp_response_t response = { 0 };
    response.status = 200;
    response.body = malloc(3);
    if (response.body) {
        memcpy(response.body, "OK", 3);
        response.body_size = 2;
    }
    return response;
}
