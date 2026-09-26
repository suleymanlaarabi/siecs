#include "traffic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <siecs/cpp/system.hpp>

namespace gpu_city::traffic {
namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float Gap = VehicleLength + 1.0f;
constexpr float Stop = EdgeLength - Gap;
constexpr float CircleRadius = 2.45f;
constexpr float ConnectorLength = 1.2f;
constexpr float Acceleration = 12.0f;
constexpr float CruiseSpeed = 8.0f;

struct Vec2 {
    float x, z;
};
struct PathPoint {
    Vec2 position, tangent;
};

Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.z + b.z }; }
Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.z - b.z }; }
Vec2 operator*(Vec2 v, float s) { return { v.x * s, v.z * s }; }

int dx(int d) { return d == East ? 1 : d == West ? -1 : 0; }
int dz(int d) { return d == South ? 1 : d == North ? -1 : 0; }
Vec2 direction(int d) { return { float(dx(d)), float(dz(d)) }; }
Vec2 right(Vec2 v) { return { -v.z, v.x }; }
float yaw(Vec2 v) { return std::atan2(-v.x, -v.z); }
float x_at(int column) { return (column - (Grid - 1) * 0.5f) * Spacing; }
float z_at(int row) { return (row - (Grid - 1) * 0.5f) * Spacing; }
Vec2 center(const Junction &j) { return { x_at(j.column), z_at(j.row) }; }

uint32_t random(uint32_t value) {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}

PathPoint bezier(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float u) {
    const float v = 1.0f - u;
    return { p0 * (v * v * v) + p1 * (3 * v * v * u) + p2 * (3 * v * u * u) + p3 * (u * u * u),
             (p1 - p0) * (v * v) + (p2 - p1) * (2 * v * u) + (p3 - p2) * (u * u) };
}

Pose road_pose(const Junction &j, int d, float progress) {
    const Vec2 forward = direction(d);
    const Vec2 p = center(j) + forward * (HalfStreet + progress) + right(forward) * LaneOffset;
    return { p.x, p.z, yaw(forward) };
}

Pose lane_pose(ecs::entity lane, float progress) {
    return road_pose(
        lane.target<FromJunction>().get<Junction>(),
        lane.get<Lane>().direction,
        progress
    );
}

float roundabout_sweep(int incoming, int outgoing) {
    const Vec2 in = direction(incoming), out = direction(outgoing);
    const Vec2 first_point = in * -HalfStreet + right(in) * LaneOffset;
    const Vec2 last_point = out * HalfStreet + right(out) * LaneOffset;
    const float first = std::atan2(first_point.z, first_point.x);
    float last = std::atan2(last_point.z, last_point.x);
    while (last >= first)
        last -= 2 * Pi;
    return first - last;
}

int turn_sign(int incoming, int outgoing) {
    return dx(incoming) * dz(outgoing) - dz(incoming) * dx(outgoing);
}

float crossing_length(ecs::entity node, int incoming, int outgoing) {
    if (node.get<Junction>().roundabout)
        return 2 * ConnectorLength + CircleRadius * (roundabout_sweep(incoming, outgoing) - 0.44f);
    if (incoming == outgoing)
        return 2 * HalfStreet;
    return (HalfStreet - turn_sign(incoming, outgoing) * LaneOffset) * Pi * 0.5f;
}

float crossing_speed(const Junction &node, const Motion &motion) {
    if (node.roundabout)
        return 5.0f;
    return motion.incoming == motion.outgoing ? CruiseSpeed : 4.5f;
}

float time_to_entry(const Motion &motion, float cruise) {
    const float distance = EdgeLength - motion.progress;
    const float speed = std::min(motion.speed, cruise);
    const float accelerating = (cruise - speed) / Acceleration;
    const float covered = (speed + cruise) * accelerating * 0.5f;
    if (distance <= covered)
        return (std::sqrt(speed * speed + 2 * Acceleration * distance) - speed) / Acceleration;
    return accelerating + (distance - covered) / cruise;
}

PathPoint roundabout_path(int incoming, int outgoing, float distance) {
    const Vec2 in = direction(incoming), out = direction(outgoing);
    const Vec2 start = in * -HalfStreet + right(in) * LaneOffset;
    const Vec2 end = out * HalfStreet + right(out) * LaneOffset;
    const float first = std::atan2(start.z, start.x);
    const float arc_start = first - 0.22f;
    const float arc_end = first - roundabout_sweep(incoming, outgoing) + 0.22f;
    const Vec2 a = { CircleRadius * std::cos(arc_start), CircleRadius * std::sin(arc_start) };
    const Vec2 b = { CircleRadius * std::cos(arc_end), CircleRadius * std::sin(arc_end) };
    const float arc_length = CircleRadius * (arc_start - arc_end);

    if (distance >= ConnectorLength && distance <= ConnectorLength + arc_length) {
        const float angle = arc_start - (distance - ConnectorLength) / CircleRadius;
        return { { CircleRadius * std::cos(angle), CircleRadius * std::sin(angle) },
                 { std::sin(angle), -std::cos(angle) } };
    }

    const bool entering = distance < ConnectorLength;
    const float u = std::clamp(
        entering ? distance / ConnectorLength
                 : (distance - ConnectorLength - arc_length) / ConnectorLength,
        0.0f,
        1.0f
    );
    const Vec2 arc_start_tangent = { -std::sin(arc_start), std::cos(arc_start) };
    const Vec2 arc_end_tangent = { std::sin(arc_end), -std::cos(arc_end) };
    return entering ? bezier(start, start + in * 0.35f, a + arc_start_tangent * 0.35f, a, u)
                    : bezier(b, b + arc_end_tangent * 0.35f, end - out * 0.35f, end, u);
}

Pose crossing_pose(ecs::entity node, int incoming, int outgoing, float distance) {
    const auto &j = node.get<Junction>();
    if (j.roundabout) {
        const auto [p, tangent] = roundabout_path(incoming, outgoing, distance);
        const Vec2 world = center(j) + p;
        return { world.x, world.z, yaw(tangent) };
    }

    const Vec2 in = direction(incoming);
    const Vec2 start = in * -HalfStreet + right(in) * LaneOffset;
    Vec2 p, tangent;
    if (incoming == outgoing) {
        p = start + in * distance;
        tangent = in;
    } else {
        const int turn = turn_sign(incoming, outgoing);
        const float radius = HalfStreet - turn * LaneOffset;
        const Vec2 pivot = start + right(in) * (turn * radius);
        const float angle = turn * distance / radius;
        const float c = std::cos(angle), s = std::sin(angle);
        const Vec2 from_pivot = start - pivot;
        p = pivot +
            Vec2{ from_pivot.x * c - from_pivot.z * s, from_pivot.x * s + from_pivot.z * c };
        tangent = { in.x * c - in.z * s, in.x * s + in.z * c };
    }
    p = center(j) + p;
    return { p.x, p.z, yaw(tangent) };
}

bool entrance_clear(ecs::entity lane, float at, float clearance = Gap) {
    const auto cars = ecs_relation_sources(lane.id(), ecs::relation<OnLane>());
    for (uint32_t i = 0; i < cars.count; ++i) {
        const auto car = ecs::entity::from(cars.entities[i]);
        if (car.has<Driving>() && std::abs(car.get<Motion>().progress - at) < clearance)
            return false;
    }
    return true;
}

ecs::entity front_car(ecs::entity lane, float &progress) {
    ecs::entity front;
    progress = -1;
    const auto cars = ecs_relation_sources(lane.id(), ecs::relation<OnLane>());
    for (uint32_t i = 0; i < cars.count; ++i) {
        const auto car = ecs::entity::from(cars.entities[i]);
        if (!car.has<Driving>())
            continue;
        const float p = car.get<Motion>().progress;
        if (p > progress) {
            front = car;
            progress = p;
        }
    }
    return front;
}

float lane_limit(ecs::entity lane, ecs::entity car, float progress, float limit) {
    const auto cars = ecs_relation_sources(lane.id(), ecs::relation<OnLane>());
    for (uint32_t i = 0; i < cars.count; ++i) {
        const auto ahead = ecs::entity::from(cars.entities[i]);
        if (ahead.id() == car.id() || !ahead.has<Driving>())
            continue;
        const float p = ahead.get<Motion>().prior_progress;
        if (p > progress)
            limit = std::min(limit, p - Gap);
    }
    return limit;
}

int choose_turn(ecs::entity car, int incoming, uint16_t turns) {
    constexpr int turns_by_choice[] = { 0, 1, 3 };
    const uint32_t seed = uint32_t(car.id() >> 32) ^ (uint32_t(turns) * 0x9e3779b9u);
    return (incoming + turns_by_choice[random(seed) % 3]) % 4;
}

bool try_border_entry(ecs::entity car, Clock &clock) {
    for (int attempt = 0; attempt < Grid * 4; ++attempt) {
        const int border =
            int(random(uint32_t(car.id() >> 32) ^ clock.tick ^ (uint32_t(attempt) * 0x9e3779b9u)) %
                (Grid * 4));
        const int side = border / Grid, offset = border % Grid;
        const int column = side == 1 ? Grid - 1 : side == 3 ? 0 : offset;
        const int row = side == 0 ? 0 : side == 2 ? Grid - 1 : offset;
        const int d = (side + 1) % 4;
        const auto node = junction(column, row);
        const auto lane = lane_from(node, d);
        if (!lane || ecs_relation_sources(node.id(), ecs::relation<Crossing>()).count ||
            !entrance_clear(lane, 0, Gap + 8 * 0.8f))
            continue;
        if (place_car(car, lane, 0)) {
            car.relate<Crossing>(node);
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
    ecs::component<Driving>();
    ecs::component<Turning>();
    ecs::component<Leaving>();
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
    const auto lanes = ecs_relation_sources(node.id(), ecs::relation<FromJunction>());
    for (uint32_t i = 0; i < lanes.count; ++i) {
        const auto lane = ecs::entity::from(lanes.entities[i]);
        if (lane.get<Lane>().direction == direction)
            return lane;
    }
    return {};
}

void build_network() {
    constexpr int center = Grid / 2;
    for (int row = 0; row < Grid; ++row) {
        for (int column = 0; column < Grid; ++column) {
            char name[32];
            std::snprintf(name, sizeof name, "City junction %d %d", column, row);
            const bool roundabout =
                (column == center && row == center) || (column == center - 2 && row == center) ||
                (column == center + 2 && row == center) || (column == center && row == center + 2);
            ecs::entity::create(name).set(
                Junction{ uint8_t(column), uint8_t(row), roundabout, !roundabout, 0 }
            );
        }
    }

    for (int row = 0; row < Grid; ++row) {
        for (int column = 0; column < Grid; ++column) {
            const auto from = junction(column, row);
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
    const bool horizontal = direction == East || direction == West;
    const float phase = horizontal ? t : std::fmod(t + 8.0f, 16.0f);
    return phase < 6.0f ? 6.0f - phase : 0.0f;
}

bool place_car(ecs::entity car, ecs::entity lane, float progress) {
    if (!lane || !entrance_clear(lane, progress))
        return false;
    const Pose pose = lane_pose(lane, progress);
    const int d = lane.get<Lane>().direction;
    car.set(
           Motion{
               .progress = progress,
               .prior_progress = progress,
               .speed = 0,
               .incoming = uint8_t(d),
               .outgoing = uint8_t(choose_turn(car, d, 0)),
               .previous = pose,
               .current = pose,
           }
    )
        .set(Position3d{ pose.x, -1.26f, pose.z }, Rotation3d{ 0, pose.yaw, 0 })
        .relate<OnLane>(lane)
        .add<Driving>()
        .remove<Waiting>();
    return true;
}

void seed_cars(ecs::entity prefab, int count) {
    uint32_t seed = 0x4a831f53u;
    for (int i = 0; i < count; ++i) {
        ecs::entity lane;
        float progress = 0;
        for (int attempt = 0; attempt < Grid * Grid * 8; ++attempt) {
            const int column = int((seed = random(seed)) % Grid);
            const int row = int((seed = random(seed)) % Grid);
            const auto node = junction(column, row);
            lane = lane_from(node, int((seed = random(seed)) % 4));
            progress = 2.0f + float((seed = random(seed)) % 6);
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

void register_systems() {
    ecs::system("Traffic tick")
        .phase(EcsPreUpdate)
        .interval(StepSeconds)
        .each([](ecs::res<Clock> clock) {
            ++clock->tick;
            clock->since_tick = 0;
            clock->spawned = false;
        });

    ecs::system("Traffic signals")
        .phase(EcsPreUpdate)
        .interval(StepSeconds)
        .each([](Junction &node) { node.cycle = std::fmod(node.cycle + StepSeconds, 16.0f); });

    ecs::system("Traffic lane snapshot")
        .phase(EcsPreUpdate)
        .interval(StepSeconds)
        .require<Driving>()
        .each([](Motion &motion) { motion.prior_progress = motion.progress; });

    ecs::system("Traffic junction grants")
        .phase(EcsPreUpdate)
        .interval(StepSeconds)
        .each([](ecs::entity node, const Junction &junction) {
            if (ecs_relation_sources(node.id(), ecs::relation<Crossing>()).count)
                return;
            ecs::entity winner, next_lane;
            float best_progress = -1;
            const auto lanes = ecs_relation_sources(node.id(), ecs::relation<ToJunction>());
            for (uint32_t i = 0; i < lanes.count; ++i) {
                const auto lane = ecs::entity::from(lanes.entities[i]);
                float progress;
                const auto front = front_car(lane, progress);
                if (!front || progress < 2.0f)
                    continue;
                const auto &motion = front.get<Motion>();
                const float speed = crossing_speed(junction, motion);
                if (green_remaining(junction, motion.incoming) <
                    time_to_entry(motion, speed) + 0.5f)
                    continue;
                const auto outgoing = lane_from(node, motion.outgoing);
                if (outgoing && !entrance_clear(outgoing, 0))
                    continue;
                if (progress > best_progress ||
                    (progress == best_progress && (!winner || front.id() < winner.id()))) {
                    winner = front;
                    next_lane = outgoing;
                    best_progress = progress;
                }
            }
            if (winner) {
                winner.relate<Crossing>(node);
                if (next_lane)
                    winner.relate<NextLane>(next_lane);
            }
        });

    ecs::system("Traffic leaving city")
        .phase(EcsOnUpdate)
        .interval(StepSeconds)
        .require<Leaving>()
        .each([](ecs::entity car, Motion &motion, Position3d &position) {
            motion.previous = motion.current;
            motion.progress += CruiseSpeed * StepSeconds;
            motion.current.x += dx(motion.outgoing) * CruiseSpeed * StepSeconds;
            motion.current.z += dz(motion.outgoing) * CruiseSpeed * StepSeconds;
            if (motion.progress < VehicleLength)
                return;
            motion.current = motion.previous = { 10000, 10000, 0 };
            position.x = position.z = 10000;
            car.remove<Leaving>().add<Waiting>();
        });

    ecs::system("Traffic through junctions")
        .phase(EcsOnUpdate)
        .interval(StepSeconds)
        .require<Turning>()
        .with_relation<Crossing>()
        .each([](ecs::entity car, Motion &motion) {
            motion.previous = motion.current;
            const auto node = car.target<Crossing>();
            const float length = crossing_length(node, motion.incoming, motion.outgoing);
            const float cruise = crossing_speed(node.get<Junction>(), motion);
            motion.speed = std::min(cruise, motion.speed + Acceleration * StepSeconds);
            motion.progress = std::min(length, motion.progress + motion.speed * StepSeconds);
            motion.current = crossing_pose(node, motion.incoming, motion.outgoing, motion.progress);
            if (motion.progress < length)
                return;
            motion.progress = 0;
            if (const auto next = car.target<NextLane>()) {
                motion.incoming = motion.outgoing;
                motion.outgoing = uint8_t(choose_turn(car, motion.incoming, ++motion.turns));
                motion.current = lane_pose(next, 0);
                car.relate<OnLane>(next)
                    .unrelate<NextLane>()
                    .unrelate<Crossing>()
                    .remove<Turning>()
                    .add<Driving>();
            } else {
                car.unrelate<Crossing>().remove<Turning>().add<Leaving>();
            }
        });

    ecs::system("Traffic on lanes")
        .phase(EcsOnUpdate)
        .interval(StepSeconds)
        .require<Driving>()
        .with_relation<OnLane>()
        .each([](ecs::entity car, Motion &motion) {
            motion.previous = motion.current;
            const auto lane = car.target<OnLane>();
            const auto node = lane.target<ToJunction>();
            const bool permitted = car.target<Crossing>().id() == node.id();
            const float lead_limit =
                lane_limit(lane, car, motion.progress, std::numeric_limits<float>::infinity());
            const float limit = std::min(lead_limit, permitted ? EdgeLength : Stop);
            const float distance = std::max(0.0f, limit - motion.progress);
            const float cruise =
                permitted ? crossing_speed(node.get<Junction>(), motion) : CruiseSpeed;
            const float braking_distance =
                permitted ? std::max(0.0f, lead_limit - motion.progress) : distance;
            const float target = std::min(cruise, std::sqrt(24.0f * braking_distance));
            const float speed = std::clamp(
                target,
                motion.speed - Acceleration * StepSeconds,
                motion.speed + Acceleration * StepSeconds
            );
            const float next =
                std::max(motion.progress, std::min(limit, motion.progress + speed * StepSeconds));
            motion.speed = (next - motion.progress) / StepSeconds;
            motion.progress = next;
            motion.current = lane_pose(lane, next);

            if (permitted && next >= EdgeLength - 0.0001f) {
                motion.progress = 0;
                motion.current = crossing_pose(node, motion.incoming, motion.outgoing, 0);
                car.unrelate<OnLane>().remove<Driving>().add<Turning>();
            } else if (
                car.target<Crossing>().id() == lane.target<FromJunction>().id() &&
                next >= VehicleLength
            ) {
                car.unrelate<Crossing>();
            }
        });

    ecs::system("Traffic border entries")
        .phase(EcsPostUpdate)
        .interval(StepSeconds)
        .require<Waiting>()
        .each([](ecs::entity car, ecs::res<Clock> clock) {
            if (!clock->spawned)
                clock->spawned = try_border_entry(car, *clock);
        });

    ecs::system("Traffic interpolation clock")
        .phase(EcsPreRender)
        .each([](ecs::res<Clock> clock, ecs::res<const DeltaTime> delta) {
            clock->since_tick = std::min(StepSeconds, clock->since_tick + delta->value);
        });

    ecs::system("Traffic render interpolation")
        .phase(EcsPreRender)
        .exclude<Waiting>()
        .each([](const Motion &motion,
                 Position3d &position,
                 Rotation3d &rotation,
                 ecs::res<const Clock> clock) {
            const float alpha = clock->since_tick / StepSeconds;
            position.x = motion.previous.x + (motion.current.x - motion.previous.x) * alpha;
            position.y = -1.26f;
            position.z = motion.previous.z + (motion.current.z - motion.previous.z) * alpha;
            const float dyaw = std::remainder(motion.current.yaw - motion.previous.yaw, 2 * Pi);
            rotation.yaw = motion.previous.yaw + dyaw * alpha;
        });
}

} // namespace gpu_city::traffic
