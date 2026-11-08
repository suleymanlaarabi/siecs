#include "eval.h"

static char *annotation_component(const char *name) {
    static const char prefix[] = "siecs::script::annotation::";
    if (!name || !*name || strlen(name) > SIZE_MAX - sizeof(prefix))
        return NULL;
    size_t length = sizeof(prefix) + strlen(name);
    char *component = malloc(length);
    if (component)
        snprintf(component, length, "%s%s", prefix, name);
    return component;
}

const char *ecs_script_annotation(ecs_entity_t entity, const char *name) {
    if (!ecs_is_alive(entity))
        return NULL;
    char *component = annotation_component(name);
    if (!component)
        return NULL;
    ecs_component_t id = ecs_component_lookup(component);
    free(component);
    if (!id)
        return NULL;
    const ecs_component_info_t *info = ecs_component_info(id);
    const sireflect_fields_t *fields = sireflect_type_fields(info->type);
    if (info->size != sizeof(char *) || fields->field_count != 1 ||
        !sireflect_type_is_cstring(fields->fields[0].type))
        return NULL;
    const char *const *value = ecs_get_cid(entity, id);
    return value ? *value : NULL;
}

int script_annotations_apply(script_eval_t *ctx, ecs_entity_t entity) {
    uint32_t count = 0;
    for (const script_annotation_t *item = ctx->annotations; item; item = item->next)
        count++;
    if (!count)
        return 0;
    const script_annotation_t **items = script_alloc(&ctx->values, count * sizeof(*items));
    uint32_t index = 0;
    for (const script_annotation_t *item = ctx->annotations; item; item = item->next)
        items[index++] = item;
    ctx->annotations = NULL;
    for (uint32_t i = count; i; i--) {
        const script_annotation_t *annotation = items[i - 1];
        char *name = annotation_component(annotation->name);
        if (!name)
            return script_eval_error(ctx, NULL, "cannot allocate annotation name");
        ecs_component_t component =
            script_backend_component_register_dynamic(name, "{ char *value; }");
        free(name);
        if (!component)
            return script_eval_error(
                ctx,
                NULL,
                "cannot register annotation '%s'",
                annotation->name
            );
        const ecs_component_info_t *info = script_backend_component_info(component);
        const sireflect_fields_t *fields = sireflect_type_fields(info->type);
        if (info->size != sizeof(char *) || fields->field_count != 1 ||
            !sireflect_type_is_cstring(fields->fields[0].type))
            return script_eval_error(ctx, NULL, "invalid native annotation layout");
        script_backend_component_set(entity, component, &annotation->value);
    }
    return 0;
}
