/* Native host contracts used by the Script runtime. */
#include <siecs.h>
#include <siecs_test.h>
#include <stdint.h>

void script_host_single_target_relations_and_dynamic_ids(void) {
    ecs_init();
    const ecs_relation_storage_t storages[] = {
        EcsRelationDense,
        EcsRelationByDepth,
        EcsRelationByTarget,
    };
    const char *names[] = { "HostDense", "HostDepth", "HostTarget" };
    for (unsigned i = 0; i < sizeof storages / sizeof storages[0]; i++) {
        ecs_relation_id_t relation = ecs_relation_init(
            names[i],
            &(ecs_relation_desc_t){
                .storage = storages[i],
                .acyclic = storages[i] == EcsRelationByDepth,
            }
        );
        ecs_entity_t entity = ecs_new();
        ecs_entity_t pizza = ecs_new();
        ecs_entity_t salad = ecs_new();
        ecs_relation_id_t resolved = ecs_relation_lookup(names[i]);
        test_uint(relation, resolved);
        ecs_relate_id(entity, resolved, pizza);
        test_true(ecs_has_relation_to_id(entity, relation, pizza));
        test_false(ecs_has_relation_to_id(entity, relation, salad));
        ecs_relate_id(entity, resolved, salad);
        test_uint(salad, ecs_target_id(entity, relation));
        test_false(ecs_has_relation_to_id(entity, relation, pizza));
        test_true(ecs_has_relation_to_id(entity, relation, salad));
        /* Exclusive relations retain exactly one target. */
        ecs_unrelate_id(entity, relation);
        test_false(ecs_has_relation_id(entity, relation));
    }
    ecs_fini();
}

void script_host_dynamic_struct_is_native_component(void) {
    ecs_init();
    ecs_component_t position = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "ScriptHostPosition",
        .fields = "{ float x; float y; }",
    });
    test_true(position != 0);
    test_uint(position, ecs_component_lookup("ScriptHostPosition"));
    const ecs_component_info_t *info = ecs_component_info(position);
    test_true(info != NULL);
    test_uint(sireflect_type_by_name("ScriptHostPosition"), info->type);
    struct {
        float x;
        float y;
    } value = { 10, 20 };
    test_uint(sizeof value, info->size);
    ecs_entity_t entity = ecs_new();
    ecs_set_cid(entity, position, &value);
    test_true(ecs_has_cid(entity, position));
    const float *data = ecs_get_cid(entity, position);
    test_flt(data[0], 10);
    test_flt(data[1], 20);
    ecs_fini();
}

void script_host_runtime_enum_lookup_and_mask_gap(void) {
    ecs_init();
    sireflect_handle_t type = sireflect_try_register_enum(&(sireflect_enum_desc_t){
        .name = "ScriptHostRuntimeEnum",
        .values = "{ Red = 1, Green = 2, Blue = 4 }",
        .size = sizeof(int32_t),
        .align = _Alignof(int32_t),
    });
    test_true(type != SIREFLECT_INVALID_HANDLE);
    const sireflect_enum_value_t *red = sireflect_enum_value_by_name(type, "Red");
    const sireflect_enum_value_t *blue = sireflect_enum_value_by_value(type, 4);
    test_true(red != NULL);
    test_true(blue != NULL);
    test_int(red->value, 1);
    test_str(blue->name, "Blue");
    test_true(sireflect_enum_value_by_name(type, "Missing") == NULL);
    test_true(sireflect_enum_value_valid(type, 1));
    /* An enum accepts only its declared constants, not arbitrary combinations. */
    test_false(sireflect_enum_value_valid(type, 3));
    ecs_fini();
}
