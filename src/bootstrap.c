#include "helper.h"
#include "sicore.h"
#include "siecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if SIECS_HAS_META && !defined(SIREFLECT_H)
#include "sireflect.h"
#endif
#include "storage/table_index.h"
#include "utils.h"
#include "world_internal.h"

ECS_RELATION_DEFINE(
    ChildOf,
    {
        .storage = EcsRelationByDepth,
        .on_delete_target = EcsDeleteSources,
        .acyclic = true,
    }
);
sicore_map_t name_map;

static char *name_copy_string(const char *value) {
    if (!value) {
        return NULL;
    }

    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    ecs_assert_not_null(copy);
    memcpy(copy, value, size);
    return copy;
}

static void name_ctor(void *ptr, uint32_t count) {
    Name *names = ptr;
    for (uint32_t i = 0; i < count; i++) {
        names[i].value = NULL;
    }
}

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
        out[i].value = name_copy_string(in[i].value);
    }
}

static void name_copy(void *dst, const void *src, uint32_t count) {
    Name *out = dst;
    const Name *in = src;
    for (uint32_t i = 0; i < count; i++) {
        if (out[i].value && in[i].value && strcmp(out[i].value, in[i].value) == 0) {
            continue;
        }
        char *copy = name_copy_string(in[i].value);
        free(out[i].value);
        out[i].value = copy;
    }
}

static void name_move_ctor(void *dst, void *src, uint32_t count) {
    Name *out = dst;
    Name *in = src;
    for (uint32_t i = 0; i < count; i++) {
        out[i].value = in[i].value;
        in[i].value = NULL;
    }
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

void name_on_add(ecs_entity_t entity, ecs_component_t component, void *data) {
    Name *name = data;
    if (name->value) {
        sicore_map_set(&name_map, name->value, ecs_first(entity));
    }
}

void name_on_set(
    ecs_entity_t entity,
    ecs_component_t component,
    const void *new_value,
    void *current_value
) {
    Name *name = current_value;
    const Name *new_name = new_value;

    if (name == new_name) {
        if (name->value) {
            sicore_map_set(&name_map, name->value, ecs_first(entity));
        }
        return;
    }

    char *value = name_copy_string(new_name->value);
    if (name->value) {
        sicore_map_unset(&name_map, name->value);
    }
    free(name->value);
    name->value = value;
    if (name->value) {
        sicore_map_set(&name_map, name->value, ecs_first(entity));
    }
}

void name_on_remove(ecs_entity_t entity, ecs_component_t component, void *data) {
    Name *name = data;
    if (name->value) {
        sicore_map_unset(&name_map, name->value);
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
    .on_add = name_on_add,
    .on_remove = name_on_remove,
    .on_set = name_on_set
);

ECS_TAG_DEFINE(Disabled);
ECS_TAG_DEFINE(Abstract);

void ecs_bootstrap() {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create((ecs_type_t){ 0 });
    sicore_vec_push_u64(&ecs_world.entity_index.entities, 0);
    ecs_component({ .name = "Invalid" });

#if SIECS_HAS_META
    // Register the ecs_entity_t struct reflection.
    sireflect_register_struct(
        sijson_default_registry(),
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );
#endif

    ECS_RELATION_REGISTER(ChildOf);
    ECS_COMPONENT_REGISTER(Name);
    sicore_map_init(&name_map);
    ECS_COMPONENT_REGISTER(Disabled);
    ECS_COMPONENT_REGISTER(Abstract);

}
