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

#ifdef __cplusplus
}

extern "C" {
#endif

ECS_COMPONENT_DECLARE_CPP(
    cpp_c_method_position,
    ECS_CPP_FIELDS(
        int value;
    ),
    ECS_CPP_METHODS(
        int doubled() const { return value * 2; }
        void reset() { value = 0; }
    )
);

ECS_RESOURCE_DECLARE_CPP(
    cpp_c_method_time,
    ECS_CPP_FIELDS(
        float dt;
    ),
    ECS_CPP_METHODS(
        bool valid() const { return dt > 0.0f; }
    )
);

extern uint32_t cpp_c_position_on_set_calls;
extern uint32_t cpp_c_time_on_set_calls;

#ifdef __cplusplus
}
#endif

#endif
