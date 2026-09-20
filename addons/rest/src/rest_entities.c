#include "rest_internal.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    ecs_entity_t *parents;
    size_t count;
} rest_children_set_t;

static ecs_query_id_t rest_entity_query(ecs_query_relation_term_t relation) {
    return ecs_query({
        .components = {
            ecs_in_optional(Abstract),
            ecs_in_optional(Disabled),
        },
        .relations = { relation },
    });
}

static int rest_entity_compare(const void *left, const void *right) {
    ecs_entity_t a = *(const ecs_entity_t *)left;
    ecs_entity_t b = *(const ecs_entity_t *)right;
    return a > b ? 1 : a < b ? -1 : 0;
}

static rest_children_set_t rest_children_set_init(void) {
    rest_children_set_t set = { 0 };
    ecs_query_id_t query = rest_entity_query(ecs_rel(ChildOf));
    set.count = ecs_query_count(query);
    if (set.count) {
        set.parents = malloc(set.count * sizeof(ecs_entity_t));
        if (!set.parents) {
            set.count = 0;
            ecs_query_fini(query);
            return set;
        }
    }

    size_t at = 0;
    ecs_iter_t it = ecs_query_iter(query);
    while (ecs_iter_next(&it)) {
        const ecs_relation_target_t *targets = ecs_targets(&it, ChildOf);
        for (uint32_t i = 0; i < it.count; i++) {
            if (targets[i].entity && at < set.count) {
                set.parents[at++] = targets[i].entity;
            }
        }
    }
    ecs_query_fini(query);
    set.count = at;
    if (set.count) {
        qsort(set.parents, set.count, sizeof(ecs_entity_t), rest_entity_compare);
    }
    return set;
}

static void rest_children_set_fini(rest_children_set_t *set) {
    free(set->parents);
    set->parents = NULL;
    set->count = 0;
}

static bool rest_children_set_contains(const rest_children_set_t *set, ecs_entity_t entity) {
    size_t first = 0;
    size_t last = set->count;
    while (first < last) {
        size_t middle = first + (last - first) / 2;
        if (set->parents[middle] < entity) {
            first = middle + 1;
        } else if (set->parents[middle] > entity) {
            last = middle;
        } else {
            return true;
        }
    }
    return false;
}

static void
rest_append_entity(sijson_value_t array, ecs_entity_t entity, const rest_children_set_t *children) {
    if (ecs_is_alive(entity)) {
        sijson_array_push(
            array,
            ecs_rest_entity_json(entity, rest_children_set_contains(children, entity))
        );
    }
}

sijson_value_t ecs_rest_entity_json(ecs_entity_t entity, bool has_children) {
    sijson_value_t object = sijson_make_object();

    sijson_object_set(object, "name", sijson_make_string(ecs_entity_name(entity)));
    sijson_object_set(object, "index", sijson_make_number(ecs_entity_id(entity)));
    sijson_object_set(object, "generation", sijson_make_number(ecs_entity_generation(entity)));
    sijson_object_set(object, "hasChildren", sijson_make_bool(has_children));
    return object;
}

sijson_value_t ecs_rest_entity_children_json(ecs_entity_t entity) {
    sijson_value_t children = sijson_make_array();
    rest_children_set_t children_set = rest_children_set_init();
    ecs_query_id_t query = rest_entity_query(ecs_rel(ChildOf));
    ecs_iter_t it = ecs_query_iter(query);
    while (ecs_iter_next(&it)) {
        const ecs_relation_target_t *targets = ecs_targets(&it, ChildOf);
        for (uint32_t i = 0; i < it.count; i++) {
            ecs_entity_t child = it.entities[i];
            if (targets[i].entity == entity) {
                rest_append_entity(children, child, &children_set);
            }
        }
    }
    ecs_query_fini(query);
    rest_children_set_fini(&children_set);
    return children;
}

sijson_value_t ecs_rest_entity_detail_json(ecs_entity_t entity) {
    sijson_value_t detail = sijson_make_object();

    sijson_object_set(detail, "name", sijson_make_string(ecs_entity_name(entity)));
    sijson_object_set(detail, "index", sijson_make_number(ecs_entity_id(entity)));
    sijson_object_set(detail, "generation", sijson_make_number(ecs_entity_generation(entity)));

    sijson_value_t components = sijson_make_array();
    for (uint32_t id = 1; id < ecs_component_count(); id++) {
        ecs_component_t component = (ecs_component_t)id;
        if (ecs_rest_entity_component_is_reflected(component) &&
            ecs_has_cid_owned(entity, component)) {
            sijson_array_push(
                components,
                ecs_rest_entity_component_json(component, ecs_get_cid(entity, component))
            );
        }
    }

    sijson_object_set(detail, "children", ecs_rest_entity_children_json(entity));
    sijson_object_set(detail, "relations", ecs_rest_entity_relations_json(entity));
    sijson_object_set(detail, "components", components);
    return detail;
}

static sihttp_response_t rest_get_entity_list(ecs_query_relation_term_t relation) {
    sijson_clean();

    rest_children_set_t children_set = rest_children_set_init();
    sijson_value_t array = sijson_make_array();
    ecs_query_id_t query = rest_entity_query(relation);
    ecs_iter_t it = ecs_query_iter(query);
    while (ecs_iter_next(&it)) {
        for (uint32_t i = 0; i < it.count; i++) {
            rest_append_entity(array, it.entities[i], &children_set);
        }
    }
    ecs_query_fini(query);
    rest_children_set_fini(&children_set);
    return ecs_rest_json_response(200, array);
}

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req) {
    (void)req;
    return rest_get_entity_list(ecs_not_rel(ChildOf));
}

sihttp_response_t ecs_rest_get_all_entities(const sihttp_request_t *req) {
    (void)req;
    return rest_get_entity_list(ecs_rel_opt(ChildOf));
}

sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = ecs_rest_request_entity(req);
    if (!entity) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_detail_json(entity));
}

sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = ecs_rest_request_entity(req);
    if (!entity) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_children_json(entity));
}

sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req) {
    (void)req;
    ecs_entity_t entity = ecs_new();
    sihttp_response_t response = { 0 };
    response.body = sijson_stringify(ecs_rest_entity_json(entity, false));
    return response;
}
