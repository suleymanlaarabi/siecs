#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "rest_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

sihttp_response_t ecs_rest_post_modules(const sihttp_request_t *req) {
    if (!req || !req->body || req->body_size == 0)
        return ecs_rest_error_response(400, "invalid module");

    SiecsRestState *state = ecs_try_get_resource(SiecsRestState);
    if (state && state->max_scene_bytes && req->body_size > state->max_scene_bytes)
        return ecs_rest_error_response(413, "module too large");

#ifdef _WIN32
    return ecs_rest_error_response(501, "dynamic module upload requires Linux");
#else
    char directory[] = "/tmp/siecs-module-XXXXXX";
    if (!mkdtemp(directory))
        return ecs_rest_error_response(500, "failed to create module directory");

    char library_path[sizeof(directory) + sizeof("/module.so")];
    char module_path[sizeof(directory) + sizeof("/module")];
    snprintf(library_path, sizeof(library_path), "%s/module.so", directory);
    snprintf(module_path, sizeof(module_path), "%s/module", directory);

    FILE *file = fopen(library_path, "wb");
    if (!file) {
        rmdir(directory);
        return ecs_rest_error_response(500, "failed to create module file");
    }

    bool written = fwrite(req->body, 1, req->body_size, file) == req->body_size;
    bool closed = fclose(file) == 0;
    if (!written || !closed) {
        unlink(library_path);
        rmdir(directory);
        return ecs_rest_error_response(500, "failed to write module file");
    }

    ecs_module_id_t replaced = state ? state->loaded_module : 0;
    if (replaced)
        ecs_module_disable(replaced);

    ecs_module_id_t module = ecs_module_load(module_path);
    unlink(library_path);
    rmdir(directory);

    if (!module) {
        if (replaced)
            ecs_module_enable(replaced);
        return ecs_rest_error_response(422, "failed to load module");
    }

    state = ecs_try_get_resource(SiecsRestState);
    if (state)
        state->loaded_module = module;

    sijson_clean();
    sijson_value_t body = sijson_make_object();
    sijson_object_set(body, "id", sijson_make_number(module));
    sijson_object_set(body, "name", sijson_make_string(ecs_module_name(module)));
    sijson_object_set(body, "enabled", sijson_make_bool(ecs_module_is_enabled(module)));
    sijson_object_set(body, "replaced", sijson_make_number(replaced));
    return ecs_rest_json_response(201, body);
#endif
}
