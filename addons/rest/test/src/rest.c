#include "rest_internal.h"
#include "siecs_rest.h"
#include <siecs_test.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

ECS_COMPONENT_DECLARE(RestTestPosition, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(RestTestPosition);

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

    test_not_null((void *)schema);
    test_not_null((void *)position);
    test_not_null((void *)child_of);
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

    ecs_fini();
}

void rest_component_mutation_uses_public_metadata(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestTestPosition);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, RestTestPosition, { 1.0f, 2.0f });
    sihttp_response_t response = ecs_rest_set_entity_component(
        entity,
        ecs_id(RestTestPosition),
        "{\"value\":{\"x\":30,\"y\":40}}"
    );

    test_int(200, response.status);
    test_int(30, (int)ecs_get(entity, RestTestPosition)->x);
    test_int(40, (int)ecs_get(entity, RestTestPosition)->y);
    free(response.body);

    ecs_fini();
}
