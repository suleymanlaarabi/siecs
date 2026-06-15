---
title: Getting Started
description: Minimal SIECS program using typed components.
---

This page shows the normal user flow and the supported ways to consume SIECS.

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

## pkg-config Example

Install SIECS once from the repository checkout:

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local -DSIECS_BUILD_CPP=ON
cmake --build build
cmake --install build
```

C
```sh
cc -std=c23 main.c $(pkg-config --cflags --libs siecs)
```

C++
```sh
c++ -std=c++23 main.cpp $(pkg-config --cflags --libs siecs-cpp)
```

## Manual Source Build

If you use a custom build system, build the C sources from `src/`, expose
`include/` to users, and link the three public dependencies: `sireflect`,
`sijson`, and `sihttp`.

Required compile definitions:

- `siecs_STATIC`
- `SIECS_REST` only when the REST addon is enabled

The C++ API is header-only and only needs `addons/siecs_cpp/include` plus the C
library.

## Minimal Program

The runtime flow is:

1. Create a world.
2. Register component types.
3. Create entities.
4. Set or read component data.
5. Destroy the world.

```c
#include <siecs.h>

ECS_COMPONENT_DECLARE(Position, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(Position);

int main(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t entity = ecs_new(world);

    ecs_set(world, entity, Position, {
        .x = 10.0f,
        .y = 20.0f,
    });

    Position *position = ecs_get(world, entity, Position);
    position->x += 1.0f;

    ecs_fini(world);
    return 0;
}
```

## Include Path

Applications include the public header:

```c
#include <siecs.h>
```

The old `ecs/world.h` header is not part of the current public API.

## Build With Bake

Bake is the maintainer build for this repository and remains supported for
consuming Bake projects.

For local development in this repository:

```sh
bake rebuild
bake rebuild test
bake run test
```

## Important Contracts

`ecs_get()` assumes the component exists on the entity. Use `ecs_try_get()` when
the component may be absent.

`ecs_set()` adds the component when needed, then writes the value.

Entities passed to API functions must come from the same world.
