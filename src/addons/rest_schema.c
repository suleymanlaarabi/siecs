#include "rest_internal.h"
#include "../storage/component_index.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool *items;
    size_t count;
} ecs_rest_type_set_t;

static bool ecs_rest_component_is_reflected(ecs_world_t *world, ecs_component_t id) {
    if (id >= world->component_index.components.size) {
        return false;
    }

    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    return record->registered && record->reflection != SIREFLECT_INVALID_HANDLE;
}

static sijson_value_t ecs_rest_field_json(const sireflect_field_info_t *field) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "name", sijson_make_string(field->name));
    sijson_object_set(object, "type", sijson_make_number(field->type));

    return object;
}

static sijson_value_t ecs_rest_component_json(
    ecs_world_t *world,
    ecs_component_t id,
    const ecs_component_record_t *record
) {
    sijson_value_t fields_json = sijson_make_array();
    const sireflect_type_info_t *type =
        sireflect_type_info(world->sireflect_registry, record->reflection);
    const sireflect_fields_t *fields =
        sireflect_type_fields(world->sireflect_registry, record->reflection);
    for (size_t i = 0; i < fields->field_count; i++) {
        sijson_array_push(fields_json, ecs_rest_field_json(&fields->fields[i]));
    }

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(type && type->name ? type->name : ""));
    sijson_object_set(object, "isRelation", sijson_make_bool(record->relation_flags != 0));
    sijson_object_set(object, "type", sijson_make_number(record->reflection));
    sijson_object_set(object, "fields", fields_json);

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
    ecs_world_t *world,
    ecs_rest_type_set_t *set,
    const ecs_component_record_t *record
) {
    ecs_rest_type_set_add(set, record->reflection);

    const sireflect_fields_t *fields =
        sireflect_type_fields(world->sireflect_registry, record->reflection);
    for (size_t i = 0; i < fields->field_count; i++) {
        ecs_rest_type_set_add(set, fields->fields[i].type);
    }
}

static bool ecs_rest_type_name_is(const sireflect_type_info_t *type, const char *name) {
    return type->name && strcmp(type->name, name) == 0;
}

static const char *
ecs_rest_editor_type(ecs_world_t *world, sireflect_handle_t id, const sireflect_type_info_t *type) {
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
            sireflect_type_info(world->sireflect_registry, type->element_type);
        if (element && element->kind == sireflect_kind_char) {
            return "string";
        }
    }

    (void)id;
    return "unsupported";
}

static sijson_value_t ecs_rest_type_json(ecs_world_t *world, sireflect_handle_t id) {
    const sireflect_type_info_t *type = sireflect_type_info(world->sireflect_registry, id);

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(type->name ? type->name : ""));
    sijson_object_set(object, "editor", sijson_make_string(ecs_rest_editor_type(world, id, type)));

    return object;
}

sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    ecs_rest_type_set_t types = { 0 };

    sijson_clean();

    sijson_value_t components = sijson_make_array();
    for (uint32_t i = 2; i < world->component_index.components.size; i++) {
        if (!ecs_rest_component_is_reflected(world, (ecs_component_t)i)) {
            continue;
        }

        const ecs_component_record_t *record =
            ecs_component_index_get(&world->component_index, (ecs_component_t)i);
        ecs_rest_collect_component_types(world, &types, record);
        sijson_array_push(components, ecs_rest_component_json(world, (ecs_component_t)i, record));
    }

    sijson_value_t type_values = sijson_make_array();
    for (size_t i = 1; i < types.count; i++) {
        if (types.items[i]) {
            sijson_array_push(type_values, ecs_rest_type_json(world, (sireflect_handle_t)i));
        }
    }

    free(types.items);

    sijson_value_t schema = sijson_make_object();
    sijson_object_set(schema, "components", components);
    sijson_object_set(schema, "types", type_values);

    return ecs_rest_json_response(200, schema);
}
