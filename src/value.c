#include "value.h"
#include "sireflect.h"
#include "utils.h"

static bool reflected_owns_strings(sireflect_handle_t type) {
    if (sireflect_type_is_cstring(type))
        return true;
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info->kind == sireflect_kind_array)
        return reflected_owns_strings(info->element_type);
    if (sireflect_type_is_struct(info)) {
        for (size_t i = 0; i < info->fields.field_count; i++)
            if (reflected_owns_strings(info->fields.fields[i].type))
                return true;
    }
    return false;
}

ecs_type_ops_t ecs_reflected_ops(sireflect_handle_t type) {
    return (ecs_type_ops_t){ .reflected_type = type && reflected_owns_strings(type) ? type : 0 };
}

void ecs_reflected_value_fini(sireflect_handle_t type, void *value) {
    if (sireflect_type_is_cstring(type)) {
        char **text = value;
        free(*text);
        *text = NULL;
        return;
    }
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info->kind == sireflect_kind_array) {
        size_t stride = sireflect_type_size(info->element_type);
        for (size_t i = 0; i < info->element_count; i++)
            ecs_reflected_value_fini(info->element_type, (char *)value + i * stride);
    } else if (sireflect_type_is_struct(info)) {
        for (size_t i = 0; i < info->fields.field_count; i++) {
            const sireflect_field_info_t *field = &info->fields.fields[i];
            ecs_reflected_value_fini(field->type, (char *)value + field->offset);
        }
    }
}

void ecs_reflected_value_copy(
    sireflect_handle_t type,
    void *destination,
    const void *source,
    bool construct
) {
    if (destination == source)
        return;
    if (sireflect_type_is_cstring(type)) {
        const char *text = *(const char *const *)source;
        char *copy = NULL;
        if (text) {
            size_t length = strlen(text) + 1;
            copy = malloc(length);
            ecs_assert(copy != NULL, "out of memory copying reflected string");
            memcpy(copy, text, length);
        }
        if (!construct)
            free(*(char **)destination);
        *(char **)destination = copy;
        return;
    }
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info->kind == sireflect_kind_array) {
        size_t stride = sireflect_type_size(info->element_type);
        for (size_t i = 0; i < info->element_count; i++)
            ecs_reflected_value_copy(
                info->element_type,
                (char *)destination + i * stride,
                (const char *)source + i * stride,
                construct
            );
    } else if (sireflect_type_is_struct(info)) {
        if (construct)
            memset(destination, 0, info->size);
        for (size_t i = 0; i < info->fields.field_count; i++) {
            const sireflect_field_info_t *field = &info->fields.fields[i];
            ecs_reflected_value_copy(
                field->type,
                (char *)destination + field->offset,
                (const char *)source + field->offset,
                construct
            );
        }
    } else
        memcpy(destination, source, info->size);
}

bool ecs_reflected_value_equal(sireflect_handle_t type, const void *left, const void *right) {
    if (!left || !right)
        return left == right;
    if (sireflect_type_is_cstring(type)) {
        const char *a = *(const char *const *)left, *b = *(const char *const *)right;
        return a && b ? !strcmp(a, b) : a == b;
    }
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info->kind == sireflect_kind_array) {
        size_t stride = sireflect_type_size(info->element_type);
        for (size_t i = 0; i < info->element_count; i++)
            if (!ecs_reflected_value_equal(
                    info->element_type,
                    (const char *)left + i * stride,
                    (const char *)right + i * stride
                ))
                return false;
        return true;
    }
    if (sireflect_type_is_struct(info)) {
        for (size_t i = 0; i < info->fields.field_count; i++) {
            const sireflect_field_info_t *field = &info->fields.fields[i];
            if (!ecs_reflected_value_equal(
                    field->type,
                    (const char *)left + field->offset,
                    (const char *)right + field->offset
                ))
                return false;
        }
        return true;
    }
    return !memcmp(left, right, info->size);
}
