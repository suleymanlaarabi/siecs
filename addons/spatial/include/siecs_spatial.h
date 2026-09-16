#ifndef SIECS_SPATIAL_H
#define SIECS_SPATIAL_H

#include <siecs.h>

ECS_COMPONENT_DECLARE_CPP(
    Position2d,
    ECS_CPP_FIELDS(float x; float y;),
    ECS_CPP_METHODS(
        Position2d() : x(0.0f), y(0.0f) {}
        Position2d(float x_value, float y_value) : x(x_value), y(y_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    GlobalPosition2d,
    ECS_CPP_FIELDS(float x; float y;),
    ECS_CPP_METHODS(
        GlobalPosition2d() : x(0.0f), y(0.0f) {}
        GlobalPosition2d(float x_value, float y_value) : x(x_value), y(y_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    Rotation2d,
    ECS_CPP_FIELDS(float value; /* Radians. */),
    ECS_CPP_METHODS(
        Rotation2d() : value(0.0f) {}
        explicit Rotation2d(float value) : value(value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    GlobalRotation2d,
    ECS_CPP_FIELDS(float value; /* Radians. */),
    ECS_CPP_METHODS(
        GlobalRotation2d() : value(0.0f) {}
        explicit GlobalRotation2d(float value) : value(value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    Scale2d,
    ECS_CPP_FIELDS(float x; float y;),
    ECS_CPP_METHODS(
        Scale2d() : x(1.0f), y(1.0f) {}
        explicit Scale2d(float uniform) : x(uniform), y(uniform) {}
        Scale2d(float x_value, float y_value) : x(x_value), y(y_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    GlobalScale2d,
    ECS_CPP_FIELDS(float x; float y;),
    ECS_CPP_METHODS(
        GlobalScale2d() : x(1.0f), y(1.0f) {}
        explicit GlobalScale2d(float uniform) : x(uniform), y(uniform) {}
        GlobalScale2d(float x_value, float y_value) : x(x_value), y(y_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    Position3d,
    ECS_CPP_FIELDS(float x; float y; float z;),
    ECS_CPP_METHODS(
        Position3d() : x(0.0f), y(0.0f), z(0.0f) {}
        Position3d(float x_value, float y_value, float z_value)
            : x(x_value), y(y_value), z(z_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    GlobalPosition3d,
    ECS_CPP_FIELDS(float x; float y; float z;),
    ECS_CPP_METHODS(
        GlobalPosition3d() : x(0.0f), y(0.0f), z(0.0f) {}
        GlobalPosition3d(float x_value, float y_value, float z_value)
            : x(x_value), y(y_value), z(z_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    Rotation3d,
    ECS_CPP_FIELDS(float x; float y; float z; /* Euler radians, X then Y then Z. */),
    ECS_CPP_METHODS(
        Rotation3d() : x(0.0f), y(0.0f), z(0.0f) {}
        Rotation3d(float x_value, float y_value, float z_value)
            : x(x_value), y(y_value), z(z_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    GlobalRotation3d,
    ECS_CPP_FIELDS(float x; float y; float z; /* Euler radians, X then Y then Z. */),
    ECS_CPP_METHODS(
        GlobalRotation3d() : x(0.0f), y(0.0f), z(0.0f) {}
        GlobalRotation3d(float x_value, float y_value, float z_value)
            : x(x_value), y(y_value), z(z_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    Scale3d,
    ECS_CPP_FIELDS(float x; float y; float z;),
    ECS_CPP_METHODS(
        Scale3d() : x(1.0f), y(1.0f), z(1.0f) {}
        explicit Scale3d(float uniform) : x(uniform), y(uniform), z(uniform) {}
        Scale3d(float x_value, float y_value, float z_value)
            : x(x_value), y(y_value), z(z_value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    GlobalScale3d,
    ECS_CPP_FIELDS(float x; float y; float z;),
    ECS_CPP_METHODS(
        GlobalScale3d() : x(1.0f), y(1.0f), z(1.0f) {}
        explicit GlobalScale3d(float uniform) : x(uniform), y(uniform), z(uniform) {}
        GlobalScale3d(float x_value, float y_value, float z_value)
            : x(x_value), y(y_value), z(z_value) {}
    )
);

ECS_TAG_DECLARE(Static);
ECS_MODULE_DECLARE(sispatial, { uint8_t _unused; });

#endif
