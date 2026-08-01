#include "siecs/config.h"
#if SIECS_HAS_REST
#include "rest_internal.h"
#include <stdlib.h>
#include <string.h>

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body) {
    char *json = sijson_stringify(body);
    if (!json) {
        json = strdup("{\"error\":\"failed to serialize response\"}");
        status = 500;
    }

    sihttp_response_t response = {
        .status = status,
        .body = json,
        .content_type = SIHTTP_CONTENT_JSON,
    };
    return response;
}

sihttp_response_t ecs_rest_error_response(int status, const char *message) {
    sijson_clean();

    sijson_value_t body = sijson_make_object();
    sijson_object_set(body, "error", sijson_make_string(message));

    return ecs_rest_json_response(status, body);
}
#endif
