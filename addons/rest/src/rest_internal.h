#ifndef SIECS_REST_INTERNAL_H
#define SIECS_REST_INTERNAL_H

#include <sijson.h>
#include <sireflect.h>
#include <sihttp.h>
#include <siecs.h>

ECS_RESOURCE_DECLARE(SiecsRestState, {
    sihttp_server_t *server;
    size_t max_scene_bytes;
    ecs_module_id_t loaded_module;
});

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body);
sihttp_response_t ecs_rest_error_response(int status, const char *message);
sihttp_response_t ecs_rest_binary_response(void *data, size_t size);
ecs_entity_t ecs_rest_request_entity(const sihttp_request_t *req);
bool ecs_rest_request_component(
    const sihttp_request_t *req,
    ecs_component_t *component
);
bool ecs_rest_request_relation(
    const sihttp_request_t *req,
    ecs_relation_id_t *relation
);

sijson_value_t ecs_rest_entity_json(ecs_entity_t entity, bool has_children);
sijson_value_t ecs_rest_entity_ref_json(ecs_entity_t entity);
sijson_value_t ecs_rest_entity_children_json(ecs_entity_t entity);
sijson_value_t ecs_rest_entity_relation_json(
    ecs_relation_id_t relation,
    ecs_entity_t target
);
sijson_value_t ecs_rest_entity_relations_json(ecs_entity_t entity);
bool ecs_rest_relation_would_cycle(
    ecs_entity_t source,
    ecs_relation_id_t relation,
    ecs_entity_t target
);
sijson_value_t ecs_rest_entity_detail_json(ecs_entity_t entity);
bool ecs_rest_entity_component_is_reflected(ecs_component_t component);
sijson_value_t ecs_rest_entity_component_json(ecs_component_t component, const void *ptr);
bool ecs_rest_decode_component_value(
    ecs_component_t component,
    sijson_value_t value,
    void **decoded
);
sihttp_response_t ecs_rest_set_entity_component(
    ecs_entity_t entity,
    ecs_component_t component,
    const char *body
);

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_all_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity_relations(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_entity_relation(const sihttp_request_t *req);
sihttp_response_t ecs_rest_delete_entity_relation(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_delete_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_scene(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_scene(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_modules(const sihttp_request_t *req);

#endif
