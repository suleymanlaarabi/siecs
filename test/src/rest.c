#include "siecs.h"
#include "siecs_rest.h"
#include "sijson.h"
#include <siecs_test.h>
#include <stdint.h>
#include <stdio.h>

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

void rest_is_a_and_child_of_same_target_routes(void) {
    rest_is_a_child_of_same_target_case(true);
    rest_is_a_child_of_same_target_case(false);
}
