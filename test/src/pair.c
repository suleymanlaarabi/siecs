#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <test.h>

static ecs_component_t payload;
static ecs_pair_id_t pair;
static ecs_entity_t target;

static ecs_pair_element_t component_element(ecs_component_t component) {
    return (ecs_pair_element_t){ .kind = EcsPairComponent, .id = component };
}
static ecs_pair_element_t entity_element(ecs_entity_t entity) {
    return (ecs_pair_element_t){ .kind = EcsPairEntity, .id = entity };
}
static void setup(void) {
    ecs_init();
    payload = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "PairValue",
                                                                          .fields = "{ int x; }" });
    target = ecs_new_named(0, "Target");
    pair = ecs_pair_init(
        &(ecs_pair_desc_t){ component_element(payload), entity_element(target), payload }
    );
    test_true(pair != 0);
}

void pair_identity_and_multiple_targets(void) {
    setup();
    ecs_entity_t entity = ecs_new(), other = ecs_new();
    ecs_pair_desc_t desc = *ecs_pair_info(pair);
    test_uint(ecs_pair_init(&desc), pair);
    desc.payload = ecs_id(Name);
    test_uint(ecs_pair_init(&desc), 0);
    desc = *ecs_pair_info(pair);
    desc.first = entity_element(target);
    desc.second = component_element(payload);
    ecs_pair_id_t reversed = ecs_pair_init(&desc);
    test_true(reversed != 0 && reversed != pair);
    desc.first = component_element(payload);
    desc.second = entity_element(other);
    ecs_pair_id_t second = ecs_pair_init(&desc);
    int a = 7, b = 9, c = 13;
    test_true(ecs_pair_set(entity, pair, &a));
    test_true(ecs_pair_set(entity, second, &b));
    test_true(ecs_pair_set(entity, reversed, &c));
    test_int(*(const int *)ecs_pair_get(entity, pair), 7);
    test_int(*(const int *)ecs_pair_get(entity, second), 9);
    test_int(*(const int *)ecs_pair_get(entity, reversed), 13);
    test_true(ecs_pair_remove(entity, second));
    test_false(ecs_pair_has(entity, second));
    test_true(ecs_pair_has(entity, pair));
    desc.first = (ecs_pair_element_t){ ecs_rid(ChildOf), EcsPairRelation };
    test_uint(ecs_pair_init(&desc), 0);
    desc.first = (ecs_pair_element_t){ payload, EcsPairAny };
    test_uint(ecs_pair_init(&desc), 0);
    ecs_fini();
}

void pair_dense_rows_queries_and_ticks(void) {
    setup();
    ecs_component_t selected = ecs_tag_init("Selected");
    ecs_entity_t entities[40];
    for (int i = 0; i < 40; i++) {
        entities[i] = ecs_new();
        test_true(ecs_pair_set(entities[i], pair, &i));
        if (i % 3 == 0)
            ecs_add_cid(entities[i], selected);
    }
    ecs_pair_iter_t changes = ecs_pair_query_iter(NULL);
    test_true(ecs_pair_set(entities[0], pair, ecs_pair_get(entities[39], pair)));
    test_int(*(const int *)ecs_pair_get(entities[0], pair), 39);
    ecs_query_id_t query =
        ecs_query_init(&(ecs_query_desc_t){ .components = { { selected, EcsFilter } } });
    ecs_pair_query_desc_t desc = { .first = { payload, EcsPairComponent },
                                   .sources = query,
                                   .filter_sources = true,
                                   .after_tick = changes.tick };
    ecs_pair_iter_t it = ecs_pair_query_iter(&desc);
    unsigned count = 0;
    while (ecs_pair_next(&it)) {
        test_uint(it.pair, pair);
        test_uint(it.stride, sizeof(int));
        for (uint32_t row = 0; row < it.count; row++) {
            test_true(ecs_has_cid(it.entities[row], selected));
            int expected = *(const int *)ecs_pair_get(it.entities[row], pair);
            test_int(((const int *)it.values)[row], expected);
            count++;
        }
    }
    test_uint(count, 14);
    for (int i = 0; i < 40; i += 2)
        test_true(ecs_pair_remove(entities[i], pair));
    for (int i = 1; i < 40; i += 2)
        test_int(*(const int *)ecs_pair_get(entities[i], pair), i);
    desc.first.kind = EcsPairEntity;
    it = ecs_pair_query_iter(&desc);
    test_false(ecs_pair_next(&it));
    ecs_fini();
}

void pair_deferred_owning_values_and_inheritance(void) {
    setup();
    ecs_component_t strings =
        ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "PairText",
                                                                    .fields = "{ char *text; }" });
    ecs_pair_id_t text_pair = ecs_pair_init(
        &(ecs_pair_desc_t){ component_element(strings), entity_element(target), strings }
    );
    ecs_entity_t base = ecs_new(), child = ecs_new();
    char buffer[] = "retained";
    char *text = buffer;
    ecs_defer_begin();
    test_true(ecs_pair_set(base, text_pair, &text));
    test_false(ecs_pair_has(base, text_pair));
    buffer[0] = 'X';
    ecs_defer_end();
    test_str(*(char *const *)ecs_pair_get(base, text_pair), "retained");
    ecs_is_a(child, base);
    test_true(ecs_pair_has_owned(child, text_pair));
    test_true(
        *(char *const *)ecs_pair_get(child, text_pair) !=
        *(char *const *)ecs_pair_get(base, text_pair)
    );
    text = "changed";
    test_true(ecs_pair_set(base, text_pair, &text));
    test_str(*(char *const *)ecs_pair_get(child, text_pair), "retained");
    ecs_entity_t queued = ecs_new();
    ecs_defer_begin();
    test_true(ecs_pair_set(queued, text_pair, &text));
    test_true(ecs_pair_remove(queued, text_pair));
    ecs_defer_end();
    test_false(ecs_pair_has_owned(queued, text_pair));
    ecs_entity_t doomed = ecs_new();
    ecs_defer_begin();
    test_true(ecs_pair_set(doomed, text_pair, &text));
    ecs_kill(doomed);
    ecs_defer_end();
    test_false(ecs_is_alive(doomed));
    ecs_fini();
}

static unsigned sets, removals;
static ecs_entity_t expected_entity;
static void observe_pair(ecs_observer_event_t *event) {
    const ecs_pair_event_t *change = event->trigger_data;
    test_uint(event->entity, expected_entity);
    test_uint(event->component, 0);
    test_uint(change->pair, pair);
    test_uint(change->payload, payload);
    if (event->event == EcsOnPairSet) {
        sets++;
        test_int(*(const int *)change->value, sets == 1 ? 7 : 11);
    } else {
        removals++;
        test_int(*(const int *)change->value, 11);
    }
}

void pair_observers_target_deletion_and_world_lifetimes(void) {
    setup();
    sets = removals = 0;
    expected_entity = ecs_new();
    ecs_observer_init(&(ecs_observer_desc_t){ .on = EcsOnPairSet,
                                              .entity = expected_entity,
                                              .callback = observe_pair });
    ecs_observer_init(&(ecs_observer_desc_t){ .on = EcsOnPairRemove,
                                              .entity = expected_entity,
                                              .callback = observe_pair });
    ecs_observer_batch_begin();
    int value = 7;
    test_true(ecs_pair_set(expected_entity, pair, &value));
    value = 11;
    test_true(ecs_pair_set(expected_entity, pair, &value));
    test_uint(sets, 0);
    test_true(ecs_pair_entity_in_use(target));
    test_true(ecs_pair_entity_in_use(expected_entity));
    ecs_kill(target);
    test_false(ecs_pair_has(expected_entity, pair));
    test_false(ecs_pair_entity_in_use(expected_entity));
    ecs_observer_batch_end();
    test_uint(sets, 2);
    test_uint(removals, 1);
    test_true(ecs_pair_info(pair) == NULL);
    ecs_entity_t recycled = ecs_new();
    test_true(recycled != target);
    test_uint(ecs_entity_id(recycled), ecs_entity_id(target));
    test_false(ecs_pair_set(expected_entity, pair, &value));
    const ecs_pair_desc_t next_desc = { component_element(payload),
                                        entity_element(recycled),
                                        payload };
    ecs_pair_id_t next = ecs_pair_init(&next_desc);
    test_true(next != 0 && next != pair);
    pair = next;
    test_true(ecs_pair_set(expected_entity, next, &value));
    test_int(*(const int *)ecs_pair_get(expected_entity, next), 11);
    test_uint(sets, 3);
    test_uint(removals, 1);
    ecs_pair_id_t previous = pair;
    ecs_fini();
    setup();
    test_true(pair != previous);
    test_true(ecs_pair_info(previous) == NULL);
    ecs_fini();
}

void pair_hot_payload_schema(void) {
    setup();
    ecs_entity_t entity = ecs_new();
    int value = 17;
    test_true(ecs_pair_set(entity, pair, &value));
    test_true(ecs_component_dynamic_redefine(payload, "{ char *text; double x; }"));
    test_true(ecs_pair_has_owned(entity, pair));
    struct {
        char *text;
        double x;
    } next = { "new schema", 3.5 };
    test_true(*(char *const *)ecs_pair_get(entity, pair) == NULL);
    test_true(ecs_pair_set(entity, pair, &next));
    test_str(*(char *const *)ecs_pair_get(entity, pair), "new schema");
    test_false(ecs_component_dynamic_redefine(payload, "{ Missing field; }"));
    test_str(*(char *const *)ecs_pair_get(entity, pair), "new schema");
    ecs_fini();
}

void pair_scene_roundtrip(void) {
    setup();
    ecs_entity_t entity = ecs_new_named(0, "Source");
    ecs_pair_id_t reversed = ecs_pair_init(
        &(ecs_pair_desc_t){ entity_element(target), component_element(payload), payload }
    );
    int a = 19, b = 23;
    test_true(ecs_pair_set(entity, pair, &a));
    test_true(ecs_pair_set(entity, reversed, &b));
    void *bytes = NULL;
    size_t size = 0;
    test_true(ecs_save_memory(&bytes, &size));
    test_true(ecs_scene_validate(bytes, size));
    for (size_t truncated = size - 32; truncated < size; truncated++)
        test_false(ecs_scene_validate(bytes, truncated));
    ecs_fini();
    ecs_init();
    payload = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "PairValue",
                                                                          .fields = "{ int x; }" });
    test_true(ecs_load_memory(bytes, size));
    free(bytes);
    target = ecs_lookup("Target");
    entity = ecs_lookup("Source");
    pair = ecs_pair_init(
        &(ecs_pair_desc_t){ component_element(payload), entity_element(target), payload }
    );
    reversed = ecs_pair_init(
        &(ecs_pair_desc_t){ entity_element(target), component_element(payload), payload }
    );
    test_int(*(const int *)ecs_pair_get(entity, pair), 19);
    test_int(*(const int *)ecs_pair_get(entity, reversed), 23);
    ecs_fini();
}

static atomic_bool pair_worker_failed;
static void pair_worker_set(ecs_iter_t *it) {
    for (uint32_t row = 0; row < it->count; row++) {
        int value = (int)ecs_entity_id(it->entities[row]);
        if (!ecs_pair_set(it->entities[row], pair, &value))
            atomic_store(&pair_worker_failed, true);
    }
}

void pair_parallel_deferred_publication(void) {
    ecs_init_w_features(&(ecs_world_feat_desc_t){ .worker_threads = 2 });
    payload = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "PairValue",
                                                                          .fields = "{ int x; }" });
    target = ecs_new();
    pair = ecs_pair_init(
        &(ecs_pair_desc_t){ component_element(payload), entity_element(target), payload }
    );
    ecs_component_t selected = ecs_tag_init("Selected");
    ecs_entity_t entities[65];
    for (unsigned i = 0; i < 65; i++) {
        entities[i] = ecs_new();
        ecs_add_cid(entities[i], selected);
    }
    atomic_store(&pair_worker_failed, false);
    ecs_system_init(&(ecs_system_desc_t){ .phase = EcsOnUpdate,
                                          .parallel = true,
                                          .chunk_size = 8,
                                          .query.components = { { selected, EcsFilter } },
                                          .callback = pair_worker_set });
    ecs_progress();
    const bool worker_failed = atomic_load(&pair_worker_failed);
    test_false(worker_failed);
    for (unsigned i = 0; i < 65; i++) {
        test_true(ecs_pair_has_owned(entities[i], pair));
        test_int(*(const int *)ecs_pair_get(entities[i], pair), ecs_entity_id(entities[i]));
    }
    ecs_fini();
}

void pair_find_is_read_only(void) {
    ecs_init();
    ecs_component_t tag = ecs_tag_init("Payload");
    ecs_entity_t target = ecs_new();
    ecs_pair_desc_t desc = { .first = { tag, EcsPairComponent },
                             .second = { target, EcsPairEntity },
                             .payload = tag };
    test_uint(ecs_pair_find(&desc), 0);
    ecs_pair_id_t id = ecs_pair_init(&desc);
    test_true(id != 0);
    test_uint(ecs_pair_find(&desc), id);
    desc.payload = ecs_id(Abstract);
    test_uint(ecs_pair_find(&desc), id);
    test_uint(ecs_pair_init(&desc), 0);
    test_uint(ecs_pair_find(NULL), 0);
    ecs_kill(target);
    test_uint(ecs_pair_find(&desc), 0);
    ecs_fini();
}
