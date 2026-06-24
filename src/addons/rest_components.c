#include "addons/rest_internal.h"
#include "storage/component_index.h"
#include <stdbool.h>
#include <stdint.h>

static sijson_value_t ecs_rest_type_json(ecs_world_t *world, sireflect_handle_t handle) {
    const sireflect_type_info_t *type = sireflect_type_info(world->sireflect_registry, handle);

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(handle));
    sijson_object_set(object, "name", sijson_make_string(type->name));
    sijson_object_set(object, "kind", sijson_make_string(sireflect_kind_name(type->kind)));
    sijson_object_set(object, "size", sijson_make_number(type->size));
    sijson_object_set(object, "align", sijson_make_number(type->align));

    if (type->element_type != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *element =
            sireflect_type_info(world->sireflect_registry, type->element_type);

        sijson_object_set(object, "elementType", sijson_make_number(type->element_type));
        sijson_object_set(object, "elementName", sijson_make_string(element->name));
    }
    if (type->element_count != 0) {
        sijson_object_set(object, "elementCount", sijson_make_number(type->element_count));
    }

    return object;
}

static sijson_value_t ecs_rest_field_json(
    ecs_world_t *world,
    const sireflect_field_info_t *field
) {
    const sireflect_type_info_t *type = sireflect_type_info(world->sireflect_registry, field->type);

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "name", sijson_make_string(field->name));
    sijson_object_set(object, "type", sijson_make_string(type->name));
    sijson_object_set(object, "typeId", sijson_make_number(field->type));
    sijson_object_set(object, "kind", sijson_make_string(sireflect_kind_name(type->kind)));
    sijson_object_set(object, "offset", sijson_make_number(field->offset));
    sijson_object_set(object, "size", sijson_make_number(field->size));
    sijson_object_set(object, "align", sijson_make_number(field->align));

    sijson_value_t qualifiers = sijson_make_object();
    sijson_object_set(
        qualifiers,
        "const",
        sijson_make_bool((field->qualifiers & SIREFLECT_QUAL_CONST) != 0)
    );
    sijson_object_set(
        qualifiers,
        "volatile",
        sijson_make_bool((field->qualifiers & SIREFLECT_QUAL_VOLATILE) != 0)
    );
    sijson_object_set(object, "qualifiers", qualifiers);

    return object;
}

static sijson_value_t ecs_rest_component_fields_json(ecs_world_t *world, ecs_component_t id) {
    sijson_value_t array = sijson_make_array();

    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    if (record->reflection == SIREFLECT_INVALID_HANDLE) {
        return array;
    }

    const sireflect_fields_t *fields =
        sireflect_type_fields(world->sireflect_registry, record->reflection);
    for (size_t i = 0; i < fields->field_count; i++) {
        sijson_array_push(array, ecs_rest_field_json(world, &fields->fields[i]));
    }

    return array;
}

sijson_value_t ecs_rest_component_json(ecs_world_t *world, ecs_component_t id) {
    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(record->name ? record->name : ""));
    sijson_object_set(object, "size", sijson_make_number(record->size));
    sijson_object_set(object, "relation", sijson_make_bool(record->relation_flags != 0));
    sijson_object_set(object, "requiredCount", sijson_make_number(record->required_count));
    sijson_object_set(object, "tableCount", sijson_make_number(record->tables.size));
    sijson_object_set(
        object,
        "reflected",
        sijson_make_bool(record->reflection != SIREFLECT_INVALID_HANDLE)
    );

    if (record->reflection != SIREFLECT_INVALID_HANDLE) {
        sijson_object_set(object, "type", ecs_rest_type_json(world, record->reflection));
    }

    sijson_object_set(object, "fields", ecs_rest_component_fields_json(world, id));

    return object;
}

static bool ecs_rest_component_exists(ecs_world_t *world, ecs_component_t id) {
    if (id >= world->component_index.components.size) {
        return false;
    }

    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    return record->registered;
}

sihttp_response_t ecs_rest_get_components(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    int64_t id = sihttp_param(req, "id");

    sijson_clean();

    if (id != 0) {
        if (id < 0 || id > UINT16_MAX || !ecs_rest_component_exists(world, (ecs_component_t)id)) {
            return ecs_rest_error_response(404, "component not found");
        }

        return ecs_rest_json_response(200, ecs_rest_component_json(world, (ecs_component_t)id));
    }

    sijson_value_t array = sijson_make_array();
    for (uint32_t i = 0; i < world->component_index.components.size; i++) {
        const ecs_component_record_t *record =
            ecs_vec_get(&world->component_index.components, i, ecs_component_record_t);
        if (!record->registered) {
            continue;
        }

        sijson_array_push(array, ecs_rest_component_json(world, (ecs_component_t)i));
    }

    return ecs_rest_json_response(200, array);
}
