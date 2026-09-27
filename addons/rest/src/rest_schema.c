#include "rest_internal.h"
#include <siecs/reflect_internal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool *items;
    size_t count;
} ecs_rest_type_set_t;

static bool ecs_rest_type_set_add(ecs_rest_type_set_t *set, sireflect_handle_t id) {
    if (id == SIREFLECT_INVALID_HANDLE)
        return true;
    if (id >= set->count) {
        size_t count = set->count ? set->count : 64;
        while (id >= count)
            count *= 2;
        bool *items = realloc(set->items, count * sizeof(bool));
        if (!items)
            return false;
        memset(items + set->count, 0, (count - set->count) * sizeof(bool));
        set->items = items;
        set->count = count;
    }
    set->items[id] = true;
    return true;
}

static bool ecs_rest_collect_type_visit(const sireflect_type_visit_t *visit, void *user) {
    return ecs_rest_type_set_add(user, visit->type);
}

static bool ecs_rest_collect_types(ecs_rest_type_set_t *set, sireflect_handle_t type) {
    return sireflect_walk_type(type, SIREFLECT_WALK_DEDUPLICATE, ecs_rest_collect_type_visit, set);
}

static sijson_value_t ecs_rest_meta_value(const sireflect_meta_t *meta) {
    switch (meta->kind) {
    case SIREFLECT_META_BOOL:
        return sijson_make_bool(meta->value.boolean);
    case SIREFLECT_META_I64:
        return sijson_make_number((double)meta->value.i64);
    case SIREFLECT_META_U64:
        return sijson_make_number((double)meta->value.u64);
    case SIREFLECT_META_F64:
        return sijson_make_number(meta->value.f64);
    default:
        return sijson_make_string(meta->value.string);
    }
}

static sijson_value_t ecs_rest_meta_json(const sireflect_metas_t *metas) {
    sijson_value_t object = sijson_make_object();
    if (metas) {
        for (size_t i = 0; i < metas->count; i++) {
            const sireflect_meta_t *meta = metas->items[i];
            sijson_object_set(object, meta->key, ecs_rest_meta_value(meta));
        }
    }
    return object;
}

static sijson_value_t
ecs_rest_field_json(sireflect_handle_t parent, const sireflect_field_info_t *field) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "name", sijson_make_string(field->name));
    sijson_object_set(object, "type", sijson_make_number((double)field->type));
    sijson_object_set(object, "qualifiers", sijson_make_number(field->qualifiers));
    sijson_object_set(
        object,
        "meta",
        ecs_rest_meta_json(sireflect_field_metas(parent, field->name))
    );
    return object;
}

static sijson_value_t ecs_rest_fields_json(sireflect_handle_t type) {
    sijson_value_t fields = sijson_make_array();
    const sireflect_fields_t *view = sireflect_type_fields(type);
    for (size_t i = 0; view && i < view->field_count; i++)
        sijson_array_push(fields, ecs_rest_field_json(type, &view->fields[i]));
    return fields;
}

static sijson_value_t
ecs_rest_component_json(ecs_component_t id, const ecs_component_info_t *info) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(info->name));
    sijson_object_set(object, "isRelation", sijson_make_bool(false));
    sijson_object_set(object, "type", sijson_make_number((double)info->type));
    sijson_object_set(object, "fields", ecs_rest_fields_json(info->type));
    return object;
}

static sijson_value_t
ecs_rest_schema_resource_json(ecs_resource_t id, const ecs_resource_info_t *info) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(info->name));
    sijson_object_set(object, "type", sijson_make_number((double)info->type));
    return object;
}

static sijson_value_t
ecs_rest_relation_json(ecs_relation_id_t id, const ecs_relation_info_t *info) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(info->name ? info->name : ""));
    sijson_object_set(object, "storage", sijson_make_number(info->desc.storage));
    sijson_object_set(object, "onDeleteTarget", sijson_make_number(info->desc.on_delete_target));
    sijson_object_set(object, "acyclic", sijson_make_bool(info->desc.acyclic));
    return object;
}

static const char *ecs_rest_category_name(sireflect_handle_t id) {
    const sireflect_type_info_t *info = sireflect_type_info(id);
    if (info && info->kind == sireflect_kind_char)
        return "character";
    switch (ecs_reflect_value_kind(id)) {
    case EcsReflectValueEntity:
        return "entity";
    case EcsReflectValueStruct:
        return "object";
    case EcsReflectValueArray:
        return "array";
    case EcsReflectValueString:
        return "string";
    case EcsReflectValueEnum:
        return "enum";
    case EcsReflectValuePointer:
        return "pointer";
    default:
        break;
    }
    switch (sireflect_type_category(id)) {
    case sireflect_category_boolean:
        return "boolean";
    case sireflect_category_integer:
        return "integer";
    case sireflect_category_floating:
        return "number";
    default:
        return "invalid";
    }
}

static sijson_value_t ecs_rest_type_json(sireflect_handle_t id) {
    const sireflect_type_info_t *type = sireflect_type_info(id);
    const char *category = ecs_rest_category_name(id);
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number((double)id));
    sijson_object_set(object, "name", sijson_make_string(type->name ? type->name : ""));
    sijson_object_set(object, "category", sijson_make_string(category));
    sijson_object_set(object, "size", sijson_make_number((double)type->size));
    sijson_object_set(object, "align", sijson_make_number((double)type->align));
    sijson_object_set(
        object,
        "editable",
        sijson_make_bool(strcmp(category, "pointer") != 0 && strcmp(category, "invalid") != 0)
    );
    sijson_object_set(object, "meta", ecs_rest_meta_json(sireflect_type_metas(id)));
    const char *editor = strcmp(category, "integer") == 0     ? "number"
                         : strcmp(category, "character") == 0 ? "string"
                                                              : category;
    sijson_object_set(object, "editor", sijson_make_string(editor));
    if (type->kind == sireflect_kind_struct)
        sijson_object_set(object, "fields", ecs_rest_fields_json(id));
    if (type->kind == sireflect_kind_array) {
        sijson_object_set(object, "element", sijson_make_number((double)type->element_type));
        sijson_object_set(object, "count", sijson_make_number((double)type->element_count));
    }
    if (strcmp(category, "pointer") == 0 && type->element_type)
        sijson_object_set(object, "element", sijson_make_number((double)type->element_type));
    if (sireflect_type_is_enum(type)) {
        sijson_value_t values = sijson_make_array();
        sijson_value_t options = sijson_make_array();
        for (size_t i = 0; i < type->enum_values.value_count; i++) {
            const sireflect_enum_value_t *item = &type->enum_values.values[i];
            sijson_value_t entry = sijson_make_object();
            sijson_object_set(entry, "name", sijson_make_string(item->name));
            sijson_object_set(entry, "value", sijson_make_number((double)item->value));
            sijson_array_push(values, entry);
            sijson_array_push(options, sijson_make_string(item->name));
        }
        sijson_object_set(object, "values", values);
        sijson_object_set(object, "options", options);
    }
    return object;
}

sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req) {
    (void)req;
    ecs_rest_type_set_t types = { 0 };
    sijson_clean();
    sijson_value_t components = sijson_make_array();
    for (uint32_t id = 1; id < ecs_component_count(); id++) {
        const ecs_component_info_t *info = ecs_component_info((ecs_component_t)id);
        if (!info || info->type == SIREFLECT_INVALID_HANDLE)
            continue;
        if (!ecs_rest_collect_types(&types, info->type)) {
            free(types.items);
            return ecs_rest_error(500, "schema_collection_failed", "schema collection failed");
        }
        sijson_array_push(components, ecs_rest_component_json((ecs_component_t)id, info));
    }
    sijson_value_t resources = sijson_make_array();
    for (uint32_t id = 1; id < ecs_resource_count(); id++) {
        const ecs_resource_info_t *info = ecs_resource_info((ecs_resource_t)id);
        if (!info || info->type == SIREFLECT_INVALID_HANDLE)
            continue;
        if (!ecs_rest_collect_types(&types, info->type)) {
            free(types.items);
            return ecs_rest_error(500, "schema_collection_failed", "schema collection failed");
        }
        sijson_array_push(resources, ecs_rest_schema_resource_json((ecs_resource_t)id, info));
    }
    sijson_value_t relations = sijson_make_array();
    for (uint32_t id = 1; id < ecs_relation_count(); id++) {
        const ecs_relation_info_t *info = ecs_relation_info((ecs_relation_id_t)id);
        if (info)
            sijson_array_push(relations, ecs_rest_relation_json((ecs_relation_id_t)id, info));
    }
    sijson_value_t type_values = sijson_make_array();
    for (size_t id = 1; id < types.count; id++)
        if (types.items[id])
            sijson_array_push(type_values, ecs_rest_type_json((sireflect_handle_t)id));
    free(types.items);
    sijson_value_t schema = sijson_make_object();
    sijson_object_set(schema, "components", components);
    sijson_object_set(schema, "resources", resources);
    sijson_object_set(schema, "relations", relations);
    sijson_object_set(schema, "types", type_values);
    return sihttp_response_json(200, schema);
}
