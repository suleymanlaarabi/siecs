#ifndef TEST_H
#define TEST_H

#include <stddef.h>

#include "siecs_test.h"

#ifdef __cplusplus
#include <siecs.h>
struct ecs_test_scope {
    ecs_test_scope() { ecs::init(); }
    ~ecs_test_scope() { ecs::fini(); }
};
#endif

#endif
