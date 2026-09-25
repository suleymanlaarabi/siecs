#ifndef SIECS_SIGPU_H
#define SIECS_SIGPU_H
#include "sigpu/input.h"
#include "sigpu/interaction.h"
#include "sigpu/rendering.h"
#ifdef __cplusplus
extern "C" {
#endif
SIECS_PUBLIC_API uint16_t sigpu_component_id(const char *name);
SIECS_PUBLIC_API uint16_t sigpu_resource_id(const char *name);
SIECS_PUBLIC_API sireflect_handle_t sigpu_resource_type(const char *name);
#ifdef __cplusplus
}
#endif
#endif
