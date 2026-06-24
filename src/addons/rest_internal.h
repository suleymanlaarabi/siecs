#ifndef SIECS_ADDONS_REST_INTERNAL_H
#define SIECS_ADDONS_REST_INTERNAL_H

#include "siecs.h"
#include "sihttp.h"
#include "sijson.h"
#include "sireflect.h"
#include "world_internal.h"

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body);
sihttp_response_t ecs_rest_error_response(int status, const char *message);

sijson_value_t ecs_rest_entity_json(ecs_world_t *world, ecs_entity_t entity);
sijson_value_t ecs_rest_component_json(ecs_world_t *world, ecs_component_t id);

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_components(const sihttp_request_t *req);

#endif
