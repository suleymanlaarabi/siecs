#include "value.h"
#include <siecs/reflect_internal.h>

bool ecs_reflect_type_has_role(sireflect_handle_t type, const char *role) {
    const sireflect_meta_t *meta = sireflect_type_meta(type, "siecs.role");
    return meta && meta->kind == SIREFLECT_META_STRING && strcmp(meta->value.string, role) == 0;
}

sireflect_struct_desc_t *ecs_reflection_desc_copy(const sireflect_struct_desc_t *desc) {
    if (!desc)
        return NULL;
    sireflect_struct_desc_t *copy = malloc(sizeof *copy);
    if (!copy)
        abort();
    *copy = *desc;
    copy->name = strdup(desc->name);
    copy->fields = strdup(desc->fields);
    if (!copy->name || !copy->fields)
        abort();
    return copy;
}

void ecs_reflection_desc_free(const sireflect_struct_desc_t *desc) {
    if (!desc)
        return;
    free((char *)desc->name);
    free((char *)desc->fields);
    free((void *)desc);
}
