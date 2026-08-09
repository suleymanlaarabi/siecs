#pragma once
#include "siecs.h"

namespace ecs {

/** Fluent builder for custom dynamic phases in C++. */
class phase {
    ecs_phase_desc_t desc{};
    mutable ecs_phase_t _id = 0;

  public:
    explicit phase(const char *name = "unnamed") {
        desc.name = name;
    }

    phase &after(ecs_phase_t p) {
        desc.after = p;
        return *this;
    }

    phase &before(ecs_phase_t p) {
        desc.before = p;
        return *this;
    }

    ecs_phase_t id() const {
        if (!_id) {
            _id = ecs_phase_init(&desc);
        }
        return _id;
    }

    operator ecs_phase_t() const {
        return id();
    }
};

} // namespace ecs
