#include "rest_internal.h"
#include <math.h>
#include <siecs/reflect_internal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

sihttp_response_t ecs_rest_error(int status, const char *code, const char *message) {
    sijson_value_t detail = sijson_make_object();
    sijson_object_set(detail, "code", sijson_make_string(code));
    sijson_object_set(detail, "message", sijson_make_string(message));
    sijson_value_t body = sijson_make_object();
    sijson_object_set(body, "error", detail);
    return sihttp_response_json(status, body);
}

bool ecs_rest_entity_component_is_reflected(ecs_component_t component) {
    const ecs_component_info_t *info = ecs_component_info(component);
    return info && info->type != SIREFLECT_INVALID_HANDLE;
}

static bool ecs_rest_readonly(const sireflect_meta_t *meta) {
    return meta && meta->kind == SIREFLECT_META_BOOL && meta->value.boolean;
}

static bool ecs_rest_json_equal(sijson_value_t a, sijson_value_t b) {
    if (!a || !b)
        return a == b;
    char *left = sijson_stringify(a);
    char *right = sijson_stringify(b);
    bool equal = left && right && strcmp(left, right) == 0;
    free(left);
    free(right);
    return equal;
}

static bool ecs_rest_integer_number(double number) {
    if (!isfinite(number))
        return false;
    if (number >= -9223372036854775808.0 && number < 9223372036854775808.0)
        return (double)(int64_t)number == number;
    if (number >= 0 && number < 18446744073709551616.0)
        return (double)(uint64_t)number == number;
    return false;
}

static bool ecs_rest_validate_value(
    sireflect_handle_t type,
    sijson_value_t value,
    sijson_value_t current,
    bool patch
) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info || !value)
        return false;
    if (ecs_rest_readonly(sireflect_type_meta(type, "readonly")))
        return current && ecs_rest_json_equal(value, current);

    switch (ecs_reflect_value_kind(type)) {
    case EcsReflectValueStruct:
    case EcsReflectValueEntity: {
        if (sijson_type(value) != SIJSON_OBJECT)
            return false;
        if (!patch && sijson_object_len(value) != info->fields.field_count)
            return false;
        for (size_t i = 0; i < sijson_object_len(value); i++) {
            const char *name = sijson_object_key(value, i);
            const sireflect_field_info_t *field = sireflect_field_info(type, name);
            if (!field)
                return false;
            sijson_value_t child = sijson_object_get(value, name);
            sijson_value_t previous = current ? sijson_object_get(current, name) : NULL;
            if (ecs_rest_readonly(sireflect_field_meta(type, name, "readonly")) &&
                (!previous || !ecs_rest_json_equal(child, previous)))
                return false;
            if (!ecs_rest_validate_value(field->type, child, previous, patch))
                return false;
        }
        if (!patch) {
            for (size_t i = 0; i < info->fields.field_count; i++)
                if (!sijson_object_get(value, info->fields.fields[i].name))
                    return false;
        }
        return true;
    }
    case EcsReflectValueArray:
        if (sijson_type(value) != SIJSON_ARRAY || sijson_array_len(value) != info->element_count)
            return false;
        for (size_t i = 0; i < info->element_count; i++) {
            sijson_value_t previous = current ? sijson_array_get(current, i) : NULL;
            if (!ecs_rest_validate_value(
                    info->element_type,
                    sijson_array_get(value, i),
                    previous,
                    false
                ))
                return false;
        }
        return true;
    case EcsReflectValueString:
        return sijson_type(value) == SIJSON_STRING || sijson_type(value) == SIJSON_NULL;
    case EcsReflectValueEnum:
        if (sijson_type(value) == SIJSON_STRING)
            return sireflect_enum_value_by_name(type, sijson_string(value)) != NULL;
        return false;
    case EcsReflectValuePointer:
        return false;
    default:
        if (info->kind == sireflect_kind_char)
            return sijson_type(value) == SIJSON_STRING && strlen(sijson_string(value)) == 1;
        switch (sireflect_type_category(type)) {
        case sireflect_category_boolean:
            return sijson_type(value) == SIJSON_BOOL;
        case sireflect_category_integer:
            return sijson_type(value) == SIJSON_NUMBER &&
                   ecs_rest_integer_number(sijson_number(value));
        case sireflect_category_floating:
            return sijson_type(value) == SIJSON_NUMBER && isfinite(sijson_number(value));
        default:
            return false;
        }
    }
}

bool ecs_rest_validate_full_value(
    sireflect_handle_t type,
    sijson_value_t value,
    sijson_value_t current
) {
    return ecs_rest_validate_value(type, value, current, false);
}

bool ecs_rest_validate_patch_value(
    sireflect_handle_t type,
    sijson_value_t value,
    sijson_value_t current
) {
    return ecs_rest_validate_value(type, value, current, true);
}

static void
ecs_rest_merge_patch(sireflect_handle_t type, sijson_value_t current, sijson_value_t patch) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info || info->kind != sireflect_kind_struct)
        return;
    for (size_t i = 0; i < sijson_object_len(patch); i++) {
        const char *name = sijson_object_key(patch, i);
        const sireflect_field_info_t *field = sireflect_field_info(type, name);
        sijson_value_t incoming = sijson_object_get(patch, name);
        sijson_value_t existing = sijson_object_get(current, name);
        if (field && sireflect_type_category(field->type) == sireflect_category_struct &&
            sijson_type(incoming) == SIJSON_OBJECT && sijson_type(existing) == SIJSON_OBJECT)
            ecs_rest_merge_patch(field->type, existing, incoming);
        else
            sijson_object_set(current, name, incoming);
    }
}

static sijson_value_t
ecs_rest_value_json(sireflect_handle_t type, const sireflect_struct_desc_t *desc, const void *ptr) {
    char *json = sijson_to_json_impl(&type, desc, ptr);
    if (!json)
        return NULL;
    sijson_value_t value = sijson_parse(json);
    free(json);
    return value;
}

sijson_value_t ecs_rest_entity_component_json(ecs_component_t component_id, const void *ptr) {
    const ecs_component_info_t *info = ecs_component_info(component_id);
    sijson_value_t component = sijson_make_object();
    sijson_object_set(component, "id", sijson_make_number(component_id));
    sijson_object_set(component, "name", sijson_make_string(info->name));
    sijson_value_t value = ecs_rest_value_json(info->type, info->reflection, ptr);
    sijson_object_set(component, "value", value ? value : sijson_make_null());
    return component;
}

sijson_value_t ecs_rest_resource_json(ecs_resource_t resource_id, const void *ptr) {
    const ecs_resource_info_t *info = ecs_resource_info(resource_id);
    sijson_value_t resource = sijson_make_object();
    sijson_object_set(resource, "id", sijson_make_number(resource_id));
    sijson_object_set(resource, "name", sijson_make_string(info->name));
    sijson_value_t value = ecs_rest_value_json(info->type, info->reflection, ptr);
    sijson_object_set(resource, "value", value ? value : sijson_make_null());
    return resource;
}

static bool ecs_rest_decode_value(
    sireflect_handle_t type,
    const sireflect_struct_desc_t *desc,
    sijson_value_t value,
    void **decoded
) {
    char *json = sijson_stringify(value);
    *decoded = json ? sijson_from_json_impl(&type, desc, json) : NULL;
    free(json);
    return *decoded && !sijson_error();
}

bool ecs_rest_decode_component_value(
    ecs_component_t component,
    sijson_value_t value,
    void **decoded
) {
    const ecs_component_info_t *info = ecs_component_info(component);
    if (!info || !ecs_rest_validate_full_value(info->type, value, NULL))
        return false;
    return ecs_rest_decode_value(info->type, info->reflection, value, decoded);
}

static sijson_value_t ecs_rest_body_value(const char *body_text) {
    sijson_value_t body = body_text ? sijson_parse(body_text) : NULL;
    if (!body || sijson_type(body) != SIJSON_OBJECT || sijson_object_len(body) != 1)
        return NULL;
    return sijson_object_get(body, "value");
}

sihttp_response_t ecs_rest_set_entity_component(
    ecs_entity_t entity,
    ecs_component_t component,
    const char *body_text,
    bool patch
) {
    sijson_clean();
    if (!ecs_is_alive(entity))
        return ecs_rest_error(404, "entity_not_found", "entity not found");
    const ecs_component_info_t *info = ecs_component_info(component);
    if (!info || info->type == SIREFLECT_INVALID_HANDLE)
        return ecs_rest_error(404, "component_not_found", "component not found");
    if (!ecs_has_cid_owned(entity, component))
        return ecs_rest_error(404, "entity_component_not_found", "entity component not found");
    sijson_value_t value = ecs_rest_body_value(body_text);
    if (!value)
        return ecs_rest_error(400, "invalid_json_body", "invalid json body");
    sijson_value_t current =
        ecs_rest_value_json(info->type, info->reflection, ecs_get_cid(entity, component));
    bool valid = patch ? ecs_rest_validate_patch_value(info->type, value, current)
                       : ecs_rest_validate_full_value(info->type, value, current);
    if (!valid)
        return ecs_rest_error(400, "invalid_component_value", "invalid component value");
    if (patch) {
        ecs_rest_merge_patch(info->type, current, value);
        value = current;
    }
    void *decoded = NULL;
    if (!ecs_rest_decode_value(info->type, info->reflection, value, &decoded))
        return ecs_rest_error(400, "invalid_component_value", "invalid component value");
    ecs_move_cid(entity, component, decoded);
    return sihttp_response_json(
        200,
        ecs_rest_entity_component_json(component, ecs_get_cid(entity, component))
    );
}

sihttp_response_t
ecs_rest_set_resource(ecs_resource_t resource, const char *body_text, bool patch) {
    sijson_clean();
    const ecs_resource_info_t *info = ecs_resource_info(resource);
    if (!info || info->type == SIREFLECT_INVALID_HANDLE || !ecs_has_resource_rid(resource))
        return ecs_rest_error(404, "resource_not_found", "resource not found");
    sijson_value_t value = ecs_rest_body_value(body_text);
    if (!value)
        return ecs_rest_error(400, "invalid_json_body", "invalid json body");
    sijson_value_t current =
        ecs_rest_value_json(info->type, info->reflection, ecs_resource_rid(resource));
    bool valid = patch ? ecs_rest_validate_patch_value(info->type, value, current)
                       : ecs_rest_validate_full_value(info->type, value, current);
    if (!valid)
        return ecs_rest_error(400, "invalid_resource_value", "invalid resource value");
    if (patch) {
        ecs_rest_merge_patch(info->type, current, value);
        value = current;
    }
    void *decoded = NULL;
    if (!ecs_rest_decode_value(info->type, info->reflection, value, &decoded))
        return ecs_rest_error(400, "invalid_resource_value", "invalid resource value");
    ecs_move_resource_rid(resource, decoded);
    return sihttp_response_json(200, ecs_rest_resource_json(resource, ecs_resource_rid(resource)));
}
