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

static sijson_value_t rest_find_by_index(sijson_value_t array, double index) {
    for (size_t i = 0; i < sijson_array_len(array); i++) {
        sijson_value_t item = sijson_array_get(array, i);
        sijson_value_t item_index = sijson_object_get(item, "index");
        if (item_index && sijson_number(item_index) == index) {
            return item;
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

void rest_entity_detail_returns_editor_state(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, RestPosition);

    ecs_entity_t parent = ecs_new(world);
    ecs_set(world, parent, Name, { strdup("Parent") });
    ecs_set(world, parent, RestPosition, { 10.0f, 20.0f });

    ecs_entity_t child = ecs_new(world);
    ecs_set(world, child, Name, { strdup("Child") });
    ecs_set(world, child, ChildOf, { parent });

    sijson_clean();
    sijson_value_t detail = ecs_rest_entity_detail_json(world, parent);

    test_str("Parent", sijson_string(sijson_object_get(detail, "name")));
    test_null(sijson_object_get(detail, "id"));
    test_int((int)ecs_first(parent), (int)sijson_number(sijson_object_get(detail, "index")));
    test_int((int)ecs_second(parent), (int)sijson_number(sijson_object_get(detail, "generation")));

    sijson_value_t children = sijson_object_get(detail, "children");
    test_int(1, (int)sijson_array_len(children));
    sijson_value_t child_summary = sijson_array_get(children, 0);
    test_str("Child", sijson_string(sijson_object_get(child_summary, "name")));
    test_null(sijson_object_get(child_summary, "id"));
    test_false(sijson_bool(sijson_object_get(child_summary, "hasChildren")));

    sijson_value_t components = sijson_object_get(detail, "components");
    sijson_value_t position = rest_find_by_name(components, "RestPosition");
    test_assert(position != NULL);
    test_int((int)ecs_id(RestPosition), (int)sijson_number(sijson_object_get(position, "id")));

    sijson_value_t position_value = sijson_object_get(position, "value");
    test_int(10, (int)sijson_number(sijson_object_get(position_value, "x")));
    test_int(20, (int)sijson_number(sijson_object_get(position_value, "y")));

    sijson_value_t name = rest_find_by_name(components, "Name");
    test_assert(name != NULL);
    sijson_value_t name_value = sijson_object_get(name, "value");
    test_str("Parent", sijson_string(sijson_object_get(name_value, "value")));

    test_assert(rest_find_by_name(components, "ChildOf") == NULL);

    sijson_clean();
    sijson_value_t child_detail = ecs_rest_entity_detail_json(world, child);
    sijson_value_t parent_summary = sijson_object_get(child_detail, "parent");
    test_assert(parent_summary != NULL);
    test_str("Parent", sijson_string(sijson_object_get(parent_summary, "name")));
    test_null(sijson_object_get(parent_summary, "id"));
    test_true(sijson_bool(sijson_object_get(parent_summary, "hasChildren")));

    free(ecs_get(world, parent, Name)->value);
    free(ecs_get(world, child, Name)->value);
    ecs_fini(world);
}

void rest_entity_children_returns_direct_children(void) {
    ecs_world_t *world = ecs_init();

    ecs_entity_t parent = ecs_new(world);
    ecs_set(world, parent, Name, { strdup("Parent") });

    ecs_entity_t child = ecs_new(world);
    ecs_set(world, child, Name, { strdup("Child") });
    ecs_set(world, child, ChildOf, { parent });

    ecs_entity_t grandchild = ecs_new(world);
    ecs_set(world, grandchild, Name, { strdup("Grandchild") });
    ecs_set(world, grandchild, ChildOf, { child });

    sijson_clean();
    sijson_value_t parent_summary = ecs_rest_entity_json(world, parent);
    test_true(sijson_bool(sijson_object_get(parent_summary, "hasChildren")));

    sijson_clean();
    sijson_value_t children = ecs_rest_entity_children_json(world, parent);
    test_int(1, (int)sijson_array_len(children));
    sijson_value_t child_summary = rest_find_by_index(children, ecs_first(child));
    test_assert(child_summary != NULL);
    test_str("Child", sijson_string(sijson_object_get(child_summary, "name")));
    test_true(sijson_bool(sijson_object_get(child_summary, "hasChildren")));
    test_assert(rest_find_by_index(children, ecs_first(grandchild)) == NULL);

    sijson_clean();
    sijson_value_t grandchild_summary = ecs_rest_entity_json(world, grandchild);
    test_false(sijson_bool(sijson_object_get(grandchild_summary, "hasChildren")));

    free(ecs_get(world, parent, Name)->value);
    free(ecs_get(world, child, Name)->value);
    free(ecs_get(world, grandchild, Name)->value);
    ecs_fini(world);
}
