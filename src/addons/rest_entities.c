#include "addons/rest_internal.h"
#include "id.h"
#include "siecs.h"
#include "sijson.h"
#include "storage/component_index.h"
#include "table.h"
#include "world_internal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
ecs_rest_entity_from_index(ecs_world_t *world, int64_t index, ecs_entity_t *out_entity) {
    if (index <= 0 || (uint64_t)index >= world->entity_index.entities.size) {
        return false;
    }

    ecs_entity_record_t *record =
        ecs_vec_get_mut(&world->entity_index.entities, (uint32_t)index, ecs_entity_record_t);
    if (record->table_id == UINT16_MAX) {
        return false;
    }

    *out_entity = ecs_entity((uint32_t)index, record->generation);
    return true;
}

static bool ecs_rest_entity_is_alive(ecs_world_t *world, ecs_entity_t entity) {
    ecs_entity_t current = 0;
    return ecs_rest_entity_from_index(world, ecs_first(entity), &current) && current == entity;
}

static char *entity_name(ecs_world_t *world, ecs_entity_t entity) {
    char *name = NULL;

    if (ecs_has(world, entity, Name)) {
        const char *value = ecs_get(world, entity, Name)->value;
        name = strdup(value ? value : "");
    } else {
        name = siformat("(%d, %d)", ecs_first(entity), ecs_second(entity));
    }

    return name;
}

sijson_value_t ecs_rest_entity_json(ecs_world_t *world, ecs_entity_t entity) {
    sijson_value_t object = sijson_make_object();

    char *name = entity_name(world, entity);
    sijson_object_set(object, "name", sijson_make_string(name));
    free(name);
    sijson_object_set(object, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(object, "generation", sijson_make_number(ecs_second(entity)));
    sijson_object_set(
        object,
        "hasChildren",
        sijson_make_bool(ecs_has_cid(world, entity, ecs_source(ChildOf)))
    );

    return object;
}

static bool ecs_rest_component_is_reflected(ecs_world_t *world, ecs_component_t id) {
    if (id >= world->component_index.components.size) {
        return false;
    }

    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    return record->registered && record->reflection != SIREFLECT_INVALID_HANDLE;
}

static bool ecs_rest_type_name_is(const sireflect_type_info_t *type, const char *name) {
    return type->name && strcmp(type->name, name) == 0;
}

static sijson_value_t
ecs_rest_reflected_value_json(ecs_world_t *world, sireflect_handle_t type_id, const void *ptr);

static sijson_value_t ecs_rest_number_json(const sireflect_type_info_t *type, const void *ptr) {
    switch (type->kind) {
    case sireflect_kind_signed_char:
        return sijson_make_number(*(const signed char *)ptr);
    case sireflect_kind_unsigned_char:
        return sijson_make_number(*(const unsigned char *)ptr);
    case sireflect_kind_u8:
        return sijson_make_number(*(const uint8_t *)ptr);
    case sireflect_kind_u16:
        return sijson_make_number(*(const uint16_t *)ptr);
    case sireflect_kind_unsigned_short:
        return sijson_make_number(*(const unsigned short *)ptr);
    case sireflect_kind_u32:
        return sijson_make_number(*(const uint32_t *)ptr);
    case sireflect_kind_unsigned_int:
        return sijson_make_number(*(const unsigned int *)ptr);
    case sireflect_kind_u64:
        return sijson_make_number((double)*(const uint64_t *)ptr);
    case sireflect_kind_i8:
        return sijson_make_number(*(const int8_t *)ptr);
    case sireflect_kind_i16:
        return sijson_make_number(*(const int16_t *)ptr);
    case sireflect_kind_i32:
        return sijson_make_number(*(const int32_t *)ptr);
    case sireflect_kind_i64:
        return sijson_make_number((double)*(const int64_t *)ptr);
    case sireflect_kind_short:
        return sijson_make_number(*(const short *)ptr);
    case sireflect_kind_int:
        return sijson_make_number(*(const int *)ptr);
    case sireflect_kind_long:
        return sijson_make_number((double)*(const long *)ptr);
    case sireflect_kind_unsigned_long:
        return sijson_make_number((double)*(const unsigned long *)ptr);
    case sireflect_kind_long_long:
        return sijson_make_number((double)*(const long long *)ptr);
    case sireflect_kind_unsigned_long_long:
        return sijson_make_number((double)*(const unsigned long long *)ptr);
    case sireflect_kind_f32:
        return sijson_make_number(*(const float *)ptr);
    case sireflect_kind_f64:
        return sijson_make_number(*(const double *)ptr);
    default:
        return sijson_make_null();
    }
}

static sijson_value_t
ecs_rest_struct_json(ecs_world_t *world, const sireflect_type_info_t *type, const void *ptr) {
    sijson_value_t object = sijson_make_object();

    for (size_t i = 0; i < type->fields.field_count; i++) {
        const sireflect_field_info_t *field = &type->fields.fields[i];
        const void *field_ptr = (const uint8_t *)ptr + field->offset;
        sijson_object_set(
            object,
            field->name,
            ecs_rest_reflected_value_json(world, field->type, field_ptr)
        );
    }

    return object;
}

static sijson_value_t
ecs_rest_array_json(ecs_world_t *world, const sireflect_type_info_t *type, const void *ptr) {
    const sireflect_type_info_t *element =
        sireflect_type_info(world->sireflect_registry, type->element_type);
    if (!element || element->size == 0) {
        return sijson_make_null();
    }

    sijson_value_t array = sijson_make_array();
    for (size_t i = 0; i < type->element_count; i++) {
        const void *element_ptr = (const uint8_t *)ptr + (i * element->size);
        sijson_array_push(
            array,
            ecs_rest_reflected_value_json(world, type->element_type, element_ptr)
        );
    }

    return array;
}

static sijson_value_t
ecs_rest_reflected_value_json(ecs_world_t *world, sireflect_handle_t type_id, const void *ptr) {
    const sireflect_type_info_t *type = sireflect_type_info(world->sireflect_registry, type_id);
    if (!type || !ptr) {
        return sijson_make_null();
    }

    if (type->kind == sireflect_kind_bool) {
        return sijson_make_bool(*(const bool *)ptr);
    }

    if (sireflect_is_numeric(type->kind)) {
        return ecs_rest_number_json(type, ptr);
    }

    if (type->kind == sireflect_kind_char) {
        char str[2] = { *(const char *)ptr, '\0' };
        return sijson_make_string(str);
    }

    if (type->kind == sireflect_kind_ptr) {
        const char *str = *(char *const *)ptr;
        return str ? sijson_make_string(str) : sijson_make_null();
    }

    if (type->kind == sireflect_kind_pointer) {
        const sireflect_type_info_t *element =
            sireflect_type_info(world->sireflect_registry, type->element_type);
        if (element && element->kind == sireflect_kind_char) {
            const char *str = *(char *const *)ptr;
            return str ? sijson_make_string(str) : sijson_make_null();
        }
    }

    if (ecs_rest_type_name_is(type, "ecs_entity_t")) {
        return sijson_make_number((double)*(const ecs_entity_t *)ptr);
    }

    if (type->kind == sireflect_kind_struct) {
        return ecs_rest_struct_json(world, type, ptr);
    }

    if (type->kind == sireflect_kind_array) {
        return ecs_rest_array_json(world, type, ptr);
    }

    return sijson_make_null();
}

static sijson_value_t
ecs_rest_component_value_json(ecs_world_t *world, ecs_component_t id, const void *ptr) {
    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    return ecs_rest_reflected_value_json(world, record->reflection, ptr);
}

sijson_value_t ecs_rest_entity_children_json(ecs_world_t *world, ecs_entity_t entity) {
    sijson_value_t children = sijson_make_array();

    RelationSource *source = ecs_try_get_cid(world, entity, ecs_source(ChildOf));
    if (!source) {
        return children;
    }

    for (uint32_t i = 0; i < source->entities.size; i++) {
        ecs_entity_t child = *ecs_vec_get(&source->entities, i, ecs_entity_t);
        if (ecs_rest_entity_is_alive(world, child)) {
            sijson_array_push(children, ecs_rest_entity_json(world, child));
        }
    }

    return children;
}

sijson_value_t ecs_rest_entity_detail_json(ecs_world_t *world, ecs_entity_t entity) {
    char *name = entity_name(world, entity);

    sijson_value_t detail = sijson_make_object();
    sijson_object_set(detail, "name", sijson_make_string(name));
    free(name);
    sijson_object_set(detail, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(detail, "generation", sijson_make_number(ecs_second(entity)));

    ChildOf *parent = ecs_try_get(world, entity, ChildOf);
    if (parent) {
        sijson_object_set(detail, "parent", ecs_rest_entity_json(world, parent->target));
    }

    sijson_value_t components = sijson_make_array();
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    for (uint32_t i = 0; i < table->type.count; i++) {
        ecs_component_t cid = table->type.ids[i];
        if (!ecs_rest_component_is_reflected(world, cid)) {
            continue;
        }

        const ecs_component_record_t *component_record =
            ecs_component_index_get(&world->component_index, cid);
        const sireflect_type_info_t *component_type =
            sireflect_type_info(world->sireflect_registry, component_record->reflection);
        void *component_ptr = ecs_table_get_component(table, cid, record->table_row);

        sijson_value_t component = sijson_make_object();
        sijson_object_set(component, "id", sijson_make_number(cid));
        sijson_object_set(
            component,
            "name",
            sijson_make_string(component_type && component_type->name ? component_type->name : "")
        );
        sijson_object_set(
            component,
            "value",
            ecs_rest_component_value_json(world, cid, component_ptr)
        );
        sijson_array_push(components, component);
    }

    sijson_object_set(detail, "children", ecs_rest_entity_children_json(world, entity));
    sijson_object_set(detail, "components", components);

    return detail;
}

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;

    sijson_clean();

    sijson_value_t array = sijson_make_array();

    ecs_query_each(world, it, i, { ecs_source(ChildOf) }, { ecs_id(ChildOf), EcsNot }) {
        sijson_array_push(array, ecs_rest_entity_json(world, it.entities[i]));
    }
    ecs_query_each(world, it, i, { ecs_source(ChildOf), EcsNot }, { ecs_id(ChildOf), EcsNot }) {
        sijson_array_push(array, ecs_rest_entity_json(world, it.entities[i]));
    }

    return ecs_rest_json_response(200, array);
}

sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    int64_t index = sihttp_param(req, "index");

    sijson_clean();

    ecs_entity_t entity = 0;
    if (!ecs_rest_entity_from_index(world, index, &entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }

    return ecs_rest_json_response(200, ecs_rest_entity_detail_json(world, entity));
}

sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    int64_t index = sihttp_param(req, "index");

    sijson_clean();

    ecs_entity_t entity = 0;
    if (!ecs_rest_entity_from_index(world, index, &entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }

    return ecs_rest_json_response(200, ecs_rest_entity_children_json(world, entity));
}
