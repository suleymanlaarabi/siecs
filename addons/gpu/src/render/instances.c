#include "render/render_internal.h"
packed_rotation pack_rotation(GlobalOrientation3d q) {
    return (packed_rotation){ (int16_t)roundf(q.x * 32767.0f),
                              (int16_t)roundf(q.y * 32767.0f),
                              (int16_t)roundf(q.z * 32767.0f),
                              (int16_t)roundf(q.w * 32767.0f) };
}
bool is_rotated(GlobalOrientation3d q) {
    return q.x != 0.0f || q.y != 0.0f || q.z != 0.0f || q.w != 1.0f;
}
primitive_size_t primitive_size(sigpu_primitive_t primitive, const void *component) {
    switch (primitive) {
    case SIGPU_PRIMITIVE_CUBE: {
        const Cuboid *c = component;
        return (primitive_size_t){ c->width, c->height, c->depth };
    }
    case SIGPU_PRIMITIVE_CYLINDER: {
        const Cylinder *c = component;
        return (primitive_size_t){ c->radius * 2, c->height, c->radius * 2 };
    }
    case SIGPU_PRIMITIVE_SPHERE: {
        const Sphere *s = component;
        return (primitive_size_t){ s->radius * 2, s->radius * 2, s->radius * 2 };
    }
    default:
        return (primitive_size_t){ 0, 0, 0 };
    }
}
primitive_size_t scaled_size(primitive_size_t size, GlobalScale3d scale) {
    return (primitive_size_t){ size.x * scale.x, size.y * scale.y, size.z * scale.z };
}
float primitive_radius(sigpu_primitive_t primitive, primitive_size_t size) {
    float x = fabsf(size.x) * 0.5f, y = fabsf(size.y) * 0.5f, z = fabsf(size.z) * 0.5f;
    if (primitive == SIGPU_PRIMITIVE_SPHERE)
        return fmaxf(x, fmaxf(y, z));
    if (primitive == SIGPU_PRIMITIVE_CYLINDER)
        return hypotf(fmaxf(x, z), y);
    return sqrtf(x * x + y * y + z * z);
}
Uint32 primitive_mesh(sigpu_primitive_t primitive, Uint32 lod) {
    if (primitive == SIGPU_PRIMITIVE_CUBE)
        return SIGPU_MESH_CUBE;
    return (primitive == SIGPU_PRIMITIVE_CYLINDER ? SIGPU_MESH_CYLINDER_LOW
                                                  : SIGPU_MESH_SPHERE_LOW) +
           lod;
}

sigpu_axis_instance_t
make_owned_axis(GlobalPosition3d p, primitive_size_t s, Color c, float bloom) {
    return (sigpu_axis_instance_t){ p.x,
                                    p.y,
                                    p.z,
                                    s.x,
                                    s.y,
                                    s.z,
                                    SIGPU_RENDERSETTINGS->linear_lut[c.r],
                                    SIGPU_RENDERSETTINGS->linear_lut[c.g],
                                    SIGPU_RENDERSETTINGS->linear_lut[c.b],
                                    c.a,
                                    bloom };
}
sigpu_rotated_instance_t make_owned_rotated(
    GlobalPosition3d p,
    GlobalOrientation3d q,
    primitive_size_t s,
    Color c,
    float bloom
) {
    packed_rotation r = pack_rotation(q);
    return (sigpu_rotated_instance_t){ p.x,
                                       p.y,
                                       p.z,
                                       s.x,
                                       s.y,
                                       s.z,
                                       r.x,
                                       r.y,
                                       r.z,
                                       r.w,
                                       SIGPU_RENDERSETTINGS->linear_lut[c.r],
                                       SIGPU_RENDERSETTINGS->linear_lut[c.g],
                                       SIGPU_RENDERSETTINGS->linear_lut[c.b],
                                       c.a,
                                       bloom };
}

const void *
primitive_field(ecs_iter_t *it, sigpu_primitive_t primitive, uint32_t index, ptrdiff_t stride) {
    switch (primitive) {
    case SIGPU_PRIMITIVE_CUBE:
        return &((const Cuboid *)ecs_field(it, 3))[index * stride];
    case SIGPU_PRIMITIVE_CYLINDER:
        return &((const Cylinder *)ecs_field(it, 3))[index * stride];
    case SIGPU_PRIMITIVE_SPHERE:
        return &((const Sphere *)ecs_field(it, 3))[index * stride];
    default:
        return NULL;
    }
}
