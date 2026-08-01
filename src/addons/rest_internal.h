#ifndef SIECS_ADDONS_REST_INTERNAL_H
#define SIECS_ADDONS_REST_INTERNAL_H

#include "siecs/config.h"

#if SIECS_HAS_REST

#include "siecs.h"
#ifndef SIHTTP_H
#include "sihttp.h"
#endif
#ifndef SIJSON_H
#include "sijson.h"
#endif
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif
#include "../world_internal.h"

ECS_INTERNAL_API sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body);
ECS_INTERNAL_API sihttp_response_t ecs_rest_error_response(int status, const char *message);

ECS_INTERNAL_API sijson_value_t ecs_rest_entity_json(ecs_entity_t entity);
ECS_INTERNAL_API sijson_value_t ecs_rest_entity_children_json(ecs_entity_t entity);
ECS_INTERNAL_API sijson_value_t ecs_rest_entity_detail_json(ecs_entity_t entity);
ECS_INTERNAL_API bool ecs_rest_entity_component_is_reflected(ecs_component_t component);
ECS_INTERNAL_API sijson_value_t
ecs_rest_entity_component_json(ecs_component_t component, const void *ptr);
ECS_INTERNAL_API sihttp_response_t ecs_rest_set_entity_component(
        ecs_entity_t entity,
    ecs_component_t component,
    const char *body
);

ECS_INTERNAL_API sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req);
ECS_INTERNAL_API sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req);
ECS_INTERNAL_API sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req);
ECS_INTERNAL_API sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req);
ECS_INTERNAL_API sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req);
ECS_INTERNAL_API sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req);

#endif
#endif
