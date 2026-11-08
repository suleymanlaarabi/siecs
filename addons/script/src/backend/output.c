#include "../../../../src/value.h"
#include "transaction.h"

bool script_output_reads(const script_staged_value_t *output, const script_read_t *read) {
    if (!output->members || !read->type)
        return true;
    const sireflect_fields_t *fields = sireflect_type_fields(output->schema);
    size_t end = read->offset + sireflect_type_size(read->type);
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        if (output->members[i] && read->offset < field->offset + field->size && field->offset < end)
            return true;
    }
    return false;
}

uint8_t *script_output_copy_mask(sireflect_handle_t type, const uint8_t *members) {
    if (!members)
        return NULL;
    size_t count = sireflect_type_fields(type)->field_count;
    uint8_t *copy = malloc(count);
    if (!copy)
        abort();
    memcpy(copy, members, count);
    return copy;
}

bool script_output_overlap(script_key_t component, const uint8_t *left, const uint8_t *right) {
    if (!left || !right)
        return true;
    size_t count = sireflect_type_fields(script_key_info(component)->type)->field_count;
    for (size_t i = 0; i < count; i++)
        if (left[i] && right[i])
            return true;
    return false;
}

void script_output_copy_members(
    script_key_t component,
    void *destination,
    const void *source,
    const uint8_t *members
) {
    if (!members) {
        script_backend_value_copy(component, destination, source);
        return;
    }
    const sireflect_fields_t *fields = sireflect_type_fields(script_key_info(component)->type);
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        if (members[i])
            ecs_reflected_value_copy(
                field->type,
                (char *)destination + field->offset,
                (const char *)source + field->offset,
                false
            );
    }
}

bool script_output_equal(const script_staged_value_t *output, const void *live) {
    const ecs_component_info_t *info = script_key_info(output->component);
    if (output->schema != info->type)
        return false;
    if (!output->members)
        return info->type ? ecs_reflected_value_equal(info->type, output->data, live) : !info->size;
    const sireflect_fields_t *fields = sireflect_type_fields(info->type);
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        if (output->members[i] && !ecs_reflected_value_equal(
                                      field->type,
                                      (const char *)output->data + field->offset,
                                      (const char *)live + field->offset
                                  ))
            return false;
    }
    return true;
}

void script_output_fini(script_staged_value_t *output) {
    free(output->members);
    if (output->data) {
        ecs_value_fini(&output->ops, output->data, 1);
        free(output->data);
    }
}
