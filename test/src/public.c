#include "siecs.h"
#include <siecs_test.h>
#include <stdlib.h>
#include <string.h>

ECS_COMPONENT_DECLARE(PublicPosition, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(PublicPosition);

ECS_COMPONENT_DECLARE(SceneValue, {
    int value;
    ecs_entity_t ref;
});

ECS_COMPONENT_DEFINE(SceneValue);

ECS_RELATION_DECLARE(SceneLink);
ECS_RELATION_DEFINE(SceneLink, { .storage = EcsRelationDense });

void public_metadata_and_entity_introspection(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(PublicPosition);

    const ecs_component_info_t *component = ecs_component_info(ecs_id(PublicPosition));
    test_not_null((void *)component);
    test_str("PublicPosition", component->name);
    test_not_null((void *)component->reflection);
    test_str("PublicPosition", component->reflection->name);
    test_true(ecs_component_count() > ecs_id(PublicPosition));

    const ecs_relation_info_t *is_a = ecs_relation_info(ecs_rid(IsA));
    test_not_null((void *)is_a);
    test_str("IsA", is_a->name);
    test_int(EcsRelationByTarget, is_a->desc.storage);
    test_int(EcsRemoveRelation, is_a->desc.on_delete_target);
    test_true(is_a->desc.acyclic);

    const ecs_relation_info_t *child_of = ecs_relation_info(ecs_rid(ChildOf));
    test_not_null((void *)child_of);
    test_str("ChildOf", child_of->name);
    test_int(EcsRelationByDepth, child_of->desc.storage);
    test_true(child_of->desc.acyclic);
    test_true(ecs_relation_count() > ecs_rid(ChildOf));
    test_null((void *)ecs_relation_info(0));

    ecs_entity_t base = ecs_new();
    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);

    test_uint(entity, ecs_entity_from_index(ecs_entity_id(entity)));
    test_true(ecs_has_relation_id(entity, ecs_rid(IsA)));
    test_uint(base, ecs_target_id(entity, ecs_rid(IsA)));
    test_uint(base, ecs_entity_base(entity));
    test_uint(0, ecs_entity_base(base));

    ecs_unrelate_id(entity, ecs_rid(IsA));
    test_false(ecs_has_relation_id(entity, ecs_rid(IsA)));
    test_uint(0, ecs_entity_base(entity));

    ecs_relate_id(entity, ecs_rid(IsA), base);
    test_uint(base, ecs_entity_base(entity));

    ecs_kill(entity);
    test_uint(0, ecs_entity_from_index(ecs_entity_id(entity)));

    ecs_fini();
}

void public_scene_save_memory_roundtrip(void) {
    void *data = NULL;
    size_t size = 0;

    ecs_init();
    ECS_COMPONENT_REGISTER(PublicPosition);
    ECS_COMPONENT_REGISTER(SceneValue);
    ECS_RELATION_REGISTER(SceneLink);

    ecs_entity_t target = ecs_new();
    ecs_entity_t source = ecs_new();
    ecs_set(source, SceneValue, { .value = 42, .ref = target });
    ecs_relate(source, SceneLink, target);
    test_true(ecs_save_memory(&data, &size));
    test_true(size > 24);
    test_assert(memcmp(data, "SIECSSCN", 8) == 0);
    test_assert(memchr(data, 0, size) != NULL);
    ecs_fini();

    ecs_init();
    ECS_COMPONENT_REGISTER(PublicPosition);
    ECS_COMPONENT_REGISTER(SceneValue);
    ECS_RELATION_REGISTER(SceneLink);
    test_true(ecs_load_memory(data, size));
    ecs_entity_t loaded = 0;
    for (uint32_t index = 1; index < 4; index++) {
        ecs_entity_t candidate = ecs_entity_from_index(index);
        if (candidate && ecs_has(candidate, SceneValue)) {
            loaded = candidate;
            break;
        }
    }
    SceneValue *loaded_value = ecs_try_get(loaded, SceneValue);
    test_not_null(loaded_value);
    test_int(42, loaded_value->value);
    test_true(loaded_value->ref != 0);
    test_true(ecs_target(loaded, SceneLink) == loaded_value->ref);

    ecs_scene_free(data);
    ecs_fini();
}

void public_scene_invalid_load_is_non_destructive(void) {
    void *data = NULL;
    size_t size = 0;

    ecs_init();
    ECS_COMPONENT_REGISTER(PublicPosition);
    ECS_COMPONENT_REGISTER(SceneValue);
    ecs_new();
    test_true(ecs_save_memory(&data, &size));
    uint32_t before = 0;
    memcpy(&before, (const unsigned char *)data + 16, sizeof(before));

    unsigned char *invalid = malloc(size);
    test_not_null(invalid);
    memcpy(invalid, data, size);
    invalid[0] = 'X';
    test_false(ecs_load_memory(invalid, size));

    void *after_data = NULL;
    size_t after_size = 0;
    test_true(ecs_save_memory(&after_data, &after_size));
    uint32_t after = 0;
    memcpy(&after, (const unsigned char *)after_data + 16, sizeof(after));
    test_uint(before, after);

    free(invalid);
    ecs_scene_free(after_data);
    ecs_scene_free(data);
    ecs_fini();
}
