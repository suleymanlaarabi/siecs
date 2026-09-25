#include <cmath>
#include <cstdio>
#include <test.h>
#include <vector>

#include "../../../example/gpu_city/src/traffic.cpp"

namespace t = gpu_city::traffic;

struct traffic_scope {
    explicit traffic_scope(bool parallel = false) {
        ecs::init({ .worker_threads = uint16_t(parallel ? 4 : 0) });
        (void)ecs::import<sispatial>();
        t::register_components();
        t::register_systems();
        t::build_network();
    }
    ~traffic_scope() { ecs::fini(); }
};

static void step() {
    ecs::run_phase(EcsPreUpdate);
    ecs::run_phase(EcsOnUpdate);
    ecs::run_phase(EcsPostUpdate);
}

static ecs::entity make_car(int column, int row, int direction, float progress) {
    auto lane = t::lane_from(t::junction(column, row), direction);
    auto car = ecs::entity::create();
    test_true(t::place_car(car, lane, progress));
    return car;
}

static bool overlap(const t::Pose &a, const t::Pose &b) {
    const float dx = b.x - a.x, dz = b.z - a.z;
    if (dx * dx + dz * dz > 25)
        return false;
    const float axes[4][2] = {
        { -std::sin(a.yaw), -std::cos(a.yaw) },
        { std::cos(a.yaw), -std::sin(a.yaw) },
        { -std::sin(b.yaw), -std::cos(b.yaw) },
        { std::cos(b.yaw), -std::sin(b.yaw) },
    };
    for (const auto &axis : axes) {
        const float ap = std::abs(axis[0] * axes[0][0] + axis[1] * axes[0][1]) * 1.8275f +
                         std::abs(axis[0] * axes[1][0] + axis[1] * axes[1][1]) * 0.93f;
        const float bp = std::abs(axis[0] * axes[2][0] + axis[1] * axes[2][1]) * 1.8275f +
                         std::abs(axis[0] * axes[3][0] + axis[1] * axes[3][1]) * 0.93f;
        if (std::abs(dx * axis[0] + dz * axis[1]) >= ap + bp - 0.03f)
            return false;
    }
    return true;
}

static void assert_safe() {
    std::vector<t::Pose> poses;
    std::vector<ecs::entity> cars;
    ecs::query().exclude<t::Waiting>().each([&](ecs::entity car, const t::Motion &motion) {
        cars.push_back(car);
        poses.push_back(motion.current);
    });
    for (size_t a = 0; a < poses.size(); ++a)
        for (size_t b = a + 1; b < poses.size(); ++b)
            if (overlap(poses[a], poses[b])) {
                std::fprintf(
                    stderr,
                    "overlap cars=%llu,%llu stage=%d,%d lane=%llu,%llu crossing=%llu,%llu "
                    "pos=%.2f,%.2f pose=(%.2f,%.2f),(%.2f,%.2f)\n",
                    (unsigned long long)cars[a].id(),
                    (unsigned long long)cars[b].id(),
                    int(cars[a].has<t::Driving>()),
                    int(cars[b].has<t::Driving>()),
                    (unsigned long long)cars[a].target<t::OnLane>().id(),
                    (unsigned long long)cars[b].target<t::OnLane>().id(),
                    (unsigned long long)cars[a].target<t::Crossing>().id(),
                    (unsigned long long)cars[b].target<t::Crossing>().id(),
                    cars[a].get<t::Motion>().progress,
                    cars[b].get<t::Motion>().progress,
                    poses[a].x,
                    poses[a].z,
                    poses[b].x,
                    poses[b].z
                );
                test_false(true);
            }
    ecs::query().each([](ecs::entity node, const t::Junction &) {
        test_true(ecs_relation_sources(node.id(), ecs::relation<t::Crossing>()).count <= 1);
    });
}

void traffic_following_gap(void) {
    traffic_scope scope;
    auto front = make_car(5, 5, t::East, 7.8f);
    auto back = make_car(5, 5, t::East, 0);
    for (int i = 0; i < 90; ++i) {
        step();
        if (front.target<t::OnLane>().id() == back.target<t::OnLane>().id() &&
            front.target<t::OnLane>())
            test_true(
                front.get<t::Motion>().progress - back.get<t::Motion>().progress >=
                t::VehicleLength + 0.98f
            );
        assert_safe();
    }
}

void traffic_red_amber_and_green(void) {
    traffic_scope scope;
    auto car = make_car(1, 0, t::South, 7.8f);
    auto node = t::junction(1, 1);
    for (int i = 0; i < 120; ++i)
        step();
    test_true(car.has<t::Driving>());
    test_true(car.get<t::Motion>().progress <= t::EdgeLength - t::VehicleLength * 0.5f);
    test_uint(0, ecs_relation_sources(node.id(), ecs::relation<t::Crossing>()).count);
    node.get_mut<t::Junction>().cycle = 14.1f;
    step();
    test_uint(0, ecs_relation_sources(node.id(), ecs::relation<t::Crossing>()).count);
    node.get_mut<t::Junction>().cycle = 8.0f;
    for (int i = 0; i < 60 && car.has<t::Driving>(); ++i)
        step();
    test_true(car.has<t::Turning>());
    test_uint(car.id(), ecs_relation_sources(node.id(), ecs::relation<t::Crossing>()).entities[0]);
}

void traffic_late_green_waits(void) {
    traffic_scope scope;
    auto car = make_car(0, 1, t::East, t::EdgeLength - t::VehicleLength - 1.0f);
    auto node = t::junction(1, 1);
    car.get_mut<t::Motion>().outgoing = t::South;
    node.get_mut<t::Junction>().cycle = 5.8f;
    for (int i = 0; i < 15; ++i) {
        step();
        test_true(car.has<t::Driving>());
        test_uint(0, ecs_relation_sources(node.id(), ecs::relation<t::Crossing>()).count);
    }
}

void traffic_exclusive_junction(void) {
    traffic_scope scope;
    auto first = make_car(1, 2, t::East, 7.8f);
    auto second = make_car(2, 3, t::North, 7.8f);
    auto node = t::junction(2, 2);
    for (int i = 0; i < 10 && !ecs_relation_sources(node.id(), ecs::relation<t::Crossing>()).count;
         ++i)
        step();
    auto owners = ecs_relation_sources(node.id(), ecs::relation<t::Crossing>());
    test_uint(1, owners.count);
    test_true(owners.entities[0] == first.id() || owners.entities[0] == second.id());
    for (int i = 0; i < 150; ++i) {
        step();
        test_true(ecs_relation_sources(node.id(), ecs::relation<t::Crossing>()).count <= 1);
    }
    assert_safe();
}

void traffic_turns_and_roundabout(void) {
    traffic_scope scope;
    auto turn = make_car(1, 2, t::East, 7.8f);
    turn.get_mut<t::Motion>().outgoing = t::South;
    constexpr int center = t::Grid / 2;
    auto circle = make_car(center - 1, center, t::East, 7.8f);
    circle.get_mut<t::Motion>().outgoing = t::South;
    test_true(t::junction(center, center).get<t::Junction>().roundabout);
    bool turned = false, circled = false;
    for (int i = 0; i < 150; ++i) {
        step();
        turned |= turn.get<t::Motion>().incoming == t::South;
        circled |= circle.get<t::Motion>().incoming == t::South;
        assert_safe();
    }
    test_true(turned);
    test_true(circled);
}

void traffic_turn_paths_are_smooth(void) {
    traffic_scope scope;
    for (auto node : { t::junction(2, 2), t::junction(t::Grid / 2, t::Grid / 2) }) {
        for (int incoming = 0; incoming < 4; ++incoming) {
            auto approach = t::lane_from(
                t::junction(
                    node.get<t::Junction>().column - t::dx(incoming),
                    node.get<t::Junction>().row - t::dz(incoming)
                ),
                incoming
            );
            for (int outgoing = 0; outgoing < 4; ++outgoing) {
                if ((outgoing + 2) % 4 == incoming)
                    continue;
                const float length = t::crossing_length(node, incoming, outgoing);
                auto first = t::crossing_pose(node, incoming, outgoing, 0);
                auto last = t::crossing_pose(node, incoming, outgoing, length);
                auto entry = t::lane_pose(approach, t::EdgeLength);
                auto exit = t::lane_pose(t::lane_from(node, outgoing), 0);
                test_true(std::hypot(first.x - entry.x, first.z - entry.z) < 0.001f);
                test_true(std::hypot(last.x - exit.x, last.z - exit.z) < 0.001f);
                test_true(std::abs(std::remainder(first.yaw - entry.yaw, 2 * t::Pi)) < 0.001f);
                test_true(std::abs(std::remainder(last.yaw - exit.yaw, 2 * t::Pi)) < 0.001f);
                auto previous = first;
                for (int sample = 1; sample <= 100; ++sample) {
                    auto pose =
                        t::crossing_pose(node, incoming, outgoing, length * sample / 100.0f);
                    const float change =
                        std::abs(std::remainder(pose.yaw - previous.yaw, 2 * t::Pi));
                    if (change >= 0.30f)
                        std::fprintf(
                            stderr,
                            "turn jump node=%d,%d in=%d out=%d sample=%d delta=%.3f\n",
                            node.get<t::Junction>().column,
                            node.get<t::Junction>().row,
                            incoming,
                            outgoing,
                            sample,
                            change
                        );
                    test_true(change < 0.30f);
                    previous = pose;
                }
            }
        }
    }
}

void traffic_border_respawns(void) {
    traffic_scope scope;
    auto car = make_car(1, 5, t::West, 7.8f);
    car.get_mut<t::Motion>().outgoing = t::West;
    bool left = false, returned = false;
    for (int i = 0; i < 300 && !returned; ++i) {
        step();
        left |= car.has<t::Leaving>() || car.has<t::Waiting>();
        returned |= left && car.has<t::Driving>() && bool(car.target<t::OnLane>());
    }
    test_true(left);
    test_true(returned);
    test_true(car.has<t::Driving>());
    test_true(car.target<t::OnLane>().is_alive());
}

void traffic_800_cars_no_overlap(void) {
    traffic_scope scope;
    constexpr int count_to_check = 800;
    t::seed_cars(ecs::entity::null(), count_to_check);
    uint32_t count = 0;
    ecs::query().each([&](const t::Motion &) { ++count; });
    test_uint(count_to_check, count);
    for (int i = 0; i < 900; ++i) {
        step();
        assert_safe();
    }
    uint32_t remaining = 0;
    ecs::query().each([&](const t::Motion &) { ++remaining; });
    test_uint(count_to_check, remaining);
}

void traffic_long_run_relation_integrity(void) {
    traffic_scope scope(true);
    t::seed_cars(ecs::entity::null(), 800);
    for (int tick = 0; tick < 5000; ++tick) {
        step();
        ecs::query().exclude<t::Waiting>().each([&](ecs::entity car, const t::Motion &motion) {
            if (car.has<t::Turning>() && motion.progress == 0)
                test_true(
                    t::light(car.target<t::Crossing>().get<t::Junction>(), motion.incoming) !=
                    t::Red
                );
            if (!car.has<t::Driving>())
                return;
            auto lane = car.target<t::OnLane>();
            test_true(lane.is_alive());
            bool found = false;
            const auto sources = ecs_relation_sources(lane.id(), ecs::relation<t::OnLane>());
            for (uint32_t i = 0; i < sources.count; ++i)
                found |= sources.entities[i] == car.id();
            if (!found)
                std::fprintf(
                    stderr,
                    "missing OnLane source at tick %d car %llu lane %llu\n",
                    tick,
                    (unsigned long long)car.id(),
                    (unsigned long long)lane.id()
                );
            test_true(found);
        });
    }
}
