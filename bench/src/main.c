#include "siecs.h"
#include <bench.h>
#include <stdint.h>

static void register_components(ecs_component_t *cids, uint32_t cid_count) {
    for (uint32_t i = 0; i < cid_count; i++) {
        cids[i] = ecs_component({});
    }
}

BENCH_SETUP(add_one_component_cold_edge, {
    arg(entity_count, 100000);

    ecs_component_t cid = ecs_component({});

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_add_cid(ecs_new(), cid);
        }
    });
});

BENCH_SETUP(add_one_component_hot_edge, {
    arg(entity_count, 100000);

    ecs_component_t cid = ecs_component({});
    ecs_add_cid(ecs_new(), cid);

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_add_cid(ecs_new(), cid);
        }
    });
});

BENCH_SETUP(add_many_components_no_required, {
    arg(cid_count, 64);
    arg(entity_count, 10000);

    ecs_component_t *cids = malloc(sizeof(ecs_component_t) * cid_count);
    register_components(cids, cid_count);

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_entity_t entity = ecs_new();
            for (uint32_t j = 0; j < cid_count; j++) {
                ecs_add_cid(entity, cids[j]);
            }
        }
    });

    free(cids);
});

BENCH_SETUP(add_required_direct_cold_edge, {
    arg(entity_count, 100000);

    ecs_component_t component = ecs_component({});
    ecs_component_t required = ecs_component({});
    ecs_with(component, required);

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_add_cid(ecs_new(), component);
        }
    });
});

BENCH_SETUP(add_required_direct_hot_edge, {
    arg(entity_count, 100000);

    ecs_component_t component = ecs_component({});
    ecs_component_t required = ecs_component({});
    ecs_with(component, required);
    ecs_add_cid(ecs_new(), component);

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_add_cid(ecs_new(), component);
        }
    });
});

BENCH_SETUP(add_required_chain_hot_edge, {
    arg(entity_count, 100000);

    ecs_component_t root = ecs_component({});
    ecs_component_t mid = ecs_component({});
    ecs_component_t leaf = ecs_component({});
    ecs_with(root, mid);
    ecs_with(mid, leaf);
    ecs_add_cid(ecs_new(), root);

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_add_cid(ecs_new(), root);
        }
    });
});

BENCH_SETUP(add_required_to_existing_component, {
    arg(entity_count, 100000);

    ecs_component_t component = ecs_component({});
    ecs_component_t required = ecs_component({});
    ecs_with(component, required);

    ecs_entity_t warmup = ecs_new();
    ecs_add_cid(warmup, required);
    ecs_add_cid(warmup, component);

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_entity_t entity = ecs_new();
            ecs_add_cid(entity, required);
            ecs_add_cid(entity, component);
        }
    });
});

BENCH_SETUP(add_duplicate_component, {
    arg(entity_count, 100000);

    ecs_component_t cid = ecs_component({});
    ecs_entity_t *entities = malloc(sizeof(ecs_entity_t) * entity_count);
    for (uint32_t i = 0; i < entity_count; i++) {
        entities[i] = ecs_new();
        ecs_add_cid(entities[i], cid);
    }

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_add_cid(entities[i], cid);
        }
    });

    free(entities);
});

BENCH_SETUP(add_many_components_wide_no_required, {
    arg(cid_count, 800);
    arg(entity_count, 1000);
    arg(iter_count, 20);

    ecs_component_t *cids = malloc(sizeof(ecs_component_t) * cid_count);

    BENCH({
        for (uint32_t i = 0; i < iter_count; i++) {
            register_components(cids, cid_count);
            for (uint32_t j = 0; j < entity_count; j++) {
                ecs_entity_t entity = ecs_new();
                for (uint32_t k = 0; k < cid_count; k++) {
                    ecs_add_cid(entity, cids[k]);
                }
            }
        }
    });

    free(cids);
});

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    run_bench(add_one_component_cold_edge);
    run_bench(add_one_component_hot_edge);
    run_bench(add_many_components_no_required);
    run_bench(add_required_direct_cold_edge);
    run_bench(add_required_direct_hot_edge);
    run_bench(add_required_chain_hot_edge);
    run_bench(add_required_to_existing_component);
    run_bench(add_duplicate_component);
    run_bench(add_many_components_wide_no_required);

    return 0;
}
