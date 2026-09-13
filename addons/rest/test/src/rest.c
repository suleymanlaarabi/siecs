#include "rest_internal.h"
#include "siecs_rest.h"
#include <errno.h>
#include <arpa/inet.h>
#include <siecs_test.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

ECS_COMPONENT_DECLARE(RestTestPosition, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(RestTestPosition);

ECS_RELATION_DECLARE(RestTestLink);
ECS_RELATION_DEFINE(
    RestTestLink,
    {
        .storage = EcsRelationDense,
        .acyclic = true,
    }
);

ECS_RELATION_DECLARE(RestTestLoop);
ECS_RELATION_DEFINE(
    RestTestLoop,
    {
        .storage = EcsRelationDense,
    }
);

void rest_listener_failure_reports_details(void) {
    int output[2];
    test_assert(pipe(output) == 0);

    sihttp_server_t *blocker = sihttp_server({ .port = 0 });
    test_not_null(blocker);
    test_int(0, sihttp_server_listen(blocker, "127.0.0.1", 0));
    uint16_t port = sihttp_server_port(blocker);
    test_true(port != 0);

    pid_t child = fork();
    test_assert(child >= 0);
    if (child == 0) {
        close(output[0]);
        dup2(output[1], STDERR_FILENO);
        close(output[1]);
        ecs_init();
        ECS_MODULE_IMPORT(sirest, { .host = "127.0.0.1", .port = port });
        _exit(EXIT_SUCCESS);
    }

    close(output[1]);
    char message[1024] = { 0 };
    ssize_t length = read(output[0], message, sizeof(message) - 1);
    close(output[0]);
    int status = 0;
    test_assert(waitpid(child, &status, 0) == child);
    test_true(WIFSIGNALED(status));
    test_int(SIGABRT, WTERMSIG(status));
    test_true(length > 0);
    test_true(strstr(message, "sirest: failed to start REST server on 127.0.0.1:") != NULL);
    char port_text[16];
    snprintf(port_text, sizeof(port_text), ":%u:\n", port);
    test_true(strstr(message, port_text) != NULL);
    test_true(strstr(message, "bind failed: ") != NULL);
    test_true(strstr(message, strerror(EADDRINUSE)) != NULL);
    test_true(strstr(message, " (errno=") != NULL);

    sihttp_server_fini(blocker);
}

static sijson_value_t find_by_name(sijson_value_t array, const char *name) {
    for (size_t i = 0; i < sijson_array_len(array); i++) {
        sijson_value_t item = sijson_array_get(array, i);
        sijson_value_t item_name = sijson_object_get(item, "name");
        if (item_name && strcmp(sijson_string(item_name), name) == 0) {
            return item;
        }
    }
    return NULL;
}

static void assert_rest_error(sihttp_response_t *response, int status, const char *message) {
    test_int(status, response->status);
    sijson_value_t body = sijson_parse(response->body);
    test_str(message, sijson_string(sijson_object_get(body, "error")));
    sihttp_response_fini(response);
}

static uint16_t rest_available_port(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    test_assert(fd >= 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    test_assert(bind(fd, (struct sockaddr *)&address, sizeof(address)) == 0);
    socklen_t length = sizeof(address);
    test_assert(getsockname(fd, (struct sockaddr *)&address, &length) == 0);
    uint16_t port = ntohs(address.sin_port);
    close(fd);
    return port;
}

static char *rest_http_request(
    uint16_t port,
    const char *method,
    const char *path,
    const char *body,
    int *status
) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    test_assert(fd >= 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    test_assert(connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0);

    size_t body_len = body ? strlen(body) : 0;
    char request[1024];
    int request_len = snprintf(
        request,
        sizeof(request),
        "%s %s HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
        method,
        path,
        body_len,
        body ? body : ""
    );
    test_assert(request_len > 0 && (size_t)request_len < sizeof(request));
    test_assert(send(fd, request, (size_t)request_len, 0) == request_len);
    test_true(ecs_progress());

    char response[8192];
    size_t response_len = 0;
    while (response_len + 1 < sizeof(response)) {
        ssize_t received = recv(
            fd,
            response + response_len,
            sizeof(response) - response_len - 1,
            0
        );
        if (received <= 0) {
            break;
        }
        response_len += (size_t)received;
    }
    close(fd);
    response[response_len] = 0;
    test_assert(sscanf(response, "HTTP/1.1 %d", status) == 1);

    const char *response_body = strstr(response, "\r\n\r\n");
    response_body = response_body ? response_body + 4 : "";
    size_t response_body_len = response_len - (size_t)(response_body - response);
    char *copy = malloc(response_body_len + 1);
    test_not_null(copy);
    memcpy(copy, response_body, response_body_len);
    copy[response_body_len] = 0;
    return copy;
}

void rest_poll_mutations_are_immediate(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestTestPosition);

    uint16_t port = rest_available_port();
    sirest_import(&(sirest_props_t){
        .host = "127.0.0.1",
        .port = port,
    });
    uint16_t server_port = sihttp_server_port(ecs_get_resource(SiecsRestState)->server);

    ecs_entity_t entity = ecs_new();
    char path[128];
    snprintf(
        path,
        sizeof(path),
        "/entities/%u/components/%u",
        ecs_entity_id(entity),
        ecs_id(RestTestPosition)
    );

    int status;
    char *body = rest_http_request(server_port, "POST", path, "{}", &status);
    test_int(201, status);
    sijson_value_t component = sijson_parse(body);
    sijson_value_t value = sijson_object_get(component, "value");
    test_int(0, (int)sijson_number(sijson_object_get(value, "x")));
    test_int(0, (int)sijson_number(sijson_object_get(value, "y")));
    free(body);

    body = rest_http_request(
        server_port,
        "PUT",
        path,
        "{\"value\":{\"x\":42,\"y\":12}}",
        &status
    );
    test_int(200, status);
    component = sijson_parse(body);
    value = sijson_object_get(component, "value");
    test_int(42, (int)sijson_number(sijson_object_get(value, "x")));
    test_int(12, (int)sijson_number(sijson_object_get(value, "y")));
    free(body);

    ecs_entity_t parent = ecs_new();
    ecs_entity_t replacement = ecs_new();
    ecs_entity_t child = ecs_new();
    char relation_path[128];
    snprintf(
        relation_path,
        sizeof(relation_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(child),
        ecs_rid(ChildOf)
    );
    char relation_body[64];
    snprintf(relation_body, sizeof(relation_body), "{\"target\":%u}", ecs_entity_id(parent));

    body = rest_http_request(server_port, "PUT", relation_path, relation_body, &status);
    test_int(200, status);
    free(body);

    body = rest_http_request(server_port, "GET", "/entities", NULL, &status);
    test_int(200, status);
    sijson_value_t roots = sijson_parse(body);
    test_null((void *)find_by_name(roots, ecs_entity_name(child)));
    free(body);

    char children_path[128];
    snprintf(children_path, sizeof(children_path), "/entities/%u/children", ecs_entity_id(parent));
    body = rest_http_request(server_port, "GET", children_path, NULL, &status);
    test_int(200, status);
    sijson_value_t children = sijson_parse(body);
    test_not_null((void *)find_by_name(children, ecs_entity_name(child)));
    free(body);

    snprintf(
        relation_body,
        sizeof(relation_body),
        "{\"target\":%u}",
        ecs_entity_id(replacement)
    );
    body = rest_http_request(server_port, "PUT", relation_path, relation_body, &status);
    test_int(200, status);
    free(body);

    body = rest_http_request(server_port, "GET", children_path, NULL, &status);
    test_int(200, status);
    children = sijson_parse(body);
    test_null((void *)find_by_name(children, ecs_entity_name(child)));
    free(body);

    snprintf(
        children_path,
        sizeof(children_path),
        "/entities/%u/children",
        ecs_entity_id(replacement)
    );
    body = rest_http_request(server_port, "GET", children_path, NULL, &status);
    test_int(200, status);
    children = sijson_parse(body);
    test_not_null((void *)find_by_name(children, ecs_entity_name(child)));
    free(body);

    char replacement_relation_path[128];
    snprintf(
        replacement_relation_path,
        sizeof(replacement_relation_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(replacement),
        ecs_rid(ChildOf)
    );
    snprintf(relation_body, sizeof(relation_body), "{\"target\":%u}", ecs_entity_id(child));
    body = rest_http_request(
        server_port,
        "PUT",
        replacement_relation_path,
        relation_body,
        &status
    );
    test_int(409, status);
    free(body);

    body = rest_http_request(server_port, "DELETE", relation_path, NULL, &status);
    test_int(204, status);
    free(body);
    body = rest_http_request(server_port, "GET", "/entities", NULL, &status);
    test_int(200, status);
    roots = sijson_parse(body);
    test_not_null((void *)find_by_name(roots, ecs_entity_name(child)));
    free(body);

    body = rest_http_request(server_port, "DELETE", relation_path, NULL, &status);
    test_int(404, status);
    test_str("relation not present on entity", sijson_string(
        sijson_object_get(sijson_parse(body), "error")
    ));
    free(body);

    char all_path[] = "/entities/all";
    body = rest_http_request(server_port, "GET", all_path, NULL, &status);
    test_int(200, status);
    sijson_value_t all = sijson_parse(body);
    test_not_null((void *)find_by_name(all, ecs_entity_name(parent)));
    test_not_null((void *)find_by_name(all, ecs_entity_name(child)));
    free(body);

    ecs_fini();
}

void rest_module_lifecycle(void) {
    ecs_init();

    ecs_module_id_t module = ECS_MODULE_IMPORT(sirest, { .port = 4041 });
    test_assert(module != 0);
    test_true(ecs_module_is_enabled(module));
    test_true(ecs_progress());

    ecs_module_disable(module);
    test_false(ecs_module_is_enabled(module));
    ecs_module_enable(module);
    test_true(ecs_module_is_enabled(module));

    ecs_fini();
}

void rest_in_process_dispatch(void) {
    ecs_init();

    sirest_import(&(sirest_props_t){
        .in_process = true,
    });

    sihttp_response_t response = sirest_dispatch(SIHTTP_METHOD_GET, "/health", NULL);
    test_int(response.status, 200);
    test_str(response.body, "OK");
    sihttp_response_fini(&response);

    response = sirest_dispatch(SIHTTP_METHOD_GET, "/entities", NULL);
    test_int(response.status, 200);
    test_str(response.body, "[]");
    sihttp_response_fini(&response);

    ecs_fini();
}

void rest_schema_uses_public_metadata(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestTestPosition);

    sihttp_response_t response = ecs_rest_get_schema(&(sihttp_request_t){ 0 });
    test_int(200, response.status);

    sijson_value_t schema = sijson_parse(response.body);
    sijson_value_t components = sijson_object_get(schema, "components");
    sijson_value_t relations = sijson_object_get(schema, "relations");
    sijson_value_t position = find_by_name(components, "RestTestPosition");
    sijson_value_t child_of = find_by_name(relations, "ChildOf");
    sijson_value_t is_a = find_by_name(relations, "IsA");

    test_not_null((void *)schema);
    test_not_null((void *)position);
    test_not_null((void *)child_of);
    test_not_null((void *)is_a);
    test_int(2, (int)sijson_array_len(sijson_object_get(position, "fields")));
    test_int(EcsRelationByDepth, (int)sijson_number(sijson_object_get(child_of, "storage")));

    free(response.body);
    ecs_fini();
}

void rest_entity_routes_use_public_introspection(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestTestPosition);

    ecs_entity_t parent = ecs_new();
    ecs_entity_t child = ecs_new();
    ecs_set(parent, RestTestPosition, { 10.0f, 20.0f });
    ecs_relate(child, ChildOf, parent);

    sijson_clean();
    sijson_value_t detail = ecs_rest_entity_detail_json(parent);
    sijson_value_t children = sijson_object_get(detail, "children");
    test_int(1, (int)sijson_array_len(children));
    test_uint(
        ecs_entity_id(child),
        (uint32_t)sijson_number(sijson_object_get(sijson_array_get(children, 0), "index"))
    );
    test_not_null(
        (void *)find_by_name(sijson_object_get(detail, "components"), "RestTestPosition")
    );
    test_null((void *)sijson_object_get(detail, "parent"));
    test_null((void *)sijson_object_get(detail, "isA"));
    test_not_null((void *)sijson_object_get(detail, "relations"));

    ecs_fini();
}

void rest_relation_routes_are_generic_and_validated(void) {
    ecs_init();
    ECS_RELATION_REGISTER(RestTestLink, RestTestLoop);
    sirest_import(&(sirest_props_t){ .in_process = true });

    ecs_entity_t source = ecs_new();
    ecs_entity_t target = ecs_new();
    char source_path[128];
    char target_path[128];
    char body[64];
    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(source),
        ecs_rid(RestTestLink)
    );
    snprintf(
        target_path,
        sizeof(target_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(target),
        ecs_rid(RestTestLink)
    );
    snprintf(body, sizeof(body), "{\"target\":%u}", ecs_entity_id(target));

    sihttp_response_t response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, body);
    test_int(200, response.status);
    sijson_value_t relation = sijson_parse(response.body);
    test_int(ecs_rid(RestTestLink), (int)sijson_number(sijson_object_get(relation, "id")));
    test_str("RestTestLink", sijson_string(sijson_object_get(relation, "name")));
    sijson_value_t target_ref = sijson_object_get(relation, "target");
    test_int(3, (int)sijson_object_len(target_ref));
    test_uint(
        ecs_entity_id(target),
        (uint32_t)sijson_number(sijson_object_get(target_ref, "index"))
    );
    sihttp_response_fini(&response);

    snprintf(source_path, sizeof(source_path), "/entities/%u/relations", ecs_entity_id(source));
    response = sirest_dispatch(SIHTTP_METHOD_GET, source_path, NULL);
    test_int(200, response.status);
    sijson_value_t relations = sijson_parse(response.body);
    test_not_null((void *)find_by_name(relations, "RestTestLink"));
    sihttp_response_fini(&response);

    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(source),
        ecs_rid(RestTestLink)
    );
    snprintf(body, sizeof(body), "{\"target\":%u}", ecs_entity_id(source));
    response = sirest_dispatch(SIHTTP_METHOD_PUT, target_path, body);
    test_int(409, response.status);
    sijson_value_t error = sijson_parse(response.body);
    test_str("relation would create a cycle", sijson_string(sijson_object_get(error, "error")));
    sihttp_response_fini(&response);

    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(source),
        ecs_rid(RestTestLink)
    );
    ecs_entity_t replacement = ecs_new();
    snprintf(body, sizeof(body), "{\"target\":%u}", ecs_entity_id(replacement));
    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, body);
    test_int(200, response.status);
    test_uint(replacement, ecs_target_id(source, ecs_rid(RestTestLink)));
    sihttp_response_fini(&response);

    snprintf(body, sizeof(body), "{\"target\":%u}", ecs_entity_id(target));
    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%ujunk/relations/%u",
        ecs_entity_id(source),
        ecs_rid(RestTestLink)
    );
    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, body);
    test_int(404, response.status);
    sihttp_response_fini(&response);

    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%u/relations/%ujunk",
        ecs_entity_id(source),
        ecs_rid(RestTestLink)
    );
    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, body);
    test_int(404, response.status);
    sihttp_response_fini(&response);

    ecs_entity_t isolated = ecs_new();
    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(isolated),
        ecs_rid(RestTestLink)
    );
    snprintf(body, sizeof(body), "{\"target\":%u}", ecs_entity_id(isolated));
    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, body);
    test_int(409, response.status);
    sihttp_response_fini(&response);

    ecs_entity_t loop_left = ecs_new();
    ecs_entity_t loop_right = ecs_new();
    ecs_relate(loop_left, RestTestLoop, loop_right);
    ecs_relate(loop_right, RestTestLoop, loop_left);
    test_true(
        ecs_rest_relation_would_cycle(
            isolated,
            ecs_rid(RestTestLoop),
            loop_left
        )
    );

    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(source),
        ecs_rid(RestTestLink)
    );
    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, "{}");
    test_int(400, response.status);
    sihttp_response_fini(&response);

    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, "{\"target\":0}");
    assert_rest_error(&response, 400, "invalid json body");

    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, "{\"target\":-1}");
    assert_rest_error(&response, 400, "invalid json body");

    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, "{\"target\":1.5}");
    assert_rest_error(&response, 400, "invalid json body");

    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, "{\"target\":4294967295}");
    assert_rest_error(&response, 404, "target not found");

    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/4294967295/relations/%u",
        ecs_rid(RestTestLink)
    );
    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, body);
    assert_rest_error(&response, 404, "entity not found");

    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%u/relations/65535",
        ecs_entity_id(source)
    );
    response = sirest_dispatch(SIHTTP_METHOD_PUT, source_path, body);
    assert_rest_error(&response, 404, "relation not found");

    snprintf(
        source_path,
        sizeof(source_path),
        "/entities/%u/relations/%u",
        ecs_entity_id(source),
        ecs_rid(RestTestLink)
    );
    response = sirest_dispatch(SIHTTP_METHOD_DELETE, source_path, NULL);
    test_int(204, response.status);
    test_false(ecs_has_relation_id(source, ecs_rid(RestTestLink)));
    sihttp_response_fini(&response);

    ecs_fini();
}

void rest_component_mutation_uses_public_metadata(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestTestPosition);
    sirest_import(&(sirest_props_t){ .in_process = true });

    ecs_entity_t entity = ecs_new();
    char path[128];
    char entity_path[64];
    snprintf(
        path,
        sizeof(path),
        "/entities/%u/components/%u",
        ecs_entity_id(entity),
        ecs_id(RestTestPosition)
    );

    sihttp_response_t response = sirest_dispatch(SIHTTP_METHOD_POST, path, "{}");
    test_int(201, response.status);
    test_true(ecs_has(entity, RestTestPosition));
    test_int(0, (int)ecs_get(entity, RestTestPosition)->x);
    test_int(0, (int)ecs_get(entity, RestTestPosition)->y);
    sijson_value_t component = sijson_parse(response.body);
    test_int(ecs_id(RestTestPosition), (int)sijson_number(sijson_object_get(component, "id")));
    test_str("RestTestPosition", sijson_string(sijson_object_get(component, "name")));
    sihttp_response_fini(&response);

    snprintf(entity_path, sizeof(entity_path), "/entities/%u", ecs_entity_id(entity));
    response = sirest_dispatch(SIHTTP_METHOD_GET, entity_path, NULL);
    test_int(200, response.status);
    sijson_value_t detail = sijson_parse(response.body);
    test_not_null(
        (void *)find_by_name(sijson_object_get(detail, "components"), "RestTestPosition")
    );
    sihttp_response_fini(&response);

    response = sirest_dispatch(SIHTTP_METHOD_POST, path, "{}");
    assert_rest_error(&response, 409, "entity already has component");

    response = sirest_dispatch(SIHTTP_METHOD_PUT, path, NULL);
    assert_rest_error(&response, 400, "invalid json body");
    test_int(0, (int)ecs_get(entity, RestTestPosition)->x);
    test_int(0, (int)ecs_get(entity, RestTestPosition)->y);

    response = sirest_dispatch(
        SIHTTP_METHOD_PUT,
        path,
        "{\"value\":{\"x\":30,\"y\":40}}"
    );
    test_int(200, response.status);
    test_int(30, (int)ecs_get(entity, RestTestPosition)->x);
    test_int(40, (int)ecs_get(entity, RestTestPosition)->y);
    sihttp_response_fini(&response);

    response = sirest_dispatch(SIHTTP_METHOD_GET, entity_path, NULL);
    test_int(200, response.status);
    detail = sijson_parse(response.body);
    component = find_by_name(sijson_object_get(detail, "components"), "RestTestPosition");
    test_not_null((void *)component);
    sijson_value_t value = sijson_object_get(component, "value");
    test_int(30, (int)sijson_number(sijson_object_get(value, "x")));
    test_int(40, (int)sijson_number(sijson_object_get(value, "y")));
    sihttp_response_fini(&response);

    response = sirest_dispatch(SIHTTP_METHOD_DELETE, path, NULL);
    test_int(204, response.status);
    test_false(ecs_has(entity, RestTestPosition));
    sihttp_response_fini(&response);

    response = sirest_dispatch(SIHTTP_METHOD_GET, entity_path, NULL);
    test_int(200, response.status);
    detail = sijson_parse(response.body);
    test_null(
        (void *)find_by_name(sijson_object_get(detail, "components"), "RestTestPosition")
    );
    sihttp_response_fini(&response);

    response = sirest_dispatch(SIHTTP_METHOD_DELETE, path, NULL);
    assert_rest_error(&response, 404, "entity component not found");

    response = sirest_dispatch(
        SIHTTP_METHOD_PUT,
        path,
        "{\"value\":{\"x\":30,\"y\":40}}"
    );
    assert_rest_error(&response, 404, "entity component not found");

    response = sirest_dispatch(
        SIHTTP_METHOD_POST,
        path,
        "{\"value\":{\"x\":10,\"y\":20}}"
    );
    test_int(201, response.status);
    test_int(10, (int)ecs_get(entity, RestTestPosition)->x);
    test_int(20, (int)ecs_get(entity, RestTestPosition)->y);
    sihttp_response_fini(&response);

    ecs_entity_t invalid_entity = ecs_new();
    snprintf(
        path,
        sizeof(path),
        "/entities/%u/components/%u",
        ecs_entity_id(invalid_entity),
        ecs_id(RestTestPosition)
    );
    response = sirest_dispatch(
        SIHTTP_METHOD_POST,
        path,
        "{\"value\":{\"x\":\"bad\",\"y\":20}}"
    );
    assert_rest_error(&response, 400, "invalid component value");
    test_false(ecs_has(invalid_entity, RestTestPosition));

    response = sirest_dispatch(SIHTTP_METHOD_POST, path, "{invalid");
    assert_rest_error(&response, 400, "invalid json body");

    response = sirest_dispatch(
        SIHTTP_METHOD_POST,
        path,
        "{\"value\":{\"x\":10,\"y\":20},\"extra\":true}"
    );
    assert_rest_error(&response, 400, "invalid json body");

    snprintf(
        path,
        sizeof(path),
        "/entities/%u/components/65535",
        ecs_entity_id(entity)
    );
    response = sirest_dispatch(SIHTTP_METHOD_POST, path, "{}");
    assert_rest_error(&response, 404, "component not found");

    snprintf(
        path,
        sizeof(path),
        "/entities/4294967295/components/%u",
        ecs_id(RestTestPosition)
    );
    response = sirest_dispatch(SIHTTP_METHOD_POST, path, "{}");
    assert_rest_error(&response, 404, "entity not found");

    ecs_fini();
}
