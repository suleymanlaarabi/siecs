#ifndef SIECS_REST_INTERNAL_H
#define SIECS_REST_INTERNAL_H

#include <sijson.h>
#include <sireflect.h>
#include <sihttp.h>
#include <siecs.h>

ECS_RESOURCE_DECLARE(SiecsRestState, { sihttp_server_t *server; });

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body);
sihttp_response_t ecs_rest_error_response(int status, const char *message);

sijson_value_t ecs_rest_entity_json(ecs_entity_t entity);
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
sihttp_response_t ecs_rest_set_entity_component(
    ecs_entity_t entity,
    ecs_component_t component,
    const char *body
);

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity_relations(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_entity_relation(const sihttp_request_t *req);
sihttp_response_t ecs_rest_delete_entity_relation(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req);

#endif
