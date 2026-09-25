#include "city.hpp"

#include <algorithm>
#include <cmath>

namespace gpu_city {

void build_city() {
    const float half = (CitySize - 1) / 2.0f;
    const auto street = [half](int i) { return (i - half) * BlockSpacing; };
    const auto block = [half](int i) { return (i + 0.5f - half) * BlockSpacing; };

    for (int row = 0; row < CitySize; ++row) {
        for (int column = 0; column < CitySize - 1; ++column)
            make_road_section(block(column), street(row), true);
        for (int column = 0; column < CitySize; ++column)
            make_road_intersection(
                street(column),
                street(row),
                column,
                row,
                traffic::junction(column, row).get<traffic::Junction>().roundabout
            );
    }
    for (int row = 0; row < CitySize - 1; ++row)
        for (int column = 0; column < CitySize; ++column)
            make_road_section(street(column), block(row), false);

    for (int row = 0; row < CitySize - 1; ++row) {
        for (int column = 0; column < CitySize - 1; ++column) {
            const float x = block(column), z = block(row);
            geometry::box(
                BlockSpacing - StreetWidth,
                0.18f,
                BlockSpacing - StreetWidth,
                Paving,
                x,
                PavementY - 0.09f,
                z
            );
            if ((column * 5 + row * 7) % 11 == 0) {
                make_park(x, z);
                continue;
            }
            const int plot_count = (column + row) % 3 == 0 ? 4 : 3;
            const float downtown = std::max(
                0.0f,
                3.0f - std::floor(std::max(std::abs(x), std::abs(z)) / BlockSpacing)
            );
            for (int i = 0; i < plot_count; ++i) {
                const float px = x + (i & 1 ? 3.2f : -3.2f);
                const float pz = z + (i & 2 ? 3.35f : -3.35f);
                const int floors = int(downtown) + ((column * 7 + row * 3 + i * 5) % 3);
                const int facade = (column * 3 + row * 5 + i) % 5;
                make_building(px, pz, floors + 2, Facades[facade], i & 2 ? 0.0f : 3.14159265f);
            }
        }
    }

    geometry::box(GroundSize, 0.3f, GroundSize, { 70, 80, 105 }, 0.0f, -1.5f, 0.0f);
}

} // namespace gpu_city
