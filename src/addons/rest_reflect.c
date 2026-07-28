#include "siecs/config.h"
#if SIECS_HAS_REST
#include "../storage/component_index.h"
#include "../world_internal.h"
#include "rest_internal.h"
#include "siecs.h"
#ifndef SIJSON_H
#include "sijson.h"
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void ensure_sijson_entity_type(void) {
    sireflect_register_struct(
        sijson_default_registry(),
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );
}

bool ecs_rest_entity_component_is_reflected(ecs_component_t component) {
    if (component >= ecs_world.component_index.components.size) {
        return false;
    }

    const ecs_component_record_t *record = ecs_component_index_get(component);
    return record->reflection != SIREFLECT_INVALID_HANDLE && record->reflection_desc != NULL;
}

static bool validate_component_shape(const ecs_component_record_t *record, sijson_value_t value) {
    const sireflect_fields_t *fields =
        sireflect_type_fields(ecs_world.sireflect_registry, record->reflection);
    if (sijson_type(value) != SIJSON_OBJECT || sijson_object_len(value) != fields->field_count) {
        return false;
    }

    for (size_t i = 0; i < fields->field_count; i++) {
        if (!sijson_object_get(value, fields->fields[i].name)) {
            return false;
        }
    }

    for (size_t i = 0; i < sijson_object_len(value); i++) {
        const char *key = sijson_object_key(value, i);
        bool found = false;
        for (size_t f = 0; f < fields->field_count; f++) {
            found = found || strcmp(key, fields->fields[f].name) == 0;
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

static sijson_value_t component_value_json(const ecs_component_record_t *record, const void *ptr) {
    ensure_sijson_entity_type();

    sireflect_handle_t ref = SIREFLECT_INVALID_HANDLE;
    char *json = sijson_to_json_impl(&ref, record->reflection_desc, ptr);
    if (!json) {
        return sijson_make_null();
    }

    sijson_value_t value = sijson_parse(json);
    free(json);
    return value ? value : sijson_make_null();
}

sijson_value_t ecs_rest_entity_component_json(ecs_component_t component_id, const void *ptr) {
    const ecs_component_record_t *record = ecs_component_index_get(component_id);
    const sireflect_type_info_t *type =
        sireflect_type_info(ecs_world.sireflect_registry, record->reflection);

    sijson_value_t component = sijson_make_object();
    sijson_object_set(component, "id", sijson_make_number(component_id));
    sijson_object_set(component, "name", sijson_make_string(type && type->name ? type->name : ""));
    sijson_object_set(component, "value", component_value_json(record, ptr));
    return component;
}

sihttp_response_t ecs_rest_set_entity_component(
    ecs_entity_t entity,
    ecs_component_t component,
    const char *body_text
) {
    sijson_clean();

    if (!ecs_is_alive(entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    if (!ecs_rest_entity_component_is_reflected(component)) {
        return ecs_rest_error_response(404, "component not found");
    }
    if (!ecs_has_cid(entity, component)) {
        return ecs_rest_error_response(404, "entity component not found");
    }

    sijson_value_t body = sijson_parse(body_text);
    sijson_value_t value = body ? sijson_object_get(body, "value") : NULL;
    if (!body || sijson_type(body) != SIJSON_OBJECT || sijson_object_len(body) != 1 || !value) {
        return ecs_rest_error_response(400, "invalid json body");
    }

    const ecs_component_record_t *record = ecs_component_index_get(component);
    if (!validate_component_shape(record, value)) {
        return ecs_rest_error_response(400, "invalid component value");
    }

    char *json = sijson_stringify(value);
    sireflect_handle_t ref = SIREFLECT_INVALID_HANDLE;
    void *decoded = json ? sijson_from_json_impl(&ref, record->reflection_desc, json) : NULL;
    free(json);
    if (!decoded || sijson_error()) {
        return ecs_rest_error_response(400, "invalid component value");
    }

    ecs_set_cid(entity, component, decoded);
    return ecs_rest_json_response(
        200,
        ecs_rest_entity_component_json(component, ecs_get_cid(entity, component))
    );
}
#endif
