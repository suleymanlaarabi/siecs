#include "city.hpp"

#include <algorithm>
#include <cmath>

namespace gpu_city {

void build_city() {
    auto city = ecs::entity::create("City");
    city.add<Static>();

    const float half = (CitySize - 1) / 2.0f;
    float streets[CitySize];
    float blocks[CitySize - 1];
    for (int i = 0; i < CitySize; ++i)
        streets[i] = (i - half) * BlockSpacing;
    for (int i = 0; i < CitySize - 1; ++i)
        blocks[i] = (streets[i] + streets[i + 1]) / 2.0f;

    for (int row = 0; row < CitySize; ++row) {
        for (int column = 0; column < CitySize - 1; ++column)
            make_road_section(blocks[column], streets[row], true);
        for (int column = 0; column < CitySize; ++column)
            make_road_intersection(
                streets[column],
                streets[row],
                column,
                row,
                (column == CitySize / 2 && row == CitySize / 2) ||
                    (column == CitySize / 2 - 2 && row == CitySize / 2) ||
                    (column == CitySize / 2 + 2 && row == CitySize / 2) ||
                    (column == CitySize / 2 && row == CitySize / 2 + 2)
            );
    }
    for (int row = 0; row < CitySize - 1; ++row)
        for (int column = 0; column < CitySize; ++column)
            make_road_section(streets[column], blocks[row], false);

    for (int row = 0; row < CitySize - 1; ++row) {
        for (int column = 0; column < CitySize - 1; ++column) {
            const float x = blocks[column], z = blocks[row];
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
            constexpr float plots[4][2] = {
                { -3.2f, -3.35f },
                { 3.2f, -3.35f },
                { -3.2f, 3.35f },
                { 3.2f, 3.35f },
            };
            for (int i = 0; i < plot_count; ++i) {
                const float px = x + plots[i][0], pz = z + plots[i][1];
                const int floors = int(downtown) + ((column * 7 + row * 3 + i * 5) % 3);
                const int facade = (column * 3 + row * 5 + i) % 5;
                make_building(
                    px,
                    pz,
                    floors + 2,
                    Facades[facade],
                    plots[i][1] < 0 ? 3.14159265f : 0.0f
                );
            }
        }
    }

    geometry::box(GroundSize, 0.3f, GroundSize, { 70, 80, 105 }, 0.0f, -1.5f, 0.0f);
}

} // namespace gpu_city
