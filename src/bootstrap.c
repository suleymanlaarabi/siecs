#include "helper.h"
#include "sicore.h"
#include "siecs.h"
#include "sireflect.h"
#include "storage/table_index.h"
#include "utils.h"
#include "world_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ECS_RELATION_DEFINE(
    ChildOf,
    {
        .storage = EcsRelationByDepth,
        .on_delete_target = EcsDeleteSources,
        .acyclic = true,
    }
);
ECS_RELATION_DEFINE(
    IsA,
    {
        .storage = EcsRelationByTarget,
        .on_delete_target = EcsRemoveRelation,
        .acyclic = true,
    }
);

static void name_ctor(void *ptr, uint32_t count) { memset(ptr, 0, (size_t)count * sizeof(Name)); }

static void name_dtor(void *ptr, uint32_t count) {
    Name *names = ptr;
    for (uint32_t i = 0; i < count; i++) {
        free(names[i].value);
        names[i].value = NULL;
    }
}

static void name_copy_ctor(void *dst, const void *src, uint32_t count) {
    Name *out = dst;
    const Name *in = src;
    for (uint32_t i = 0; i < count; i++) {
        out[i].value = in[i].value ? strdup(in[i].value) : NULL;
    }
}

static void name_copy(void *dst, const void *src, uint32_t count) {
    Name *out = dst;
    const Name *in = src;
    for (uint32_t i = 0; i < count; i++) {
        if (out[i].value && in[i].value && strcmp(out[i].value, in[i].value) == 0) {
            continue;
        }
        char *copy = in[i].value ? strdup(in[i].value) : NULL;
        free(out[i].value);
        out[i].value = copy;
    }
}

static void name_move_ctor(void *dst, void *src, uint32_t count) {
    memcpy(dst, src, (size_t)count * sizeof(Name));
    memset(src, 0, (size_t)count * sizeof(Name));
}

static void name_move(void *dst, void *src, uint32_t count) {
    Name *out = dst;
    Name *in = src;
    for (uint32_t i = 0; i < count; i++) {
        if (out == in) {
            continue;
        }
        if (out[i].value && in[i].value && strcmp(out[i].value, in[i].value) == 0) {
            free(in[i].value);
            in[i].value = NULL;
            continue;
        }
        free(out[i].value);
        out[i].value = in[i].value;
        in[i].value = NULL;
    }
}

ECS_COMPONENT_DEFINE(
    Name,
    .ops = {
        .ctor = name_ctor,
        .dtor = name_dtor,
        .copy_ctor = name_copy_ctor,
        .copy = name_copy,
        .move_ctor = name_move_ctor,
        .move = name_move,
    },
    ECS_SET_ONLY
    .on_remove = ecs_name_on_remove
);

ECS_RESOURCE_DEFINE(DeltaTime);

ECS_TAG_DEFINE(Disabled);
ECS_TAG_DEFINE(Abstract);

void ecs_bootstrap() {
    ecs_names_init();
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create((ecs_type_t){ 0 });
    sicore_vec_push_u64(&entity_index.entities, 0);
    ecs_component({ .name = "Invalid" });

    // Register the ecs_entity_t struct reflection.
    sireflect_handle_t entity_type = sireflect_register_struct(&(sireflect_struct_desc_t){
        .name = "ecs_entity_t",
        .fields = "{ uint32_t id; uint32_t generation; }",
        .size = sizeof(ecs_entity_t),
        .align = _Alignof(ecs_entity_t),
    });
    sireflect_type_set_meta(entity_type, &(sireflect_meta_t){
        .key = "siecs.role", .kind = SIREFLECT_META_STRING, .value.string = "entity",
    });

    ecs_relation_register_virtual(&ecs_rid(IsA), "IsA", &ecs_rid(IsA_desc), &ecs_relation_ops_isa);
    ECS_RELATION_REGISTER(ChildOf);
    ECS_COMPONENT_REGISTER(Name);
    ECS_RESOURCE_REGISTER(DeltaTime);
    ecs_set_resource(DeltaTime, { .value = 0.0f });
    ECS_COMPONENT_REGISTER(Disabled);
    ECS_COMPONENT_REGISTER(Abstract);
}
