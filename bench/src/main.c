#include "siecs.h"
#include <bench.h>
#include <stdint.h>

static void register_components(ecs_component_t *cids, uint32_t cid_count) {
    for (uint32_t i = 0; i < cid_count; i++) {
        cids[i] = ecs_component({});
    }
}

typedef struct {
    uint64_t values[4];
} trivial_bench_value_t;

static void register_trivial_data_components(ecs_component_t *cids, uint32_t cid_count) {
    for (uint32_t i = 0; i < cid_count; i++) {
        cids[i] = ecs_component({ .size = sizeof(trivial_bench_value_t) });
    }
}

static ecs_entity_t *make_entities_with_trivial_data(
    const ecs_component_t *cids,
    uint32_t cid_count,
    uint32_t entity_count
) {
    ecs_entity_t *entities = malloc(sizeof(ecs_entity_t) * entity_count);
    trivial_bench_value_t value = { .values = { 1, 2, 3, 4 } };

    for (uint32_t i = 0; i < entity_count; i++) {
        ecs_entity_t entity = ecs_new();
        entities[i] = entity;
        for (uint32_t j = 0; j < cid_count; j++) {
            ecs_set_cid(entity, cids[j], &value);
        }
    }

    return entities;
}

static void make_query_tables(
    ecs_component_t common,
    ecs_component_t rare,
    uint32_t table_count,
    uint32_t rare_table_count
) {
    ecs_component_t *tags = malloc(sizeof(ecs_component_t) * table_count);
    register_components(tags, table_count);

    for (uint32_t i = 0; i < table_count; i++) {
        ecs_entity_t entity = ecs_new();
        ecs_add_cid(entity, common);
        ecs_add_cid(entity, tags[i]);
        if (i < rare_table_count) {
            ecs_add_cid(entity, rare);
        }
    }

    free(tags);
}

static void
make_owned_query_tables(const ecs_component_t *fields, uint32_t field_count, uint32_t table_count) {
    ecs_component_t *tags = malloc(sizeof(ecs_component_t) * table_count);
    register_components(tags, table_count);
    trivial_bench_value_t value = { .values = { 1, 2, 3, 4 } };

    for (uint32_t i = 0; i < table_count; i++) {
        ecs_entity_t entity = ecs_new();
        for (uint32_t field = 0; field < field_count; field++) {
            ecs_set_cid(entity, fields[field], &value);
        }
        ecs_add_cid(entity, tags[i]);
    }

    free(tags);
}

static ecs_query_desc_t
make_query_desc(const ecs_component_t *fields, uint32_t field_count, ecs_term_access_t access) {
    ecs_query_desc_t desc = { 0 };
    for (uint32_t i = 0; i < field_count; i++) {
        desc.terms[i] = (ecs_query_term_t){ .id = fields[i], .access = access };
    }
    return desc;
}

BENCH_SETUP(query_init_rarest_positive, {
    arg(table_count, 4096);
    arg(rare_table_count, 8);
    arg(iter_count, 2000);

    ecs_component_t common = ecs_component({});
    ecs_component_t rare = ecs_component({});
    make_query_tables(common, rare, table_count, rare_table_count);
    ecs_query_desc_t desc = {
        .terms = {
            { .id = common, .access = EcsFilter },
            { .id = rare, .access = EcsFilter },
        },
    };

    BENCH({
        for (uint32_t i = 0; i < iter_count; i++) {
            ecs_query_id_t query = ecs_query_init(&desc);
            ecs_query_fini(query);
        }
    });
});

BENCH_SETUP(query_init_owned_fields, {
    arg(field_count, 8);
    arg(table_count, 256);
    arg(iter_count, 1000);

    ecs_component_t fields[field_count];
    register_trivial_data_components(fields, field_count);
    make_owned_query_tables(fields, field_count, table_count);
    ecs_query_desc_t desc = make_query_desc(fields, field_count, EcsInOut);

    BENCH({
        for (uint32_t i = 0; i < iter_count; i++) {
            ecs_query_id_t query = ecs_query_init(&desc);
            ecs_query_fini(query);
        }
    });
});

BENCH_SETUP(query_iter_owned_fields, {
    arg(field_count, 8);
    arg(table_count, 256);
    arg(iter_count, 10000);

    ecs_component_t fields[field_count];
    register_trivial_data_components(fields, field_count);
    make_owned_query_tables(fields, field_count, table_count);
    ecs_query_desc_t desc = make_query_desc(fields, field_count, EcsInOut);
    ecs_query_id_t query = ecs_query_init(&desc);
    uint64_t checksum = 0;

    BENCH({
        for (uint32_t i = 0; i < iter_count; i++) {
            ecs_iter_t it = ecs_query_iter(query);
            while (ecs_iter_next(&it)) {
                for (uint32_t field = 0; field < field_count; field++) {
                    trivial_bench_value_t *value = ecs_field(&it, field);
                    checksum += value->values[0];
                }
            }
        }
    });

    if (checksum == 0) {
        abort();
    }
    ecs_query_fini(query);
});

BENCH_SETUP(migrate_trivial_columns, {
    arg(cid_count, 8);
    arg(entity_count, 100000);

    ecs_component_t cids[cid_count];
    register_trivial_data_components(cids, cid_count);
    ecs_entity_t *entities = make_entities_with_trivial_data(cids, cid_count, entity_count);
    ecs_component_t tag = ecs_component({});

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_add_cid(entities[i], tag);
        }
    });

    free(entities);
});

BENCH_SETUP(remove_trivial_rows, {
    arg(cid_count, 8);
    arg(entity_count, 100000);

    ecs_component_t cids[cid_count];
    register_trivial_data_components(cids, cid_count);
    ecs_entity_t *entities = make_entities_with_trivial_data(cids, cid_count, entity_count);

    BENCH({
        for (uint32_t i = 0; i < entity_count; i++) {
            ecs_kill(entities[i]);
        }
    });

    free(entities);
});

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

BENCH_SETUP(create_wide_tables_across_index_resize, {
    arg(common_count, 256);
    arg(table_count, 4096);

    ecs_component_t *common = malloc(sizeof(ecs_component_t) * common_count);
    ecs_component_t *unique = malloc(sizeof(ecs_component_t) * table_count);
    ecs_entity_t *entities = malloc(sizeof(ecs_entity_t) * table_count);
    register_components(common, common_count);
    register_components(unique, table_count);

    for (uint32_t i = 0; i < table_count; i++) {
        entities[i] = ecs_new();
        for (uint32_t j = 0; j < common_count; j++) {
            ecs_add_cid(entities[i], common[j]);
        }
    }

    BENCH({
        for (uint32_t i = 0; i < table_count; i++) {
            ecs_add_cid(entities[i], unique[i]);
        }
    });

    free(entities);
    free(unique);
    free(common);
});

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    run_bench(query_init_rarest_positive);
    run_bench(query_init_owned_fields);
    run_bench(query_iter_owned_fields);
    run_bench(migrate_trivial_columns);
    run_bench(remove_trivial_rows);
    run_bench(add_one_component_cold_edge);
    run_bench(add_one_component_hot_edge);
    run_bench(add_many_components_no_required);
    run_bench(add_required_direct_cold_edge);
    run_bench(add_required_direct_hot_edge);
    run_bench(add_required_chain_hot_edge);
    run_bench(add_required_to_existing_component);
    run_bench(add_duplicate_component);
    run_bench(add_many_components_wide_no_required);
    run_bench(create_wide_tables_across_index_resize);

    return 0;
}
