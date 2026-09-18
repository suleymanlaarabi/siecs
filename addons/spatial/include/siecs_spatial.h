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
DEFINE_VEC3(Velocity3d);
DEFINE_VEC3(GlobalPosition3d);
ECS_COMPONENT_DECLARE_CPP(
    Rotation3d,
    ECS_CPP_FIELDS(float pitch; float yaw; float roll;),
    ECS_CPP_METHODS(
        Rotation3d() : pitch(0.0f),
        yaw(0.0f),
        roll(0.0f) {
        } Rotation3d(float pitch_value, float yaw_value, float roll_value) : pitch(pitch_value),
        yaw(yaw_value),
        roll(roll_value){}
    )
);
ECS_COMPONENT_DECLARE_CPP(
    GlobalOrientation3d,
    ECS_CPP_FIELDS(float x; float y; float z; float w;),
    ECS_CPP_METHODS(
        GlobalOrientation3d() : x(0.0f),
        y(0.0f),
        z(0.0f),
        w(1.0f) {} GlobalOrientation3d(
            float x_value,
            float y_value,
            float z_value,
            float w_value
        ) : x(x_value),
        y(y_value),
        z(z_value),
        w(w_value){}
    )
);
DEFINE_VEC3(Scale3d);
DEFINE_VEC3(GlobalScale3d);

typedef struct Direction3d {
    float x;
    float y;
    float z;
} Direction3d;

#ifdef __cplusplus
extern "C" {
#endif

SIECS_API Direction3d sispatial_forward_3d(const GlobalOrientation3d *orientation);

#ifdef __cplusplus
}
#endif

ECS_TAG_DECLARE(Static);
ECS_MODULE_DECLARE(sispatial, { uint8_t _unused; });

#endif
