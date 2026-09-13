#ifndef SIECS_REST_H
#define SIECS_REST_H

#include <siecs.h>
#include <sihttp.h>

#ifdef __cplusplus

extern "C" {
#endif

/* Configuration for the SIECS REST explorer module. */
ECS_MODULE_DECLARE(sirest, {
  const char *host;
  int port;
  int backlog;
  int max_requests_per_poll;
  size_t max_scene_bytes;
  bool in_process;
});

sihttp_response_t sirest_dispatch(
    sihttp_method_t method,
    const char *path,
    const char *body
);

sihttp_response_t sirest_dispatch_bytes(
    sihttp_method_t method,
    const char *path,
    const void *data,
    size_t size
);

sihttp_response_t ecs_rest_binary_response(void *data, size_t size);

/* Explorer routes:
 * GET    /schema
 * GET    /scene
 * POST   /scene
 * POST   /modules
 * GET    /entities
 * GET    /entities/all
 * POST   /entities
 * GET    /entities/:index
 * GET    /entities/:index/children
 * GET    /entities/:index/relations
 * PUT    /entities/:index/relations/:relation
 * DELETE /entities/:index/relations/:relation
 * POST   /entities/:index/components/:component
 * PUT    /entities/:index/components/:component
 * DELETE /entities/:index/components/:component
 *
 * POST /scene instantiates the saved scene into the current world. It does not
 * clear or replace entities that already exist.
 * POST /modules accepts a Linux shared object as application/octet-stream,
 * loads it, and invokes its exported ecs_module_import entry point. A later
 * upload disables the previously uploaded module before activating the new one.
 */

#ifdef __cplusplus
}
#endif

#endif
