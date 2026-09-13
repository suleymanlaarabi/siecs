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
  bool in_process;
});

sihttp_response_t sirest_dispatch(
    sihttp_method_t method,
    const char *path,
    const char *body
);

/* Explorer routes:
 * GET    /schema
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
 */

#ifdef __cplusplus
}
#endif

#endif
