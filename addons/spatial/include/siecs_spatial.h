#ifndef SIECS_SPATIAL_H
#define SIECS_SPATIAL_H

#include <siecs.h>

#define DEFINE_VEC2(cname)                                                                         \
    ECS_COMPONENT_DECLARE_CPP(                                                                     \
        cname,                                                                                     \
        ECS_CPP_FIELDS(float x; float y;),                                                         \
        ECS_CPP_METHODS(                                                                           \
            cname() : x(0.0f),                                                                     \
            y(0.0f) {} cname(float x_value, float y_value) : x(x_value),                           \
            y(y_value) {} explicit cname(float value) : x(value),                                  \
            y(value){}                                                                             \
        )                                                                                          \
    )

#define DEFINE_VEC3(cname)                                                                         \
    ECS_COMPONENT_DECLARE_CPP(                                                                     \
        cname,                                                                                     \
        ECS_CPP_FIELDS(float x; float y; float z;),                                                \
        ECS_CPP_METHODS(                                                                           \
            cname() : x(0.0f),                                                                     \
            y(0.0f),                                                                               \
            z(0.0f) {} cname(float x_value, float y_value, float z_value) : x(x_value),            \
            y(y_value),                                                                            \
            z(z_value) {} explicit cname(float value) : x(value),                                  \
            y(value),                                                                              \
            z(value){}                                                                             \
        )                                                                                          \
    )

#define DEFINE_F32(cname)                                                                          \
    ECS_COMPONENT_DECLARE_CPP(                                                                     \
        cname,                                                                                     \
        ECS_CPP_FIELDS(float value;),                                                              \
        ECS_CPP_METHODS(cname() : value(0.0f) {} explicit cname(float value) : value(value){})     \
    );

DEFINE_VEC2(Position2d);
DEFINE_VEC2(Velocity2d);
DEFINE_VEC2(GlobalPosition2d);
DEFINE_VEC2(Scale2d);
DEFINE_VEC2(GlobalScale2d);
DEFINE_F32(Rotation2d);
DEFINE_F32(GlobalRotation2d);

DEFINE_VEC3(Position3d);
DEFINE_VEC2(Velocity3d);
DEFINE_VEC3(GlobalPosition3d);
DEFINE_VEC3(Rotation3d);
DEFINE_VEC3(GlobalRotation3d);
DEFINE_VEC3(Scale3d);
DEFINE_VEC3(GlobalScale3d);

ECS_TAG_DECLARE(Static);
ECS_MODULE_DECLARE(sispatial, { uint8_t _unused; });

#endif
