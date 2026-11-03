#include "city.hpp"

#include <cstdio>
#include <siecs_script/cpp.hpp>

namespace gpu_city {

bool build_city(const char *filename) {
    ecs::ensure("CitySettings")
        .set(
            CityLayout{
                CitySize,
                BlockSpacing,
                StreetWidth,
                RoadY,
                PavementY,
            }
        );
    const auto result = ecs::script::run_file(filename);
    if (!result) {
        const auto &error = result.error();
        std::fprintf(
            stderr,
            "%s:%d:%d: %s\n",
            filename,
            error.line,
            error.column,
            error.message.c_str()
        );
    }
    return bool(result);
}

} // namespace gpu_city
