#ifndef SIECS_REFLECT_INTERNAL_H
#define SIECS_REFLECT_INTERNAL_H

#include <siecs.h>

typedef enum {
    EcsReflectValueScalar,
    EcsReflectValueStruct,
    EcsReflectValueArray,
    EcsReflectValueString,
    EcsReflectValueEntity,
    EcsReflectValueEnum,
    EcsReflectValuePointer,
} ecs_reflect_value_kind_t;

SIECS_API bool ecs_reflect_type_has_role(sireflect_handle_t type, const char *role);

static inline ecs_reflect_value_kind_t ecs_reflect_value_kind(sireflect_handle_t type) {
    if (ecs_reflect_type_has_role(type, "entity"))
        return EcsReflectValueEntity;
    switch (sireflect_type_category(type)) {
    case sireflect_category_struct:
        return EcsReflectValueStruct;
    case sireflect_category_array:
        return EcsReflectValueArray;
    case sireflect_category_cstring:
        return EcsReflectValueString;
    case sireflect_category_enum:
        return EcsReflectValueEnum;
    case sireflect_category_pointer:
    case sireflect_category_function_pointer:
        return EcsReflectValuePointer;
    default:
        return EcsReflectValueScalar;
    }
}

#endif
