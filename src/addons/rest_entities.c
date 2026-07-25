#include "helper.h"
#include "rest_internal.h"
#include "siecs.h"
#include <string.h>
#ifndef SIJSON_H
#include "sijson.h"
#endif
#include "../storage/component_index.h"
#include "../table.h"
#include "../world_internal.h"
#include <stdint.h>
#include <stdlib.h>

static bool entity_from_index(int64_t index, ecs_entity_t *out) {
    if (index <= 0 || (uint64_t)index >= ecs_world.entity_index.entities.size) {
        return false;
    }

    ecs_entity_record_t *record =
        ecs_vec_get_mut(&ecs_world.entity_index.entities, (uint32_t)index, ecs_entity_record_t);
    if (record->table_id == UINT16_MAX) {
        return false;
    }

    *out = ecs_entity((uint32_t)index, record->generation);
    return true;
}

static bool entity_is_alive(ecs_entity_t entity) {
    ecs_entity_t current = 0;
    return entity_from_index(ecs_first(entity), &current) && current == entity;
}

static char *entity_name(ecs_entity_t entity) {
    if (ecs_has(entity, Name)) {
        const char *value = ecs_get(entity, Name)->value;
        return strdup(value ? value : "");
    }
    return siformat("(%d, %d)", ecs_first(entity), ecs_second(entity));
}

sijson_value_t ecs_rest_entity_json(ecs_entity_t entity) {
    sijson_value_t object = sijson_make_object();

    char *name = entity_name(entity);
    sijson_object_set(object, "name", sijson_make_string(name));
    free(name);

    sijson_object_set(object, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(object, "generation", sijson_make_number(ecs_second(entity)));
    sijson_object_set(
        object,
        "hasChildren",
        sijson_make_bool(ecs_has_cid(entity, ecs_source(ChildOf)))
    );
    return object;
}

sijson_value_t ecs_rest_entity_children_json(ecs_entity_t entity) {
    sijson_value_t children = sijson_make_array();
    RelationSource *source = ecs_try_get_cid(entity, ecs_source(ChildOf));
    if (!source) {
        return children;
    }

    for (uint32_t i = 0; i < source->entities.size; i++) {
        ecs_entity_t child = *ecs_vec_get(&source->entities, i, ecs_entity_t);
        if (entity_is_alive(child)) {
            sijson_array_push(children, ecs_rest_entity_json(child));
        }
    }
    return children;
}

sijson_value_t ecs_rest_entity_detail_json(ecs_entity_t entity) {
    sijson_value_t detail = sijson_make_object();

    char *name = entity_name(entity);
    sijson_object_set(detail, "name", sijson_make_string(name));
    free(name);

    sijson_object_set(detail, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(detail, "generation", sijson_make_number(ecs_second(entity)));

    ChildOf *parent = ecs_try_get(entity, ChildOf);
    if (parent) {
        sijson_object_set(detail, "parent", ecs_rest_entity_json(parent->target));
    }

    sijson_value_t components = sijson_make_array();
    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    for (uint32_t i = 0; i < table->type.count; i++) {
        ecs_component_t cid = table->type.ids[i];
        if (ecs_rest_entity_component_is_reflected(cid)) {
            void *ptr = ecs_table_get_component(table, cid, record->table_row);
            sijson_array_push(components, ecs_rest_entity_component_json(cid, ptr));
        }
    }

    if (table->type.base) {
        sijson_object_set(detail, "isA", ecs_rest_entity_detail_json(table->type.base));
    }

    sijson_object_set(detail, "children", ecs_rest_entity_children_json(entity));
    sijson_object_set(detail, "components", components);
    return detail;
}

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req) {
    sijson_clean();

    sijson_value_t array = sijson_make_array();
    ecs_query_each(it, i, { ecs_source(ChildOf) }, { ecs_id(ChildOf), EcsNot }) {
        sijson_array_push(array, ecs_rest_entity_json(it.entities[i]));
    }
    ecs_query_each(it, i, { ecs_source(ChildOf), EcsNot }, { ecs_id(ChildOf), EcsNot }) {
        sijson_array_push(array, ecs_rest_entity_json(it.entities[i]));
    }
    return ecs_rest_json_response(200, array);
}

sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = 0;
    if (!entity_from_index(sihttp_param(req, "index"), &entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_detail_json(entity));
}

sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = 0;
    if (!entity_from_index(sihttp_param(req, "index"), &entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_children_json(entity));
}

sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req) {
    int64_t component = sihttp_param(req, "component");

    ecs_entity_t entity = 0;
    if (!entity_from_index(sihttp_param(req, "index"), &entity)) {
        sijson_clean();
        return ecs_rest_error_response(404, "entity not found");
    }
    if (component <= 0 || component > UINT16_MAX) {
        sijson_clean();
        return ecs_rest_error_response(404, "component not found");
    }

    return ecs_rest_set_entity_component(entity, (ecs_component_t)component, req->body);
}

sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req) {
    ecs_entity_t entity = ecs_new();
    return sihttp_response(
        {
            .body = sijson_stringify(ecs_rest_entity_json(entity)),
        }
    );
}
