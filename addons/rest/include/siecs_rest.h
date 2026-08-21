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

#ifdef __cplusplus
}
#endif

#endif
