#include "siecs.h"
#include "siecs_rest.h"
#include "sijson.h"
#include <siecs_test.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

SIREFLECT_ENUM(
    RestMode,
    {
        REST_MODE_IDLE = -2,
        REST_MODE_RUN = 7,
        REST_MODE_PAUSED,
    }
);

ECS_COMPONENT(RestEnumComponent, { RestMode mode; });

static sijson_value_t rest_schema_type(sijson_value_t schema, sireflect_handle_t type) {
    sijson_value_t types = sijson_object_get(schema, "types");

    for (size_t i = 0; i < sijson_array_len(types); i++) {
        sijson_value_t candidate = sijson_array_get(types, i);
        sijson_value_t id = sijson_object_get(candidate, "id");

        if ((sireflect_handle_t)sijson_number(id) == type) {
            return candidate;
        }
    }

    return NULL;
}

static uint32_t rest_create_entity(void) {
    sihttp_response_t response = sirest_dispatch(SIHTTP_METHOD_POST, "/entities", NULL);
    test_true(response.body != NULL);
    sijson_value_t body = sijson_parse(response.body);
    test_true(body != NULL);
    sijson_value_t index = sijson_object_get(body, "index");
    test_true(index != NULL);
    uint32_t result = (uint32_t)sijson_number(index);
    sihttp_response_fini(&response);
    return result;
}

static void rest_expect_ok(sihttp_method_t method, const char *path, const char *body) {
    sihttp_response_t response = sirest_dispatch(method, path, body);
    test_int(200, response.status);
    test_true(response.body != NULL);
    sihttp_response_fini(&response);
}

static void rest_set_relation(uint32_t source, ecs_relation_id_t relation, uint32_t target) {
    char path[96];
    char body[48];
    snprintf(path, sizeof(path), "/entities/%u/relations/%u", source, relation);
    snprintf(body, sizeof(body), "{\"target\":%u}", target);
    rest_expect_ok(SIHTTP_METHOD_PUT, path, body);
}

static void rest_is_a_child_of_same_target_case(bool is_a_first) {
    ecs_init();
    ECS_MODULE_IMPORT(sirest, { .in_process = true });

    uint32_t a_index = rest_create_entity();
    uint32_t b_index = rest_create_entity();
    if (is_a_first) {
        rest_set_relation(b_index, ecs_rid(IsA), a_index);
        rest_set_relation(b_index, ecs_rid(ChildOf), a_index);
    } else {
        rest_set_relation(b_index, ecs_rid(ChildOf), a_index);
        rest_set_relation(b_index, ecs_rid(IsA), a_index);
    }

    rest_expect_ok(SIHTTP_METHOD_GET, "/entities", NULL);

    char path[64];
    snprintf(path, sizeof(path), "/entities/%u", b_index);
    rest_expect_ok(SIHTTP_METHOD_GET, path, NULL);
    snprintf(path, sizeof(path), "/entities/%u/children", a_index);
    rest_expect_ok(SIHTTP_METHOD_GET, path, NULL);

    ecs_entity_t a = ecs_entity_from_index(a_index);
    ecs_entity_t b = ecs_entity_from_index(b_index);
    test_true(ecs_is(b, a));
    test_uint(a, ecs_target(b, ChildOf));
    ecs_fini();
}

void rest_enum_schema_and_component_roundtrip(void) {
    ecs_init();

    sireflect_handle_t enum_type = sireflect(RestMode);
    ECS_COMPONENT_REGISTER(RestEnumComponent);
    ECS_MODULE_IMPORT(sirest, { .in_process = true });

    sihttp_response_t schema_response = sirest_dispatch(SIHTTP_METHOD_GET, "/schema", NULL);

    test_int(200, schema_response.status);
    test_true(schema_response.body != NULL);

    sijson_value_t schema = sijson_parse(schema_response.body);
    test_true(schema != NULL);

    sijson_value_t type = rest_schema_type(schema, enum_type);
    test_true(type != NULL);

    sijson_value_t editor = sijson_object_get(type, "editor");
    test_true(editor != NULL);
    test_true(strcmp(sijson_string(editor), "enum") == 0);

    sijson_value_t options = sijson_object_get(type, "options");
    test_true(options != NULL);
    test_uint(3, sijson_array_len(options));

    test_true(strcmp(sijson_string(sijson_array_get(options, 0)), "REST_MODE_IDLE") == 0);
    test_true(strcmp(sijson_string(sijson_array_get(options, 1)), "REST_MODE_RUN") == 0);
    test_true(strcmp(sijson_string(sijson_array_get(options, 2)), "REST_MODE_PAUSED") == 0);

    sihttp_response_fini(&schema_response);

    uint32_t entity_index = rest_create_entity();

    char path[96];
    snprintf(
        path,
        sizeof(path),
        "/entities/%u/components/%u",
        entity_index,
        ecs_id(RestEnumComponent)
    );

    sihttp_response_t component_response =
        sirest_dispatch(SIHTTP_METHOD_POST, path, "{\"value\":{\"mode\":\"REST_MODE_RUN\"}}");

    test_int(201, component_response.status);
    test_true(component_response.body != NULL);

    sijson_value_t component_body = sijson_parse(component_response.body);
    test_true(component_body != NULL);

    sijson_value_t component_value = sijson_object_get(component_body, "value");
    test_true(component_value != NULL);

    sijson_value_t mode = sijson_object_get(component_value, "mode");
    test_true(mode != NULL);
    test_true(strcmp(sijson_string(mode), "REST_MODE_RUN") == 0);

    sihttp_response_fini(&component_response);

    ecs_fini();
}

void rest_is_a_and_child_of_same_target_routes(void) {
    rest_is_a_child_of_same_target_case(true);
    rest_is_a_child_of_same_target_case(false);
}
