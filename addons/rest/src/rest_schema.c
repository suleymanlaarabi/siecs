#include "rest_internal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool *items;
    size_t count;
} ecs_rest_type_set_t;

static bool ecs_rest_component_is_reflected(ecs_component_t id) {
    const ecs_component_info_t *info = ecs_component_info(id);
    return info && info->type != SIREFLECT_INVALID_HANDLE && info->reflection != NULL;
}

static sijson_value_t ecs_rest_field_json(const sireflect_field_info_t *field) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "name", sijson_make_string(field->name));
    sijson_object_set(object, "type", sijson_make_number(field->type));
    return object;
}

static sijson_value_t ecs_rest_component_json(
    ecs_component_t id,
    const ecs_component_info_t *info
) {
    sijson_value_t fields_json = sijson_make_array();
    const sireflect_type_info_t *type =
        sireflect_type_info(sijson_default_registry(), info->type);
    const sireflect_fields_t *fields =
        sireflect_type_fields(sijson_default_registry(), info->type);
    for (size_t i = 0; i < fields->field_count; i++) {
        sijson_array_push(fields_json, ecs_rest_field_json(&fields->fields[i]));
    }

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(type && type->name ? type->name : ""));
    sijson_object_set(object, "isRelation", sijson_make_bool(false));
    sijson_object_set(object, "type", sijson_make_number(info->type));
    sijson_object_set(object, "fields", fields_json);
    return object;
}

static sijson_value_t ecs_rest_relation_json(
    ecs_relation_id_t id,
    const ecs_relation_info_t *info
) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(info->name ? info->name : ""));
    sijson_object_set(object, "storage", sijson_make_number(info->desc.storage));
    sijson_object_set(
        object,
        "onDeleteTarget",
        sijson_make_number(info->desc.on_delete_target)
    );
    sijson_object_set(object, "acyclic", sijson_make_bool(info->desc.acyclic));
    return object;
}

static void ecs_rest_type_set_add(ecs_rest_type_set_t *set, sireflect_handle_t id) {
    if (id == SIREFLECT_INVALID_HANDLE) {
        return;
    }

    if (id >= set->count) {
        size_t count = set->count == 0 ? 64 : set->count;
        while (id >= count) {
            count *= 2;
        }

        bool *items = realloc(set->items, count * sizeof(bool));
        if (!items) {
            abort();
        }

        memset(items + set->count, 0, (count - set->count) * sizeof(bool));
        set->items = items;
        set->count = count;
    }

    set->items[id] = true;
}

static void ecs_rest_collect_component_types(
    ecs_rest_type_set_t *set,
    const ecs_component_info_t *info
) {
    ecs_rest_type_set_add(set, info->type);

    const sireflect_fields_t *fields =
        sireflect_type_fields(sijson_default_registry(), info->type);
    for (size_t i = 0; i < fields->field_count; i++) {
        ecs_rest_type_set_add(set, fields->fields[i].type);
    }
}

static bool ecs_rest_type_name_is(const sireflect_type_info_t *type, const char *name) {
    return type->name && strcmp(type->name, name) == 0;
}

static const char *ecs_rest_editor_type(
    sireflect_handle_t id,
    const sireflect_type_info_t *type
) {
    if (ecs_rest_type_name_is(type, "ecs_entity_t")) {
        return "entity";
    }

    if (type->kind == sireflect_kind_bool) {
        return "boolean";
    }

    if (sireflect_is_numeric(type->kind)) {
        return "number";
    }

    if (type->kind == sireflect_kind_struct) {
        return "object";
    }

    if (type->kind == sireflect_kind_pointer) {
        const sireflect_type_info_t *element =
            sireflect_type_info(sijson_default_registry(), type->element_type);
        if (element && element->kind == sireflect_kind_char) {
            return "string";
        }
    }

    (void)id;
    return "unsupported";
}

static sijson_value_t ecs_rest_type_json(sireflect_handle_t id) {
    const sireflect_type_info_t *type = sireflect_type_info(sijson_default_registry(), id);

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(type->name ? type->name : ""));
    sijson_object_set(object, "editor", sijson_make_string(ecs_rest_editor_type(id, type)));
    return object;
}

sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req) {
    (void)req;
    ecs_rest_type_set_t types = { 0 };

    sijson_clean();

    sijson_value_t components = sijson_make_array();
    for (uint32_t id = 1; id < ecs_component_count(); id++) {
        ecs_component_t component = (ecs_component_t)id;
        const ecs_component_info_t *info = ecs_component_info(component);
        if (!ecs_rest_component_is_reflected(component)) {
            continue;
        }

        ecs_rest_collect_component_types(&types, info);
        sijson_array_push(components, ecs_rest_component_json(component, info));
    }

    sijson_value_t relations = sijson_make_array();
    for (uint32_t id = 1; id < ecs_relation_count(); id++) {
        const ecs_relation_info_t *info = ecs_relation_info((ecs_relation_id_t)id);
        if (info) {
            sijson_array_push(relations, ecs_rest_relation_json((ecs_relation_id_t)id, info));
        }
    }

    sijson_value_t type_values = sijson_make_array();
    for (size_t id = 1; id < types.count; id++) {
        if (types.items[id]) {
            sijson_array_push(type_values, ecs_rest_type_json((sireflect_handle_t)id));
        }
    }

    free(types.items);

    sijson_value_t schema = sijson_make_object();
    sijson_object_set(schema, "components", components);
    sijson_object_set(schema, "relations", relations);
    sijson_object_set(schema, "types", type_values);
    return ecs_rest_json_response(200, schema);
}
