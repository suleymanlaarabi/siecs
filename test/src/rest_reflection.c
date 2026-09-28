#include <siecs.h>
#include <siecs/reflect_internal.h>
#include <siecs_rest.h>
#include <siecs_test.h>
#include <sijson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SIREFLECT_ENUM(RestChoice, { REST_CHOICE_LOW = -2, REST_CHOICE_HIGH = 5 });
SIREFLECT_STRUCT(RestPoint, {
    float x;
    float y;
});
SIREFLECT_STRUCT(RestFakeEntity, {
    uint32_t id;
    uint32_t generation;
});
ECS_COMPONENT_DECLARE(RestComplex, {
    RestPoint point;
    RestChoice choice;
    const char *label;
    int levels[2];
    int locked;
});
ECS_RESOURCE_DECLARE(RestSettings, {
    RestPoint point;
    RestChoice choice;
    const char *label;
    int levels[2];
    int locked;
});
ECS_COMPONENT(RestMatrix, { float matrix[2][2]; });
ECS_COMPONENT(RestPointer, { int *pointer; });

static int rest_complex_sets;
static int rest_settings_sets;
static float rest_complex_last_x;
static float rest_settings_last_x;

static char *rest_strdup(const char *src)
{
    if (!src)
        return NULL;

    size_t size = strlen(src) + 1;
    char *dst = malloc(size);

    if (!dst)
        abort();

    memcpy(dst, src, size);
    return dst;
}

static void rest_complex_dtor(void *ptr, uint32_t count)
{
    RestComplex *values = ptr;

    for (uint32_t i = 0; i < count; ++i) {
        free((void *)values[i].label);
        values[i].label = NULL;
    }
}

static void rest_complex_copy_ctor(
    void *dst,
    const void *src,
    uint32_t count)
{
    RestComplex *out = dst;
    const RestComplex *in = src;

    for (uint32_t i = 0; i < count; ++i) {
        out[i] = in[i];
        out[i].label = rest_strdup(in[i].label);
    }
}

static void rest_complex_copy(
    void *dst,
    const void *src,
    uint32_t count)
{
    RestComplex *out = dst;
    const RestComplex *in = src;

    if (out == in)
        return;

    for (uint32_t i = 0; i < count; ++i) {
        char *label = rest_strdup(in[i].label);

        free((void *)out[i].label);

        out[i] = in[i];
        out[i].label = label;
    }
}

static void rest_complex_move_ctor(
    void *dst,
    void *src,
    uint32_t count)
{
    RestComplex *out = dst;
    RestComplex *in = src;

    for (uint32_t i = 0; i < count; ++i) {
        out[i] = in[i];
        in[i].label = NULL;
    }
}

static void rest_complex_move(
    void *dst,
    void *src,
    uint32_t count)
{
    RestComplex *out = dst;
    RestComplex *in = src;

    if (out == in)
        return;

    for (uint32_t i = 0; i < count; ++i) {
        free((void *)out[i].label);

        out[i] = in[i];
        in[i].label = NULL;
    }
}

static void rest_settings_dtor(void *ptr, uint32_t count)
{
    RestSettings *values = ptr;

    for (uint32_t i = 0; i < count; ++i) {
        free((void *)values[i].label);
        values[i].label = NULL;
    }
}

static void rest_settings_copy_ctor(
    void *dst,
    const void *src,
    uint32_t count)
{
    RestSettings *out = dst;
    const RestSettings *in = src;

    for (uint32_t i = 0; i < count; ++i) {
        out[i] = in[i];
        out[i].label = rest_strdup(in[i].label);
    }
}

static void rest_settings_copy(
    void *dst,
    const void *src,
    uint32_t count)
{
    RestSettings *out = dst;
    const RestSettings *in = src;

    if (out == in)
        return;

    for (uint32_t i = 0; i < count; ++i) {
        char *label = rest_strdup(in[i].label);

        free((void *)out[i].label);

        out[i] = in[i];
        out[i].label = label;
    }
}

static void rest_settings_move_ctor(
    void *dst,
    void *src,
    uint32_t count)
{
    RestSettings *out = dst;
    RestSettings *in = src;

    for (uint32_t i = 0; i < count; ++i) {
        out[i] = in[i];
        in[i].label = NULL;
    }
}

static void rest_settings_move(
    void *dst,
    void *src,
    uint32_t count)
{
    RestSettings *out = dst;
    RestSettings *in = src;

    if (out == in)
        return;

    for (uint32_t i = 0; i < count; ++i) {
        free((void *)out[i].label);

        out[i] = in[i];
        in[i].label = NULL;
    }
}

static void rest_complex_on_set(
    ecs_entity_t entity,
    ecs_component_t component,
    const void *next,
    void *current
) {
    (void)entity;
    (void)component;
    (void)current;
    rest_complex_sets++;
    rest_complex_last_x = ((const RestComplex *)next)->point.x;
}
static void rest_settings_on_set(const void *next) {
    rest_settings_sets++;
    rest_settings_last_x = ((const RestSettings *)next)->point.x;
}
ECS_COMPONENT_DEFINE(
    RestComplex,
    .ops = {
        .dtor = rest_complex_dtor,
        .copy_ctor = rest_complex_copy_ctor,
        .copy = rest_complex_copy,
        .move_ctor = rest_complex_move_ctor,
        .move = rest_complex_move,
    },
    .on_set = rest_complex_on_set
);
ECS_RESOURCE_DEFINE(
    RestSettings,
    .ops = {
        .dtor = rest_settings_dtor,
        .copy_ctor = rest_settings_copy_ctor,
        .copy = rest_settings_copy,
        .move_ctor = rest_settings_move_ctor,
        .move = rest_settings_move,
    },
    .on_set = rest_settings_on_set
);

static sijson_value_t reflected_type(sijson_value_t schema, sireflect_handle_t handle) {
    sijson_value_t types = sijson_object_get(schema, "types");
    for (size_t i = 0; i < sijson_array_len(types); i++) {
        sijson_value_t type = sijson_array_get(types, i);
        if ((sireflect_handle_t)sijson_number(sijson_object_get(type, "id")) == handle)
            return type;
    }
    return NULL;
}

static uint32_t create_entity(void) {
    sihttp_response_t response = sirest_dispatch(SIHTTP_METHOD_POST, "/entities", NULL);
    test_int(200, response.status);
    test_not_null((void *)sihttp_response_header(&response, "Location"));
    sijson_value_t json = sijson_parse(response.body);
    uint32_t index = (uint32_t)sijson_number(sijson_object_get(json, "index"));
    sihttp_response_fini(&response);
    return index;
}

static void assert_error(
    sihttp_method_t method,
    const char *path,
    const char *body,
    int status,
    const char *code
) {
    sihttp_response_t response = sirest_dispatch(method, path, body);
    test_int(status, response.status);
    sijson_value_t json = sijson_parse(response.body);
    test_not_null((void *)json);
    sijson_value_t error = sijson_object_get(json, "error");
    test_not_null((void *)error);
    test_str(code, sijson_string(sijson_object_get(error, "code")));
    test_not_null((void *)sijson_object_get(error, "message"));
    sihttp_response_fini(&response);
}

void rest_reflection_reflected_schema_graph_and_roles(void) {
    ecs_init();
    sireflect_handle_t point = sireflect(RestPoint);
    sireflect_handle_t choice = sireflect(RestChoice);
    sireflect_handle_t fake = sireflect(RestFakeEntity);
    ECS_COMPONENT_REGISTER(RestComplex);
    ECS_COMPONENT_REGISTER(RestMatrix, RestPointer);
    ECS_RESOURCE_REGISTER(RestSettings);
    sireflect_field_set_meta(
        ecs_component_info(ecs_id(RestComplex))->type,
        "point",
        &(sireflect_meta_t){ .key = "hint",
                             .kind = SIREFLECT_META_STRING,
                             .value.string = "position" }
    );
    sireflect_type_set_meta(
        point,
        &(sireflect_meta_t){ .key = "unit",
                             .kind = SIREFLECT_META_STRING,
                             .value.string = "meters" }
    );
    ECS_MODULE_IMPORT(sirest, { .in_process = true });

    sireflect_handle_t entity = sireflect_type_by_name("ecs_entity_t");
    test_true(ecs_reflect_type_has_role(entity, "entity"));
    test_false(ecs_reflect_type_has_role(fake, "entity"));
    sireflect_type_set_meta(
        fake,
        &(sireflect_meta_t){ .key = "siecs.role",
                             .kind = SIREFLECT_META_STRING,
                             .value.string = "entity" }
    );
    test_int(EcsReflectValueEntity, ecs_reflect_value_kind(fake));

    sihttp_response_t response = sirest_dispatch(SIHTTP_METHOD_GET, "/schema", NULL);
    test_int(200, response.status);
    sijson_value_t schema = sijson_parse(response.body);
    test_not_null((void *)reflected_type(schema, point));
    sijson_value_t enum_type = reflected_type(schema, choice);
    test_not_null((void *)enum_type);
    sijson_value_t values = sijson_object_get(enum_type, "values");
    test_uint(2, sijson_array_len(values));
    test_int(-2, sijson_number(sijson_object_get(sijson_array_get(values, 0), "value")));
    test_int(5, sijson_number(sijson_object_get(sijson_array_get(values, 1), "value")));
    sireflect_handle_t component_type = ecs_component_info(ecs_id(RestComplex))->type;
    const sireflect_field_info_t *array = sireflect_field_info(component_type, "levels");
    test_not_null((void *)array);
    sijson_value_t array_type = reflected_type(schema, array->type);
    test_not_null((void *)array_type);
    test_str("array", sijson_string(sijson_object_get(array_type, "category")));
    test_uint(2, sijson_number(sijson_object_get(array_type, "count")));
    sireflect_handle_t string_type = sireflect_field_type(component_type, "label");
    test_str(
        "string",
        sijson_string(sijson_object_get(reflected_type(schema, string_type), "category"))
    );
    sijson_value_t point_type = reflected_type(schema, point);
    test_str(
        "meters",
        sijson_string(sijson_object_get(sijson_object_get(point_type, "meta"), "unit"))
    );
    sijson_value_t complex_type = reflected_type(schema, component_type);
    sijson_value_t fields = sijson_object_get(complex_type, "fields");
    bool found_hint = false;
    for (size_t i = 0; i < sijson_array_len(fields); i++) {
        sijson_value_t field = sijson_array_get(fields, i);
        if (strcmp(sijson_string(sijson_object_get(field, "name")), "point") == 0) {
            found_hint =
                strcmp(
                    sijson_string(sijson_object_get(sijson_object_get(field, "meta"), "hint")),
                    "position"
                ) == 0;
        }
    }
    test_true(found_hint);
    sireflect_handle_t matrix_type =
        sireflect_field_type(ecs_component_info(ecs_id(RestMatrix))->type, "matrix");
    sireflect_handle_t matrix_row = sireflect_type_element(matrix_type);
    test_not_null((void *)reflected_type(schema, matrix_type));
    test_not_null((void *)reflected_type(schema, matrix_row));
    sireflect_handle_t pointer_type =
        sireflect_field_type(ecs_component_info(ecs_id(RestPointer))->type, "pointer");
    sijson_value_t pointer_schema = reflected_type(schema, pointer_type);
    test_str("pointer", sijson_string(sijson_object_get(pointer_schema, "category")));
    test_false(sijson_bool(sijson_object_get(pointer_schema, "editable")));
    sijson_value_t resources = sijson_object_get(schema, "resources");
    bool found = false;
    for (size_t i = 0; i < sijson_array_len(resources); i++)
        found |= (ecs_resource_t)sijson_number(
                     sijson_object_get(sijson_array_get(resources, i), "id")
                 ) == ecs_id(RestSettings);
    test_true(found);
    sihttp_response_fini(&response);
    ecs_fini();
}

void rest_reflection_reflected_patch_and_resources(void) {
    ecs_init();
    sireflect(RestPoint);
    sireflect(RestChoice);
    ECS_COMPONENT_REGISTER(RestComplex);
    ECS_RESOURCE_REGISTER(RestSettings);
    sireflect_field_set_meta(
        ecs_component_info(ecs_id(RestComplex))->type,
        "locked",
        &(sireflect_meta_t){ .key = "readonly", .kind = SIREFLECT_META_BOOL, .value.boolean = true }
    );
    sireflect_field_set_meta(
        ecs_resource_info(ecs_id(RestSettings))->type,
        "locked",
        &(sireflect_meta_t){ .key = "readonly", .kind = SIREFLECT_META_BOOL, .value.boolean = true }
    );
    rest_complex_sets = rest_settings_sets = 0;
    ECS_MODULE_IMPORT(sirest, { .in_process = true });
    uint32_t index = create_entity();
    ecs_entity_t entity = ecs_entity_from_index(index);
    ecs_set(
        entity,
        RestComplex,
        { .point = { 1, 2 },
          .choice = REST_CHOICE_LOW,
          .label = "before",
          .levels = { 3, 4 },
          .locked = 9 }
    );
    ecs_set_resource(
        RestSettings,
        { .point = { 1, 2 },
          .choice = REST_CHOICE_LOW,
          .label = "before",
          .levels = { 3, 4 },
          .locked = 9 }
    );
    rest_complex_sets = rest_settings_sets = 0;
    char component_path[100];
    char resource_path[100];
    snprintf(
        component_path,
        sizeof component_path,
        "/entities/%u/components/%u",
        index,
        ecs_id(RestComplex)
    );
    snprintf(resource_path, sizeof resource_path, "/resources/%u", ecs_id(RestSettings));

    sihttp_response_t response = sirest_dispatch(
        SIHTTP_METHOD_PATCH,
        component_path,
        "{\"value\":{\"point\":{\"x\":42},\"choice\":\"REST_CHOICE_HIGH\"}}"
    );
    test_int(200, response.status);
    sihttp_response_fini(&response);
    test_int(1, rest_complex_sets);
    test_int(42, rest_complex_last_x);
    test_int(42, ecs_get(entity, RestComplex)->point.x);
    test_int(2, ecs_get(entity, RestComplex)->point.y);
    test_int(REST_CHOICE_HIGH, ecs_get(entity, RestComplex)->choice);
    assert_error(
        SIHTTP_METHOD_PATCH,
        component_path,
        "{\"value\":{\"point\":{\"x\":77},\"unknown\":1}}",
        400,
        "invalid_component_value"
    );
    assert_error(
        SIHTTP_METHOD_PATCH,
        component_path,
        "{\"value\":{\"choice\":\"NO_SUCH_VALUE\"}}",
        400,
        "invalid_component_value"
    );
    assert_error(
        SIHTTP_METHOD_PATCH,
        component_path,
        "{\"value\":{\"locked\":10}}",
        400,
        "invalid_component_value"
    );
    test_int(1, rest_complex_sets);
    test_int(42, ecs_get(entity, RestComplex)->point.x);
    assert_error(
        SIHTTP_METHOD_PATCH,
        component_path,
        "{\"value\":{\"levels\":[1]}}",
        400,
        "invalid_component_value"
    );
    response =
        sirest_dispatch(SIHTTP_METHOD_PUT, component_path, "{\"value\":{\"point\":{\"x\":8}}}");
    test_int(400, response.status);
    sihttp_response_fini(&response);
    response = sirest_dispatch(
        SIHTTP_METHOD_PUT,
        component_path,
        "{\"value\":{\"point\":{\"x\":8,\"y\":6},\"choice\":\"REST_CHOICE_LOW\","
        "\"label\":null,\"levels\":[1,2],\"locked\":9}}"
    );
    test_int(200, response.status);
    sihttp_response_fini(&response);
    test_int(2, rest_complex_sets);
    test_int(8, ecs_get(entity, RestComplex)->point.x);
    test_null((void *)ecs_get(entity, RestComplex)->label);

    response = sirest_dispatch(SIHTTP_METHOD_GET, "/resources", NULL);
    test_int(200, response.status);
    sijson_value_t listing = sijson_parse(response.body);
    test_true(sijson_array_len(listing) > 0);
    sihttp_response_fini(&response);
    response = sirest_dispatch(SIHTTP_METHOD_GET, resource_path, NULL);
    test_int(200, response.status);
    sihttp_response_fini(&response);
    response = sirest_dispatch(
        SIHTTP_METHOD_PATCH,
        resource_path,
        "{\"value\":{\"point\":{\"x\":13},\"levels\":[8,9]}}"
    );
    test_int(200, response.status);
    sihttp_response_fini(&response);
    test_int(1, rest_settings_sets);
    test_int(13, rest_settings_last_x);
    test_int(13, ecs_get_resource(RestSettings)->point.x);
    test_int(9, ecs_get_resource(RestSettings)->levels[1]);
    response = sirest_dispatch(
        SIHTTP_METHOD_PUT,
        resource_path,
        "{\"value\":{\"point\":{\"x\":17,\"y\":3},\"choice\":\"REST_CHOICE_HIGH\","
        "\"label\":\"updated\",\"levels\":[5,6],\"locked\":9}}"
    );
    test_int(200, response.status);
    sihttp_response_fini(&response);
    test_int(2, rest_settings_sets);
    test_int(17, ecs_get_resource(RestSettings)->point.x);
    test_str("updated", ecs_get_resource(RestSettings)->label);
    assert_error(
        SIHTTP_METHOD_PATCH,
        resource_path,
        "{\"value\":{\"locked\":11}}",
        400,
        "invalid_resource_value"
    );
    assert_error(
        SIHTTP_METHOD_PATCH,
        "/resources/65535",
        "{\"value\":{}}",
        404,
        "resource_not_found"
    );
    test_int(2, rest_settings_sets);
    ecs_remove_resource(RestSettings);
    assert_error(SIHTTP_METHOD_GET, resource_path, NULL, 404, "resource_not_found");
    ecs_fini();
}
