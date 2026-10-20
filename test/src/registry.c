#include <siecs.h>
#include <siecs_test.h>
#include <stdio.h>
#include <string.h>

ECS_COMPONENT_DECLARE(RegistryValue, { int value; });
ECS_COMPONENT_DEFINE(RegistryValue);

void registry_component_names(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RegistryValue);
    test_int(ecs_id(RegistryValue), ecs_component_lookup("RegistryValue"));
    test_int(ecs_id(Name), ecs_component_lookup("Name"));
    test_int(0, ecs_component_lookup("registryvalue"));
    test_int(0, ecs_component_lookup("MissingRegistryComponent"));
    test_int(0, ecs_component_lookup(NULL));
    test_int(0, ecs_component_lookup(""));
    ecs_component_t dynamic = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "RegistryDynamic", .fields = "{ int value; }",
    });
    test_true(dynamic != 0);
    test_int(dynamic, ecs_component_lookup("RegistryDynamic"));
    test_int(dynamic, ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "RegistryDynamic", .fields = "{ int value; }",
    }));

    /* The existing reflected-type alias behavior remains intact. */
    ecs_component_desc_t alias = ecs_id(RegistryValue_desc);
    alias.name = "RegistryAlias";
    ecs_component_t aliased = ecs_component_init(&alias);
    test_int(aliased, ecs_component_lookup("RegistryAlias"));
    test_int(ecs_id(RegistryValue), ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "RegistryValue", .fields = "{ int value; }",
    }));
    ecs_fini();
}

void registry_relation_names_and_namespaces(void) {
    ecs_init();
    test_int(ecs_rid(ChildOf), ecs_relation_lookup("ChildOf"));
    test_int(ecs_rid(IsA), ecs_relation_lookup("IsA"));
    test_int(0, ecs_relation_lookup(NULL));
    test_int(0, ecs_relation_lookup(""));
    test_int(0, ecs_relation_lookup("childof"));
    test_int(0, ecs_relation_lookup("MissingRegistryRelation"));
    ecs_relation_id_t relation = ecs_relation_init("RegistryShared", &(ecs_relation_desc_t){
        .storage = EcsRelationDense,
    });
    test_true(relation != 0);
    test_int(relation, ecs_relation_lookup("RegistryShared"));
    test_int(0, ecs_component_lookup("RegistryShared"));
    ecs_component_t component = ecs_component({ .name = "RegistryShared" });
    test_int(component, ecs_component_lookup("RegistryShared"));
    test_int(relation, ecs_relation_lookup("RegistryShared"));
    test_int(0, ecs_relation_lookup("Name"));
    ecs_fini();
}

void registry_growth_and_owned_names(void) {
    ecs_init();
    ecs_component_t components[1024];
    ecs_relation_id_t relations[1024];
    char name[64];
    for (unsigned i = 0; i < 1024; i++) {
        snprintf(name, sizeof name, "RegistryGrowComponent%u", i);
        components[i] = ecs_component({ .name = name });
        snprintf(name, sizeof name, "RegistryGrowRelation%u", i);
        relations[i] = ecs_relation_init(name, &(ecs_relation_desc_t){
            .storage = EcsRelationByTarget,
        });
    }
    memset(name, 'x', sizeof name);
    for (unsigned i = 0; i < 1024; i++) {
        snprintf(name, sizeof name, "RegistryGrowComponent%u", i);
        test_int(components[i], ecs_component_lookup(name));
        snprintf(name, sizeof name, "RegistryGrowRelation%u", i);
        test_int(relations[i], ecs_relation_lookup(name));
        test_int(0, ecs_component_lookup(name));
    }
    ecs_fini();
}

void registry_duplicate_names_keep_first(void) {
    ecs_init();
    ecs_component_t first = ecs_component({ .name = "RegistryDuplicate" });
    ecs_component_t second = ecs_component({ .name = "RegistryDuplicate" });
    test_true(first != second);
    test_int(first, ecs_component_lookup("RegistryDuplicate"));
    ecs_relation_desc_t desc = { .storage = EcsRelationDense };
    ecs_relation_id_t a = ecs_relation_init("RegistryDuplicate", &desc);
    ecs_relation_id_t b = ecs_relation_init("RegistryDuplicate", &desc);
    test_true(a != b);
    test_int(a, ecs_relation_lookup("RegistryDuplicate"));
    test_int(first, ecs_component_lookup("RegistryDuplicate"));
    ecs_fini();
}

void registry_world_restart(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RegistryValue);
    ecs_component({ .name = "RegistryPreviousWorld" });
    ecs_relation_init("RegistryPreviousWorld", &(ecs_relation_desc_t){
        .storage = EcsRelationDense,
    });
    ecs_fini();
    ecs_init();
    test_int(0, ecs_component_lookup("RegistryValue"));
    test_int(0, ecs_component_lookup("RegistryPreviousWorld"));
    test_int(0, ecs_relation_lookup("RegistryPreviousWorld"));
    test_int(ecs_id(Name), ecs_component_lookup("Name"));
    test_int(ecs_rid(ChildOf), ecs_relation_lookup("ChildOf"));
    ECS_COMPONENT_REGISTER(RegistryValue);
    test_int(ecs_id(RegistryValue), ecs_component_lookup("RegistryValue"));
    ecs_fini();
}

void registry_type_slots_preserve_pointers(void) {
    char key[64] = "component:RegistryStable";
    uint16_t *stable = ecs_type_slot(key, 37);
    *stable = 91;
    key[0] = 'x'; /* The registry owns the key. */
    for (unsigned i = 0; i < 4096; i++) {
        snprintf(key, sizeof key, "component:RegistrySlot%u", i);
        uint16_t *slot = ecs_type_slot(key, (uint16_t)i);
        test_int(i, *slot);
    }
    test_ptr(stable, ecs_type_slot("component:RegistryStable", 0));
    test_int(91, *stable);
    uint16_t *relation = ecs_type_slot("relation:RegistryStable", 17);
    test_true(stable != relation);
    test_int(17, *relation);
    ecs_init();
    ecs_fini();
    ecs_init();
    test_ptr(stable, ecs_type_slot("component:RegistryStable", 0));
    test_int(91, *stable);
    ecs_fini();
}
