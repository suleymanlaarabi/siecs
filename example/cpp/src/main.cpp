#include "siecs.h"
#include "siecs/cpp/world.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <siecs_rest.h>
#include <siecs_spatial.h>

struct Position {};
struct Enemy {};

int main() {
    ecs::init();
    ecs::import<sirest>();
    ecs::import<sispatial>();

    ecs::progress();
    return 0;
}
