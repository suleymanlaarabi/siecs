#ifndef SIECS_CPP_C_TYPES_TEST_H
#define SIECS_CPP_C_TYPES_TEST_H

#include <siecs.h>

ECS_COMPONENT_DECLARE(cpp_c_position, {
    int value;
});

ECS_RESOURCE_DECLARE(cpp_c_time, {
    float dt;
});

#ifdef __cplusplus
extern "C" {
#endif

ECS_RELATION_DECLARE(cpp_c_parent);

extern uint32_t cpp_c_position_on_set_calls;
extern uint32_t cpp_c_time_on_set_calls;

#ifdef __cplusplus
}
#endif

#endif
