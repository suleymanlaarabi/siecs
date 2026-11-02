#include "city.hpp"

#include <cstdio>
#include <cstdlib>
#include <siecs_script.h>

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
    ecs_script_eval_result_t result = {};
    const int status = ecs_script_run_file(filename, &result);
    if (status) {
        std::fprintf(
            stderr,
            "%s:%d:%d: %s\n",
            filename,
            result.line,
            result.column,
            result.error ? result.error : "city script failed"
        );
    }
    std::free(result.error);
    return status == 0;
}

} // namespace gpu_city
