#include <siecs_test.h>

void source_query_c_target_filter_and_shared_write(void) {
    ecs_init();
    ecs_component_t state = ecs_component({ .size = sizeof(int) });
    ecs_component_t value = ecs_component({ .size = sizeof(int) });
    ecs_component_t active = ecs_component({ 0 });
    ecs_relation_id_t owner = 0;
    ecs_relation_desc_t relation = { .storage = EcsRelationByTarget,
                                     .on_delete_target = EcsRemoveRelation };
    ecs_relation_register(&owner, "SourceOwner", &relation);
    ecs_entity_t root = ecs_new(), child = ecs_new(), global = ecs_new();
    int data = 12;
    ecs_set_cid(root, state, &data);
    ecs_add_cid(root, active);
    ecs_relate_id(child, owner, root);
    ecs_set_cid(child, value, &data);
    ecs_set_cid(global, value, &data);
    ecs_query_id_t query = ecs_query(
        { .components = { { value, EcsInOut },
                          { state, ECS_QUERY_UP_ACCESS(EcsInOutTarget, owner) },
                          { active, ECS_QUERY_UP_ACCESS(EcsFilterTarget, owner) } } }
    );
    ecs_iter_t it = ecs_query_iter_target(query, owner, root);
    test_true(ecs_iter_next(&it));
    test_uint(1, it.count);
    test_uint(child, it.entities[0]);
    test_true(ecs_field_is_shared(&it, 1));
    *(int *)ecs_field(&it, 1) = 24;
    test_false(ecs_iter_next(&it));
    test_int(24, *(int *)ecs_get_cid(root, state));
    ecs_remove_cid(root, active);
    test_uint(0, ecs_query_count(query));
    ecs_query_fini(query);
    ecs_fini();
}

void source_query_c_ancestor_filter_without_fields(void) {
    ecs_init();
    ecs_entity_t root = ecs_new(), child = ecs_new(), grandchild = ecs_new(), outside = ecs_new();
    ecs_relate_id(child, ecs_rid(ChildOf), root);
    ecs_relate_id(grandchild, ecs_rid(ChildOf), child);
    ecs_add(root, SimulationPaused);
    ecs_query_id_t query = ecs_query(
        { .components = {
              { ecs_id(SimulationPaused), ECS_QUERY_UP_ACCESS(EcsFilterUp, ecs_rid(ChildOf)) } } }
    );
    test_uint(3, ecs_query_count(query));
    ecs_remove(root, SimulationPaused);
    test_uint(0, ecs_query_count(query));
    ecs_add(outside, SimulationPaused);
    test_uint(1, ecs_query_count(query));
    ecs_query_fini(query);
    ecs_fini();
}
