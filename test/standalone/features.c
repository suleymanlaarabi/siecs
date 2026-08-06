#include "siecs.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef EXPECT_META
#error "EXPECT_META must be defined"
#endif
_Static_assert(SIECS_HAS_META == EXPECT_META);

ECS_COMPONENT(FeaturePosition, { int value; });
ECS_RESOURCE(FeatureTime, { int value; });
ECS_MODULE_DECLARE(feature_module, { int value; });
ECS_MODULE_DEFINE(feature_module);

static void feature_system(ecs_iter_t *it) { (void)it; }

void feature_module_import(const feature_module_props_t *props) { (void)props; }

int main(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(FeaturePosition);
    ECS_RESOURCE_REGISTER(FeatureTime);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, FeaturePosition, { 42 });
    assert(ecs_get(entity, FeaturePosition)->value == 42);

    ecs_set_resource(FeatureTime, { 7 });
    assert(ecs_get_resource(FeatureTime)->value == 7);

    ecs_system_id_t system = ecs_system(
        {
            .name = "FeatureSystem",
            .callback = feature_system,
            .phase = EcsOnUpdate,
        }
    );
    ecs_module_id_t module = ECS_MODULE_IMPORT(feature_module, { 1 });

#if SIECS_HAS_META
    assert(ecs_id(FeaturePosition_desc).struct_desc != NULL);
#endif

    ecs_fini();
    return 0;
}
