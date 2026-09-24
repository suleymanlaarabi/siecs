#include "traffic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <siecs/cpp/query.hpp>

namespace gpu_city::traffic {
namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float Gap = VehicleLength + 1.0f;
constexpr float Stop = EdgeLength - Gap;
constexpr float CircleRadius = 2.45f;
constexpr float ConnectorLength = 1.2f;

int dx(int d) { return d == East ? 1 : d == West ? -1 : 0; }
int dz(int d) { return d == South ? 1 : d == North ? -1 : 0; }
float x_at(int column) { return (column - (Grid - 1) * 0.5f) * Spacing; }
float z_at(int row) { return (row - (Grid - 1) * 0.5f) * Spacing; }

uint32_t random(Clock &clock) {
    clock.random ^= clock.random << 13;
    clock.random ^= clock.random >> 17;
    clock.random ^= clock.random << 5;
    return clock.random;
}

Pose lane_pose(ecs::entity lane, float progress) {
    const auto from_entity = lane.target<FromJunction>();
    const auto &from = from_entity.get<Junction>();
    const int d = lane.get<Lane>().direction;
    const float x = x_at(from.column) + dx(d) * (HalfStreet + progress) - dz(d) * LaneOffset;
    const float z = z_at(from.row) + dz(d) * (HalfStreet + progress) + dx(d) * LaneOffset;
    return { x, z, std::atan2(float(-dx(d)), float(-dz(d))) };
}

float roundabout_sweep(int incoming, int outgoing) {
    const float first = std::atan2(
        -dz(incoming) * HalfStreet + dx(incoming) * LaneOffset,
        -dx(incoming) * HalfStreet - dz(incoming) * LaneOffset
    );
    float last = std::atan2(
        dz(outgoing) * HalfStreet + dx(outgoing) * LaneOffset,
        dx(outgoing) * HalfStreet - dz(outgoing) * LaneOffset
    );
    while (last >= first)
        last -= 2 * Pi;
    return first - last;
}

float crossing_length(ecs::entity node, int incoming, int outgoing) {
    if (node.get<Junction>().roundabout)
        return 2 * ConnectorLength + CircleRadius * (roundabout_sweep(incoming, outgoing) - 0.44f);
    if (incoming == outgoing)
        return 2 * HalfStreet;
    const int turn = dx(incoming) * dz(outgoing) - dz(incoming) * dx(outgoing);
    return (HalfStreet - turn * LaneOffset) * Pi * 0.5f;
}

Pose crossing_pose(ecs::entity node, int incoming, int outgoing, float distance) {
    const auto &junction = node.get<Junction>();
    const float sx = -dx(incoming) * HalfStreet - dz(incoming) * LaneOffset;
    const float sz = -dz(incoming) * HalfStreet + dx(incoming) * LaneOffset;
    const float ex = dx(outgoing) * HalfStreet - dz(outgoing) * LaneOffset;
    const float ez = dz(outgoing) * HalfStreet + dx(outgoing) * LaneOffset;
    float x, z, tx, tz;
    if (junction.roundabout) {
        const float first = std::atan2(sz, sx);
        const float arc_start = first - 0.22f;
        const float arc_end = first - roundabout_sweep(incoming, outgoing) + 0.22f;
        const float ax = CircleRadius * std::cos(arc_start),
                    az = CircleRadius * std::sin(arc_start);
        const float bx = CircleRadius * std::cos(arc_end), bz = CircleRadius * std::sin(arc_end);
        const float arc_length = CircleRadius * (arc_start - arc_end);
        if (distance < ConnectorLength || distance > ConnectorLength + arc_length) {
            const bool entering = distance < ConnectorLength;
            const float u = std::clamp(
                entering ? distance / ConnectorLength
                         : (distance - ConnectorLength - arc_length) / ConnectorLength,
                0.0f,
                1.0f
            );
            const float p0x = entering ? sx : bx, p0z = entering ? sz : bz;
            const float p1x = entering ? sx + dx(incoming) * 0.35f : bx + std::sin(arc_end) * 0.35f;
            const float p1z = entering ? sz + dz(incoming) * 0.35f : bz - std::cos(arc_end) * 0.35f;
            const float p2x =
                entering ? ax - std::sin(arc_start) * 0.35f : ex - dx(outgoing) * 0.35f;
            const float p2z =
                entering ? az + std::cos(arc_start) * 0.35f : ez - dz(outgoing) * 0.35f;
            const float p3x = entering ? ax : ex, p3z = entering ? az : ez;
            const float v = 1 - u;
            x = v * v * v * p0x + 3 * v * v * u * p1x + 3 * v * u * u * p2x + u * u * u * p3x;
            z = v * v * v * p0z + 3 * v * v * u * p1z + 3 * v * u * u * p2z + u * u * u * p3z;
            tx = v * v * (p1x - p0x) + 2 * v * u * (p2x - p1x) + u * u * (p3x - p2x);
            tz = v * v * (p1z - p0z) + 2 * v * u * (p2z - p1z) + u * u * (p3z - p2z);
        } else {
            const float angle = arc_start - (distance - ConnectorLength) / CircleRadius;
            x = CircleRadius * std::cos(angle);
            z = CircleRadius * std::sin(angle);
            tx = std::sin(angle);
            tz = -std::cos(angle);
        }
    } else if (incoming == outgoing) {
        x = sx + dx(incoming) * distance;
        z = sz + dz(incoming) * distance;
        tx = dx(incoming);
        tz = dz(incoming);
    } else {
        const int turn = dx(incoming) * dz(outgoing) - dz(incoming) * dx(outgoing);
        const float radius = HalfStreet - turn * LaneOffset;
        const float cx = sx - dz(incoming) * turn * radius;
        const float cz = sz + dx(incoming) * turn * radius;
        const float angle = turn * distance / radius;
        const float vx = sx - cx, vz = sz - cz;
        x = cx + vx * std::cos(angle) - vz * std::sin(angle);
        z = cz + vx * std::sin(angle) + vz * std::cos(angle);
        tx = dx(incoming) * std::cos(angle) - dz(incoming) * std::sin(angle);
        tz = dx(incoming) * std::sin(angle) + dz(incoming) * std::cos(angle);
    }
    return { x_at(junction.column) + x, z_at(junction.row) + z, std::atan2(-tx, -tz) };
}

bool entrance_clear(ecs::entity lane, float at, float clearance = Gap) {
    const auto sources = ecs_relation_sources(lane.id(), ecs::relation<OnLane>());
    for (uint32_t i = 0; i < sources.count; ++i) {
        auto car = ecs::entity::from(sources.entities[i]);
        if (car.has<Waiting>())
            continue;
        if (std::abs(car.get<Motion>().progress - at) < clearance)
            return false;
    }
    return true;
}

int choose_turn(ecs::entity node, int incoming, Clock &clock) {
    const int choices[3] = { incoming, (incoming + 1) % 4, (incoming + 3) % 4 };
    const int first = int(random(clock) % 3);
    for (int i = 0; i < 3; ++i) {
        const int direction = choices[(first + i) % 3];
        const auto &j = node.get<Junction>();
        const int column = int(j.column) + dx(direction);
        const int row = int(j.row) + dz(direction);
        if (column < 0 || column >= Grid || row < 0 || row >= Grid)
            return direction; // Leave the city.
        if (lane_from(node, direction))
            return direction;
    }
    return incoming;
}

bool try_border_entry(ecs::entity car, Clock &clock) {
    for (int attempt = 0; attempt < Grid * 4; ++attempt) {
        const int border = int(random(clock) % (Grid * 4));
        const int column = border < Grid       ? border
                           : border < 2 * Grid ? Grid - 1
                           : border < 3 * Grid ? border - 2 * Grid
                                               : 0;
        const int row = border < Grid       ? 0
                        : border < 2 * Grid ? border - Grid
                        : border < 3 * Grid ? Grid - 1
                                            : border - 3 * Grid;
        const int direction = border < Grid       ? South
                              : border < 2 * Grid ? West
                              : border < 3 * Grid ? North
                                                  : East;
        auto node = junction(column, row);
        auto lane = lane_from(node, direction);
        if (!lane || ecs_relation_sources(node.id(), ecs::relation<Crossing>()).count ||
            !entrance_clear(lane, 0, Gap + 8 * 0.8f))
            continue;
        if (place_car(car, lane, 0)) {
            car.relate<Crossing>(node);
            ++clock.entries;
            return true;
        }
    }
    return false;
}

} // namespace

void register_components() {
    ecs::component<Junction>();
    ecs::component<Lane>();
    ecs::component<Motion>();
    ecs::component<Waiting>();
    ecs::relation<OnLane>();
    ecs::relation<FromJunction>();
    ecs::relation<ToJunction>();
    ecs::relation<Crossing>();
    ecs::relation<NextLane>();
    ecs::set_resource(Clock{});
}

ecs::entity junction(int column, int row) {
    char name[32];
    std::snprintf(name, sizeof name, "City junction %d %d", column, row);
    return ecs::entity::lookup(name);
}

ecs::entity lane_from(ecs::entity node, int direction) {
    if (!node)
        return {};
    const auto sources = ecs_relation_sources(node.id(), ecs::relation<FromJunction>());
    for (uint32_t i = 0; i < sources.count; ++i) {
        auto lane = ecs::entity::from(sources.entities[i]);
        if (lane.get<Lane>().direction == direction)
            return lane;
    }
    return {};
}

void build_network() {
    for (int row = 0; row < Grid; ++row)
        for (int column = 0; column < Grid; ++column) {
            char name[32];
            std::snprintf(name, sizeof name, "City junction %d %d", column, row);
            const bool roundabout = (column == 16 && row == 16) || (column == 14 && row == 16) ||
                                    (column == 18 && row == 16) || (column == 16 && row == 18);
            ecs::entity::create(name).set(
                Junction{ uint8_t(column),
                          uint8_t(row),
                          roundabout,
                          !roundabout && column % 2 && row % 2,
                          0 }
            );
        }
    for (int row = 0; row < Grid; ++row)
        for (int column = 0; column < Grid; ++column) {
            auto from = junction(column, row);
            for (int d = 0; d < 4; ++d) {
                const int next_column = column + dx(d), next_row = row + dz(d);
                if (next_column < 0 || next_column >= Grid || next_row < 0 || next_row >= Grid)
                    continue;
                ecs::entity::create()
                    .set(Lane{ uint8_t(d) })
                    .relate<FromJunction>(from)
                    .relate<ToJunction>(junction(next_column, next_row));
            }
        }
}

Light light(const Junction &junction, int direction) {
    if (!junction.signal)
        return Green;
    const float t = std::fmod(junction.cycle, 16.0f);
    const float phase = t < 8 ? t : t - 8;
    const bool active = (t < 8) == (direction == East || direction == West);
    return active && phase < 6 ? Green : active && phase < 7.5f ? Amber : Red;
}

float green_remaining(const Junction &junction, int direction) {
    if (!junction.signal)
        return 1000.0f;
    const float t = std::fmod(junction.cycle, 16.0f);
    const float phase = (direction == East || direction == West) ? t : std::fmod(t + 8.0f, 16.0f);
    return phase < 6.0f ? 6.0f - phase : 0.0f;
}

bool place_car(ecs::entity car, ecs::entity lane, float progress) {
    if (!lane || !entrance_clear(lane, progress))
        return false;
    const Pose pose = lane_pose(lane, progress);
    const int direction = lane.get<Lane>().direction;
    car.set(
           Motion{ .progress = progress,
                   .old_progress = progress,
                   .speed = 0,
                   .arrival = 0,
                   .incoming = uint8_t(direction),
                   .outgoing = 255,
                   .stage = Road,
                   .previous = pose,
                   .current = pose }
    )
        .set(Position3d{ pose.x, -1.26f, pose.z }, Rotation3d{ 0, pose.yaw, 0 })
        .relate<OnLane>(lane)
        .remove<Waiting>();
    return true;
}

void seed_cars(ecs::entity prefab, int count) {
    auto &clock = ecs::resource<Clock>();
    for (int i = 0; i < count; ++i) {
        ecs::entity lane;
        float progress = 0;
        for (int attempt = 0; attempt < Grid * Grid * 8; ++attempt) {
            auto node = junction(int(random(clock) % Grid), int(random(clock) % Grid));
            lane = lane_from(node, int(random(clock) % 4));
            progress = 2.0f + float(random(clock) % 6);
            if (lane && entrance_clear(lane, progress))
                break;
            lane = {};
        }
        if (!lane)
            break;
        auto car = ecs::entity::create();
        if (prefab)
            car.is_a(prefab);
        place_car(car, lane, progress);
    }
}

void step() {
    auto &clock = ecs::resource<Clock>();
    ++clock.tick;
    ecs::query().each([](Junction &node) {
        node.cycle = std::fmod(node.cycle + StepSeconds, 16.0f);
    });
    ecs::query().exclude<Waiting>().each([&](ecs::entity car, Motion &motion) {
        motion.old_progress = motion.progress;
        motion.previous = motion.current;
        if (motion.stage == Road && motion.progress >= Stop - 0.5f && !motion.arrival) {
            motion.arrival = clock.tick;
            auto lane = car.target<OnLane>();
            auto node = lane.target<ToJunction>();
            motion.outgoing = uint8_t(choose_turn(node, motion.incoming, clock));
        }
    });
    ecs::query().each([&](ecs::entity node, const Junction &junction) {
        if (ecs_relation_sources(node.id(), ecs::relation<Crossing>()).count)
            return;
        ecs::entity winner, next_lane;
        uint32_t arrival = UINT32_MAX;
        const auto incoming_lanes = ecs_relation_sources(node.id(), ecs::relation<ToJunction>());
        for (uint32_t i = 0; i < incoming_lanes.count; ++i) {
            auto lane = ecs::entity::from(incoming_lanes.entities[i]);
            const auto cars = ecs_relation_sources(lane.id(), ecs::relation<OnLane>());
            for (uint32_t c = 0; c < cars.count; ++c) {
                auto candidate = ecs::entity::from(cars.entities[c]);
                const auto &motion = candidate.get<Motion>();
                if (motion.stage != Road || !motion.arrival ||
                    light(junction, motion.incoming) != Green)
                    continue;
                const float approach_speed = junction.roundabout                  ? 5.0f
                                             : motion.outgoing == motion.incoming ? 8.0f
                                                                                  : 4.5f;
                if (green_remaining(junction, motion.incoming) <
                    (EdgeLength - motion.progress) / approach_speed + 0.5f)
                    continue;
                bool blocked = false;
                for (uint32_t ahead_index = 0; ahead_index < cars.count; ++ahead_index) {
                    const auto ahead = ecs::entity::from(cars.entities[ahead_index]);
                    if (ahead.id() != candidate.id() && ahead.get<Motion>().stage == Road &&
                        ahead.get<Motion>().progress > motion.progress) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked)
                    continue;
                auto outgoing = lane_from(node, motion.outgoing);
                if (outgoing && !entrance_clear(outgoing, 0))
                    continue;
                if (motion.arrival < arrival ||
                    (motion.arrival == arrival && (!winner || candidate.id() < winner.id()))) {
                    winner = candidate;
                    next_lane = outgoing;
                    arrival = motion.arrival;
                }
            }
        }
        if (winner) {
            winner.relate<Crossing>(node);
            if (next_lane)
                winner.relate<NextLane>(next_lane);
        }
    });
    ecs::query().exclude<Waiting>().each([&](ecs::entity car, Motion &motion) {
        if (motion.stage == Road) {
            auto lane = car.target<OnLane>();
            auto at = lane.target<ToJunction>();
            const bool permitted = car.target<Crossing>().id() == at.id();
            float limit = permitted ? EdgeLength : Stop;
            const auto cars = ecs_relation_sources(lane.id(), ecs::relation<OnLane>());
            for (uint32_t i = 0; i < cars.count; ++i) {
                auto ahead = ecs::entity::from(cars.entities[i]);
                if (ahead.id() == car.id())
                    continue;
                const auto &other = ahead.get<Motion>();
                if (other.stage == Road && other.old_progress > motion.old_progress)
                    limit = std::min(limit, other.old_progress - Gap);
            }
            const float distance = std::max(0.0f, limit - motion.progress);
            const float cruise =
                permitted && (at.get<Junction>().roundabout || motion.outgoing != motion.incoming)
                    ? (at.get<Junction>().roundabout ? 5.0f : 4.5f)
                    : 8.0f;
            const float target = std::min(cruise, std::sqrt(24.0f * distance));
            const float speed = std::clamp(
                target,
                motion.speed - 12 * StepSeconds,
                motion.speed + 12 * StepSeconds
            );
            const float next =
                std::max(motion.progress, std::min(limit, motion.progress + speed * StepSeconds));
            motion.speed = (next - motion.progress) / StepSeconds;
            motion.progress = next;
            motion.current = lane_pose(lane, motion.progress);
            if (permitted && motion.progress >= EdgeLength - 0.0001f) {
                motion.stage = Intersection;
                motion.progress = 0;
                motion.current = crossing_pose(at, motion.incoming, motion.outgoing, 0);
                car.unrelate<OnLane>();
            } else if (
                car.target<Crossing>().id() == lane.target<FromJunction>().id() &&
                motion.progress >= VehicleLength
            ) {
                car.unrelate<Crossing>();
            }
        } else if (motion.stage == Intersection) {
            auto node = car.target<Crossing>();
            const float length = crossing_length(node, motion.incoming, motion.outgoing);
            const float cruise = node.get<Junction>().roundabout      ? 5.0f
                                 : motion.incoming == motion.outgoing ? 8.0f
                                                                      : 4.5f;
            motion.speed = std::min(cruise, motion.speed + 12 * StepSeconds);
            motion.progress = std::min(length, motion.progress + motion.speed * StepSeconds);
            motion.current = crossing_pose(node, motion.incoming, motion.outgoing, motion.progress);
            if (motion.progress >= length) {
                auto next = car.target<NextLane>();
                motion.progress = 0;
                if (next) {
                    motion.stage = Road;
                    motion.incoming = motion.outgoing;
                    motion.outgoing = 255;
                    motion.arrival = 0;
                    motion.current = lane_pose(next, 0);
                    car.relate<OnLane>(next).unrelate<NextLane>();
                } else {
                    motion.stage = Exit;
                }
            }
        } else {
            motion.progress += 8 * StepSeconds;
            motion.speed = 8;
            const auto node = car.target<Crossing>();
            const auto &j = node.get<Junction>();
            motion.current = {
                x_at(j.column) + dx(motion.outgoing) * (HalfStreet + motion.progress) -
                    dz(motion.outgoing) * LaneOffset,
                z_at(j.row) + dz(motion.outgoing) * (HalfStreet + motion.progress) +
                    dx(motion.outgoing) * LaneOffset,
                std::atan2(float(-dx(motion.outgoing)), float(-dz(motion.outgoing)))
            };
            if (motion.progress >= VehicleLength + HalfStreet) {
                ++clock.exits;
                car.unrelate<Crossing>().add<Waiting>();
                motion.current = motion.previous = { 10000, 10000, 0 };
            }
        }
    });
    bool spawned = false;
    ecs::query().require<Waiting>().each([&](ecs::entity car, Motion &) {
        if (!spawned)
            spawned = try_border_entry(car, clock);
    });
}

void render(float alpha) {
    ecs::query().exclude<Waiting>().each(
        [&](const Motion &motion, Position3d &position, Rotation3d &rotation) {
            position.x = motion.previous.x + (motion.current.x - motion.previous.x) * alpha;
            position.y = -1.26f;
            position.z = motion.previous.z + (motion.current.z - motion.previous.z) * alpha;
            const float yaw = std::remainder(motion.current.yaw - motion.previous.yaw, 2 * Pi);
            rotation.yaw = motion.previous.yaw + yaw * alpha;
        }
    );
}

} // namespace gpu_city::traffic
