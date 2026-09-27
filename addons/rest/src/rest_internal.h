#ifndef SIECS_REST_INTERNAL_H
#define SIECS_REST_INTERNAL_H

#include <siecs.h>
#include <sihttp.h>
#include <sijson.h>
#include <sireflect.h>

ECS_RESOURCE_DECLARE(SiecsRestState, {
    sihttp_server_t *server;
    ecs_module_id_t loaded_module;
});

ecs_entity_t ecs_rest_request_entity(const sihttp_request_t *req);
bool ecs_rest_request_component(const sihttp_request_t *req, ecs_component_t *component);
bool ecs_rest_request_relation(const sihttp_request_t *req, ecs_relation_id_t *relation);

sijson_value_t ecs_rest_entity_json(ecs_entity_t entity, bool has_children);
sijson_value_t ecs_rest_entity_relations_json(ecs_entity_t entity);
bool ecs_rest_entity_component_is_reflected(ecs_component_t component);
sihttp_response_t ecs_rest_error(int status, const char *code, const char *message);
sijson_value_t ecs_rest_entity_component_json(ecs_component_t component, const void *ptr);
sijson_value_t ecs_rest_resource_json(ecs_resource_t resource, const void *ptr);
bool ecs_rest_validate_full_value(sireflect_handle_t type, sijson_value_t value,
                                  sijson_value_t current);
bool ecs_rest_validate_patch_value(sireflect_handle_t type, sijson_value_t value,
                                   sijson_value_t current);
bool ecs_rest_decode_component_value(
    ecs_component_t component,
    sijson_value_t value,
    void **decoded
);
sihttp_response_t ecs_rest_set_entity_component(ecs_entity_t entity, ecs_component_t component,
                                               const char *body, bool patch);
sihttp_response_t ecs_rest_set_resource(ecs_resource_t resource, const char *body, bool patch);

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_all_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity_relations(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_entity_relation(const sihttp_request_t *req);
sihttp_response_t ecs_rest_delete_entity_relation(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_patch_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_delete_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_resources(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_resource(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_resource(const sihttp_request_t *req);
sihttp_response_t ecs_rest_patch_resource(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_scene(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_scene(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_modules(const sihttp_request_t *req);

#endif
