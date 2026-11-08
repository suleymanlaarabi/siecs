#include <siecs.h>
#include <string>
#include <test.h>

struct PairOwned {
    static inline int alive;
    std::string text;
    PairOwned() { alive++; }
    explicit PairOwned(std::string value) : text(std::move(value)) { alive++; }
    PairOwned(const PairOwned &value) : text(value.text) { alive++; }
    PairOwned(PairOwned &&value) noexcept : text(std::move(value.text)) { alive++; }
    PairOwned &operator=(const PairOwned &) = default;
    PairOwned &operator=(PairOwned &&) = default;
    ~PairOwned() { alive--; }
};

static ecs_entity_t observed_entity;
static ecs_pair_id_t observed_pair;
static ecs_component_t observed_payload;
static unsigned observations;
static void pair_owned_snapshot(ecs_observer_event_t *event) {
    const auto *data = static_cast<const ecs_pair_event_t *>(event->trigger_data);
    test_uint(event->entity, observed_entity);
    test_uint(data->pair, observed_pair);
    test_uint(data->payload, observed_payload);
    test_str(static_cast<const PairOwned *>(data->value)->text.c_str(), "retained");
    test_false(ecs_pair_has_owned(observed_entity, observed_pair));
    observations++;
}

void pair_native_lifecycle(void) {
    test_int(PairOwned::alive, 0);
    {
        PairOwned source("retained");
        ecs::init();
        ecs_component_t payload = ecs::component<PairOwned>().id();
        ecs_entity_t target = ecs_new();
        const ecs_pair_desc_t desc = { { payload, EcsPairComponent },
                                       { target, EcsPairEntity },
                                       payload };
        ecs_pair_id_t pair = ecs_pair_init(&desc);
        test_true(pair != 0);
        ecs_entity_t entities[20];
        for (unsigned i = 0; i < 20; i++) {
            entities[i] = ecs_new();
            test_true(ecs_pair_set(entities[i], pair, &source));
        }
        test_int(PairOwned::alive, 21);
        ecs_entity_t child = ecs_new();
        ecs_is_a(child, entities[0]);
        test_true(ecs_pair_has_owned(child, pair));
        test_int(PairOwned::alive, 22);
        ecs_defer_begin();
        test_true(ecs_pair_set(entities[1], pair, ecs_pair_get(entities[0], pair)));
        test_true(ecs_pair_remove(entities[2], pair));
        ecs_defer_end();
        test_int(PairOwned::alive, 21);
        test_str(
            static_cast<const PairOwned *>(ecs_pair_get(child, pair))->text.c_str(),
            "retained"
        );
        observed_entity = child;
        observed_pair = pair;
        observed_payload = payload;
        observations = 0;
        ecs_observer_desc_t observer = {};
        observer.entity = child;
        observer.callback = pair_owned_snapshot;
        observer.on = EcsOnPairSet;
        ecs_observer_init(&observer);
        observer.on = EcsOnPairRemove;
        ecs_observer_init(&observer);
        ecs_observer_batch_begin();
        test_true(ecs_pair_set(child, pair, &source));
        test_true(ecs_pair_remove(child, pair));
        test_uint(observations, 0);
        ecs_observer_batch_end();
        test_uint(observations, 2);
        test_int(PairOwned::alive, 20);
        ecs_kill(target);
        test_int(PairOwned::alive, 1);
        ecs::fini();
        test_int(PairOwned::alive, 1);
    }
    test_int(PairOwned::alive, 0);
}
