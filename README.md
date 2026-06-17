![SIECS](assets/banner.png)

[![Documentation](https://img.shields.io/badge/docs-siecs-blue?style=for-the-badge&color=blue)](https://suleymanlaarabi.github.io/siecs/)
[![actions](https://img.shields.io/github/actions/workflow/status/suleymanlaarabi/siecs/ci.yml?branch=main&style=for-the-badge)](https://github.com/suleymanlaarabi/siecs/actions?query=workflow%3ACI)

`siecs` is an entity component system with a compact C API inspired by the way Flecs lets you describe data, create entities, and run logic over matching component sets.

- Fast core with zero runtime dependencies.
- Modern type-safe C++23 API.
- Built-in support for entity hierarchies and entity relations.
- C modules for grouping components, systems, and observers behind one import.
- Integrated reflection framework with JSON serialization and deserialization.

## Why C?

C is an extremely simple, predictable language that compiles quickly. It also provides a stable and widely supported ABI, making SIECS easy to use from many other languages.

* Simple and widely supported ABI.
* Fast compilation times.
* Few hidden abstractions.
* Easy to integrate into existing projects.
* C is a stupidly simple language. Less time is spent thinking about complex abstractions, and more time is spent designing the ECS itself.

SIECS is designed to remain extensible at runtime. Important parts of the system remain represented during execution rather than existing only for the compiler.

* Runtime extensibility comes first.
* Less logic locked away inside the compiler.
* Less reliance on language-specific features.
* More control over what actually exists at runtime.
* Components and behaviors can be added, discovered and manipulated dynamically.

# C++
```cpp
#include <siecs_cpp/siecs_cpp.hpp>

struct Position {
    float x, y;
};

struct Velocity {
    float x, y;
};

int main() {
    ecs::world world;

    world.entity().set(Position{ 0, 0 }).set(Velocity{ 10, 10 });

    world.system("Move")
        .phase(EcsOnUpdate)
        .each([](Position &pos, const Velocity &vel) {
            pos.x += vel.x;
            pos.y += vel.y;
        });

    world.progress();
}
```

# C
```c
#include <siecs.h>

ECS_COMPONENT(Position, {
    float x;
    float y;
});

ECS_COMPONENT(Velocity, {
    float x;
    float y;
});

void Move(ecs_iter_t *it) {
    Position *positions = ecs_field(it, 0);
    Velocity *velocities = ecs_field(it, 1);

    for (uint32_t i = 0; i < it->count; i++) {
        positions[i].x += velocities[i].x;
        positions[i].y += velocities[i].y;
    }
}

int main(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_system(world, {
        .query.terms = { ecs_inout(Position), ecs_in(Velocity) },
        .callback = Move,
        .phase = EcsOnUpdate,
    });

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, Position, {0, 0});
    ecs_set(world, entity, Velocity, {1, 1});

    while (ecs_progress(world)) {}

    ecs_fini(world);
}
```

# C Modules
Modules can wrap the component and system setup from the C example:

```c
ECS_MODULE_DECLARE(physics, {
    float gravity;
});

ECS_MODULE_DEFINE(physics);

void physics_import(ecs_world_t *world, const physics_props_t *props) {
    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_system(world, {
        .name = "Move",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_inout(Position), ecs_in(Velocity) } },
        .callback = Move,
    });
}

int main(void) {
    ecs_world_t *world = ecs_init();

    ecs_module_id_t Physics = ECS_MODULE_IMPORT(world, physics, {
        .gravity = 9.81f,
    });

    ecs_module_disable(world, Physics);
    ecs_module_enable(world, Physics);

    ecs_fini(world);
}
```


## CMake Example

C
```cmake
cmake_minimum_required(VERSION 3.21)

project(my_app LANGUAGES C)

include(FetchContent)

FetchContent_Declare(
  siecs
  GIT_REPOSITORY https://github.com/suleymanlaarabi/siecs.git
  GIT_TAG main
)

FetchContent_MakeAvailable(siecs)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE siecs::siecs)
```

C++
```cmake
cmake_minimum_required(VERSION 3.21)

project(my_app LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
  siecs
  GIT_REPOSITORY https://github.com/suleymanlaarabi/siecs.git
  GIT_TAG main
)

FetchContent_MakeAvailable(siecs)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE siecs::siecs_cpp)
```

## Bake Example

C
```json
{
  "id": "my_app",
  "type": "application",
  "value": {
    "use": ["siecs"]
  },
  "lang.c": {
    "c-standard": "c23"
  },
  "bundle": {
    "repositories": {
      "siecs": "https://github.com/suleymanlaarabi/siecs"
    }
  }
}
```

C++
```json
{
  "id": "my_app",
  "type": "application",
  "value": {
    "language": "cpp",
    "use": ["siecs_cpp", "siecs"]
  },
  "lang.cpp": {
    "cpp-standard": "c++23"
  },
  "bundle": {
    "repositories": {
      "siecs": "https://github.com/suleymanlaarabi/siecs",
      "siecs_cpp": "https://github.com/suleymanlaarabi/siecs"
    }
  }
}
```
