#include "rest_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool rest_path_uint_segment(
    const sihttp_request_t *req,
    size_t segment,
    uint32_t limit,
    uint32_t *result
) {
    const char *cursor = req->path;

    if (*cursor == '/') {
        cursor++;
    }

    for (size_t current = 0;; current++) {
        const char *end = cursor;
        while (*end && *end != '/' && *end != '?') {
            end++;
        }

        if (current == segment) {
            if (cursor == end) {
                return false;
            }

            uint32_t value = 0;
            for (const char *digit = cursor; digit < end; digit++) {
                if (*digit < '0' || *digit > '9') {
                    return false;
                }

                uint32_t number = (uint32_t)(*digit - '0');
                if (value > (limit - number) / 10) {
                    return false;
                }
                value = value * 10 + number;
            }

            *result = value;
            return true;
        }

        if (*end != '/') {
            return false;
        }
        cursor = end + 1;
    }
}

ecs_entity_t ecs_rest_request_entity(const sihttp_request_t *req) {
    uint32_t index = 0;
    return rest_path_uint_segment(req, 1, UINT32_MAX, &index) && index
               ? ecs_entity_from_index(index)
               : 0;
}

bool ecs_rest_request_component(const sihttp_request_t *req, ecs_component_t *component) {
    uint32_t value = 0;
    if (!rest_path_uint_segment(req, 3, UINT16_MAX, &value) || !value ||
        value >= ecs_component_count() || !ecs_component_info((ecs_component_t)value)) {
        return false;
    }

    *component = (ecs_component_t)value;
    return true;
}

bool ecs_rest_request_relation(const sihttp_request_t *req, ecs_relation_id_t *relation) {
    uint32_t value = 0;
    if (!rest_path_uint_segment(req, 3, UINT16_MAX, &value) || !value ||
        value >= ecs_relation_count() || !ecs_relation_info((ecs_relation_id_t)value)) {
        return false;
    }

    *relation = (ecs_relation_id_t)value;
    return true;
}

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body) {
    char *json = sijson_stringify(body);
    if (!json) {
        const char *fallback = "{\"error\":\"failed to serialize response\"}";
        json = malloc(strlen(fallback) + 1);
        if (json) {
            memcpy(json, fallback, strlen(fallback) + 1);
        }
        status = 500;
    }

    sihttp_response_t response = { 0 };
    response.status = status;
    response.body = json;
    response.body_size = json ? strlen(json) : 0;
    response.content_type = SIHTTP_CONTENT_JSON;
    return response;
}

sihttp_response_t ecs_rest_error_response(int status, const char *message) {
    sijson_clean();

    sijson_value_t body = sijson_make_object();
    sijson_object_set(body, "error", sijson_make_string(message));

    return ecs_rest_json_response(status, body);
}

sihttp_response_t ecs_rest_binary_response(void *data, size_t size) {
    return (sihttp_response_t){
        .status = 200,
        .body = data,
        .body_size = size,
        .content_type = SIHTTP_CONTENT_BINARY,
    };
}
