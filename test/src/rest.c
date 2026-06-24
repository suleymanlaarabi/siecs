#include "addons/rest_internal.h"
#include "siecs.h"
#include "sijson.h"
#include "world_internal.h"
#include <siecs_test.h>
#include <stdlib.h>
#include <string.h>

ECS_COMPONENT_DECLARE(RestPosition, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(RestPosition);

static sijson_value_t rest_find_by_name(sijson_value_t array, const char *name) {
    for (size_t i = 0; i < sijson_array_len(array); i++) {
        sijson_value_t item = sijson_array_get(array, i);
        sijson_value_t item_name = sijson_object_get(item, "name");
        if (item_name && strcmp(sijson_string(item_name), name) == 0) {
            return item;
        }
    }

    return NULL;
}

static sijson_value_t rest_find_type(sijson_value_t types, double id) {
    for (size_t i = 0; i < sijson_array_len(types); i++) {
        sijson_value_t type = sijson_array_get(types, i);
        sijson_value_t type_id = sijson_object_get(type, "id");
        if (type_id && sijson_number(type_id) == id) {
            return type;
        }
    }

    return NULL;
}

static void rest_assert_no_extra_schema_fields(sijson_value_t schema) {
    sijson_value_t components = sijson_object_get(schema, "components");
    sijson_value_t types = sijson_object_get(schema, "types");

    for (size_t i = 0; i < sijson_array_len(components); i++) {
        sijson_value_t component = sijson_array_get(components, i);
        test_int(5, (int)sijson_object_len(component));

        sijson_value_t fields = sijson_object_get(component, "fields");
        for (size_t f = 0; f < sijson_array_len(fields); f++) {
            test_int(2, (int)sijson_object_len(sijson_array_get(fields, f)));
        }
    }

    for (size_t i = 0; i < sijson_array_len(types); i++) {
        test_int(3, (int)sijson_object_len(sijson_array_get(types, i)));
    }
}

void rest_schema_returns_editor_contract(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, RestPosition);

    sihttp_app_state_t state = { .world = world };
    sihttp_request_t req = { .state = &state };
    sihttp_response_t res = ecs_rest_get_schema(&req);

    test_int(200, res.status);

    sijson_value_t schema = sijson_parse(res.body);
    test_assert(schema != NULL);
    test_int(2, (int)sijson_object_len(schema));

    sijson_value_t components = sijson_object_get(schema, "components");
    sijson_value_t types = sijson_object_get(schema, "types");
    test_assert(components != NULL);
    test_assert(types != NULL);

    sijson_value_t position = rest_find_by_name(components, "RestPosition");
    test_assert(position != NULL);
    test_false(sijson_bool(sijson_object_get(position, "isRelation")));

    sijson_value_t fields = sijson_object_get(position, "fields");
    test_int(2, (int)sijson_array_len(fields));

    sijson_value_t x_field = rest_find_by_name(fields, "x");
    test_assert(x_field != NULL);

    sijson_value_t float_type =
        rest_find_type(types, sijson_number(sijson_object_get(x_field, "type")));
    test_assert(float_type != NULL);
    test_str("number", sijson_string(sijson_object_get(float_type, "editor")));

    sijson_value_t name = rest_find_by_name(components, "Name");
    test_assert(name != NULL);
    sijson_value_t name_value = rest_find_by_name(sijson_object_get(name, "fields"), "value");
    test_assert(name_value != NULL);

    sijson_value_t string_type =
        rest_find_type(types, sijson_number(sijson_object_get(name_value, "type")));
    test_assert(string_type != NULL);
    test_str("string", sijson_string(sijson_object_get(string_type, "editor")));

    sijson_value_t isa = rest_find_by_name(components, "IsA");
    test_assert(isa != NULL);
    sijson_value_t target = rest_find_by_name(sijson_object_get(isa, "fields"), "target");
    test_assert(target != NULL);

    sijson_value_t entity_type =
        rest_find_type(types, sijson_number(sijson_object_get(target, "type")));
    test_assert(entity_type != NULL);
    test_str("entity", sijson_string(sijson_object_get(entity_type, "editor")));

    test_assert(rest_find_by_name(components, "ChildOf") == NULL);
    rest_assert_no_extra_schema_fields(schema);

    free(res.body);
    ecs_fini(world);
}
