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

void rest_disabled_runtime_does_not_create_server(void) {
    ecs_init();

    test_assert(ecs_world.server == NULL);

    ecs_fini();
}

void rest_schema_returns_editor_contract(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestPosition);

    sihttp_request_t req = {};
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

    test_assert(rest_find_by_name(components, "IsA") == NULL);
    sijson_value_t child_of = rest_find_by_name(components, "ChildOf");
    test_assert(child_of != NULL);
    test_true(sijson_bool(sijson_object_get(child_of, "isRelation")));
    rest_assert_no_extra_schema_fields(schema);

    free(res.body);
    ecs_fini();
}

void rest_entity_detail_returns_editor_state(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestPosition);

    ecs_entity_t parent = ecs_new();
    char *parent_name = strdup("Parent");
    ecs_set(parent, Name, { parent_name });
    free(parent_name);
    ecs_set(parent, RestPosition, { 10.0f, 20.0f });

    ecs_entity_t child = ecs_new();
    char *child_name = strdup("Child");
    ecs_set(child, Name, { child_name });
    free(child_name);
    ecs_set(child, ChildOf, { parent });

    sijson_clean();
    sijson_value_t detail = ecs_rest_entity_detail_json(parent);

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
    sijson_value_t child_detail = ecs_rest_entity_detail_json(child);
    sijson_value_t parent_summary = sijson_object_get(child_detail, "parent");
    test_assert(parent_summary != NULL);
    test_str("Parent", sijson_string(sijson_object_get(parent_summary, "name")));
    test_null(sijson_object_get(parent_summary, "id"));
    test_true(sijson_bool(sijson_object_get(parent_summary, "hasChildren")));

    ecs_fini();
}

void rest_entity_children_returns_direct_children(void) {
    ecs_init();

    ecs_entity_t parent = ecs_new();
    char *parent_name = strdup("Parent");
    ecs_set(parent, Name, { parent_name });
    free(parent_name);

    ecs_entity_t child = ecs_new();
    char *child_name = strdup("Child");
    ecs_set(child, Name, { child_name });
    free(child_name);
    ecs_set(child, ChildOf, { parent });

    ecs_entity_t grandchild = ecs_new();
    char *grandchild_name = strdup("Grandchild");
    ecs_set(grandchild, Name, { grandchild_name });
    free(grandchild_name);
    ecs_set(grandchild, ChildOf, { child });

    sijson_clean();
    sijson_value_t parent_summary = ecs_rest_entity_json(parent);
    test_true(sijson_bool(sijson_object_get(parent_summary, "hasChildren")));

    sijson_clean();
    sijson_value_t children = ecs_rest_entity_children_json(parent);
    test_int(1, (int)sijson_array_len(children));
    sijson_value_t child_summary = rest_find_by_index(children, ecs_first(child));
    test_assert(child_summary != NULL);
    test_str("Child", sijson_string(sijson_object_get(child_summary, "name")));
    test_true(sijson_bool(sijson_object_get(child_summary, "hasChildren")));
    test_assert(rest_find_by_index(children, ecs_first(grandchild)) == NULL);

    sijson_clean();
    sijson_value_t grandchild_summary = ecs_rest_entity_json(grandchild);
    test_false(sijson_bool(sijson_object_get(grandchild_summary, "hasChildren")));

    ecs_fini();
}

void rest_set_component_value_updates_entity(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestPosition);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, RestPosition, { 1.0f, 2.0f });

    sihttp_response_t res = ecs_rest_set_entity_component(
        entity,
        ecs_id(RestPosition),
        "{\"value\":{\"x\":30,\"y\":40}}"
    );

    test_int(200, res.status);
    RestPosition *position = ecs_get(entity, RestPosition);
    test_int(30, (int)position->x);
    test_int(40, (int)position->y);

    sijson_value_t body = sijson_parse(res.body);
    test_assert(body != NULL);
    test_int((int)ecs_id(RestPosition), (int)sijson_number(sijson_object_get(body, "id")));
    test_str("RestPosition", sijson_string(sijson_object_get(body, "name")));

    sijson_value_t value = sijson_object_get(body, "value");
    test_int(30, (int)sijson_number(sijson_object_get(value, "x")));
    test_int(40, (int)sijson_number(sijson_object_get(value, "y")));

    free(res.body);
    ecs_fini();
}

void rest_set_component_value_rejects_missing_field(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestPosition);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, RestPosition, { 1.0f, 2.0f });

    sihttp_response_t res =
        ecs_rest_set_entity_component(entity, ecs_id(RestPosition), "{\"value\":{\"x\":30}}");

    test_int(400, res.status);
    RestPosition *position = ecs_get(entity, RestPosition);
    test_int(1, (int)position->x);
    test_int(2, (int)position->y);

    free(res.body);
    ecs_fini();
}

void rest_set_component_value_rejects_unknown_field(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestPosition);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, RestPosition, { 1.0f, 2.0f });

    sihttp_response_t res = ecs_rest_set_entity_component(
        entity,
        ecs_id(RestPosition),
        "{\"value\":{\"x\":30,\"y\":40,\"z\":50}}"
    );

    test_int(400, res.status);
    RestPosition *position = ecs_get(entity, RestPosition);
    test_int(1, (int)position->x);
    test_int(2, (int)position->y);

    free(res.body);
    ecs_fini();
}

void rest_set_component_value_rejects_wrong_type(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestPosition);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, RestPosition, { 1.0f, 2.0f });

    sihttp_response_t res = ecs_rest_set_entity_component(
        entity,
        ecs_id(RestPosition),
        "{\"value\":{\"x\":\"bad\",\"y\":40}}"
    );

    test_int(400, res.status);
    RestPosition *position = ecs_get(entity, RestPosition);
    test_int(1, (int)position->x);
    test_int(2, (int)position->y);

    free(res.body);
    ecs_fini();
}

void rest_set_component_value_rejects_missing_component(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RestPosition);

    ecs_entity_t entity = ecs_new();

    sihttp_response_t res = ecs_rest_set_entity_component(
        entity,
        ecs_id(RestPosition),
        "{\"value\":{\"x\":30,\"y\":40}}"
    );

    test_int(404, res.status);

    free(res.body);
    ecs_fini();
}

void rest_set_component_value_rejects_non_reflected_component(void) {
    ecs_init();
    ecs_component_t opaque = ecs_component({ .name = "RestOpaque", .size = sizeof(int) });

    ecs_entity_t entity = ecs_new();
    int value = 7;
    ecs_set_cid(entity, opaque, &value);

    sihttp_response_t res = ecs_rest_set_entity_component(entity, opaque, "{\"value\":10}");

    test_int(404, res.status);
    test_int(7, *(int *)ecs_get_cid(entity, opaque));

    free(res.body);
    ecs_fini();
}
