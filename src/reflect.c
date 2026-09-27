#include <siecs/reflect_internal.h>
#include <string.h>

bool ecs_reflect_type_has_role(sireflect_handle_t type, const char *role) {
    const sireflect_meta_t *meta = sireflect_type_meta(type, "siecs.role");
    return meta && meta->kind == SIREFLECT_META_STRING && strcmp(meta->value.string, role) == 0;
}
