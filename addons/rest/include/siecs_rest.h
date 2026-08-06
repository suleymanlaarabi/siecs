#ifndef SIECS_REST_H
#define SIECS_REST_H

#include <siecs.h>

#ifdef __cplusplus

extern "C" {
#endif

/* Configuration for the SIECS REST explorer module. */
ECS_MODULE_DECLARE(SiecsRest, {
  const char *host;
  int port;
  int backlog;
  int max_requests_per_poll;
});

#ifdef __cplusplus
}
#endif

#endif
