#ifndef SIECS_CPP_TEST_H
#define SIECS_CPP_TEST_H

#include <cstddef>
#include <cstdint>
#include <stdint.h>

#include <bake_test.h>
#include <siecs.h>

struct ecs_test_scope {
    ecs_test_scope() { ecs::init(); }
    ~ecs_test_scope() { ecs::fini(); }
};

#endif
