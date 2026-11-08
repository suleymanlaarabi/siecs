#include "eval.h"

script_template_t *script_template_lookup(script_eval_t *ctx, const char *name) {
    for (script_template_t *it = ctx->templates; it; it = it->next)
        if (!strcmp(it->node->name, name))
            return it;
    return NULL;
}

/* Template properties use the native dynamic component's reflected layout. */
static bool property_type_supported(sireflect_handle_t type) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info)
        return false;
    if (sireflect_type_is_cstring(type))
        return true;
    if (info->kind == sireflect_kind_array || info->kind == sireflect_kind_vector)
        return property_type_supported(info->element_type);
    if (sireflect_type_is_struct(info)) {
        const sireflect_fields_t *fields = sireflect_type_fields(type);
        for (size_t i = 0; i < fields->field_count; i++)
            if (!property_type_supported(fields->fields[i].type))
                return false;
        return true;
    }
    return sireflect_is_numeric(info->kind) || info->kind == sireflect_kind_bool ||
           info->kind == sireflect_kind_char || info->kind == sireflect_kind_enum;
}

static void
append_property_field(ecs_strbuf_t *description, sireflect_handle_t type, const char *name) {
    sireflect_handle_t element = type;
    while (sireflect_type_info(element)->kind == sireflect_kind_array)
        element = sireflect_type_info(element)->element_type;
    ecs_strbuf_appendstr(description, sireflect_type_name(element));
    ecs_strbuf_appendch(description, ' ');
    ecs_strbuf_appendstr(description, name);
    for (const sireflect_type_info_t *info = sireflect_type_info(type);
         info->kind == sireflect_kind_array;
         info = sireflect_type_info(info->element_type)) {
        char count[32];
        snprintf(count, sizeof count, "[%zu]", info->element_count);
        ecs_strbuf_appendstr(description, count);
    }
    ecs_strbuf_appendstr(description, "; ");
}

static const sireflect_fields_t *template_fields(const script_template_t *definition) {
    const ecs_component_info_t *info = script_backend_component_info(definition->component);
    static const sireflect_fields_t empty;
    return info->type ? sireflect_type_fields(info->type) : &empty;
}

static const sireflect_field_info_t *
find_field(const sireflect_fields_t *fields, const char *name) {
    for (size_t i = 0; i < fields->field_count; i++)
        if (!strcmp(fields->fields[i].name, name))
            return &fields->fields[i];
    return NULL;
}

static const ecs_script_var_node_t *find_property(script_template_t **owner, const char *name) {
    for (; *owner; *owner = (*owner)->base) {
        ecs_script_node_t *const *nodes = (*owner)->node->scope->stmts.data;
        for (uint32_t i = 0; i < (*owner)->node->scope->stmts.size; i++) {
            if (nodes[i]->kind != EcsAstProp)
                continue;
            const ecs_script_var_node_t *property = (const ecs_script_var_node_t *)nodes[i];
            if (!strcmp(property->name, name))
                return property;
        }
    }
    return NULL;
}

int script_template_declare(script_eval_t *ctx, const ecs_script_template_node_t *node) {
    if (!node->name)
        return script_eval_error(ctx, node->node.pos, "anonymous templates not implemented");
    script_template_t *base = node->base ? script_template_lookup(ctx, node->base) : NULL;
    if (node->base && !base)
        return script_eval_error(ctx, node->node.pos, "unknown base template '%s'", node->base);
    uint32_t depth = 0;
    for (script_template_t *ancestor = base; ancestor; ancestor = ancestor->base)
        if (++depth >= 64)
            return script_eval_error(ctx, node->node.pos, "template inheritance depth exceeded");
    const sireflect_fields_t empty = { 0 };
    const sireflect_fields_t *inherited = base ? template_fields(base) : &empty;
    uint32_t prop_count = (uint32_t)inherited->field_count;
    ecs_strbuf_t description = ECS_STRBUF_INIT;
    ecs_strbuf_appendstr(&description, "{ ");
    for (size_t i = 0; i < inherited->field_count; i++)
        append_property_field(&description, inherited->fields[i].type, inherited->fields[i].name);
    ecs_script_node_t **statements = node->scope->stmts.data;
    for (uint32_t i = 0; i < node->scope->stmts.size; i++) {
        if (statements[i]->kind != EcsAstProp)
            continue;
        const ecs_script_var_node_t *prop = (ecs_script_var_node_t *)statements[i];
        const sireflect_field_info_t *overridden = find_field(inherited, prop->name);
        for (uint32_t previous = 0; previous < i; previous++)
            if (statements[previous]->kind == EcsAstProp &&
                !strcmp(((const ecs_script_var_node_t *)statements[previous])->name, prop->name)) {
                free(ecs_strbuf_get(&description));
                return script_eval_error(
                    ctx,
                    prop->node.pos,
                    "duplicate property '%s'",
                    prop->name
                );
            }
        sireflect_handle_t type = prop->type   ? script_resolve_type(ctx, prop->type)
                                  : overridden ? overridden->type
                                               : 0;
        if (!type && !prop->type && prop->expr) {
            script_value_t value;
            if (script_eval_expr(ctx, prop->expr, &value)) {
                free(ecs_strbuf_get(&description));
                return -1;
            }
            type = value.type;
            if (value.collection) {
                sireflect_handle_t element = value.item_type;
                if (!element && value.count)
                    element = script_collection_at(&value, 0).type;
                type = element ? sireflect_vector_type(element) : 0;
            }
        }
        if (type && prop->type_is_vector)
            type = sireflect_vector_type(type);
        if (prop->type_is_template || !property_type_supported(type) ||
            (overridden && overridden->type != type)) {
            free(ecs_strbuf_get(&description));
            return script_eval_error(
                ctx,
                prop->node.pos,
                "unsupported template property type for '%s'",
                prop->name
            );
        }
        if (!overridden) {
            append_property_field(&description, type, prop->name);
            prop_count++;
        }
    }
    ecs_strbuf_appendch(&description, '}');
    char *fields = ecs_strbuf_get(&description);
    ecs_entity_t entity = script_backend_entity_ensure(node->name, ctx->parent);
    if (script_annotations_apply(ctx, entity)) {
        free(fields);
        return -1;
    }
    char *path = script_backend_entity_path(entity);
    const char *name = path && !strncmp(path, "::", 2) ? path + 2 : path;
    ecs_component_t component = !name ? 0
                                : prop_count
                                    ? script_backend_component_register_dynamic(name, fields)
                                    : script_backend_tag_register(name);
    free(path);
    free(fields);
    if (!component)
        return script_eval_error(
            ctx,
            node->node.pos,
            "could not register template '%s'",
            node->name
        );
    script_template_t *template = script_alloc(&ctx->values, sizeof(*template));
    if (!template)
        return script_eval_error(ctx, node->node.pos, "out of memory");
    *template = (script_template_t){ .base = base,
                                     .node = node,
                                     .program = ctx->script,
                                     .component = component,
                                     .functions = ctx->functions,
                                     .imports = ctx->imports,
                                     .next = ctx->templates };
    template->capture_count = ctx->capture_count + ctx->variables.size - ctx->variable_base;
    if (template->capture_count) {
        template->captures =
            script_alloc(&ctx->values, template->capture_count * sizeof(script_variable_t));
        if (!template->captures)
            return script_eval_error(ctx, node->node.pos, "out of memory");
        if (ctx->capture_count)
            memcpy(
                template->captures,
                ctx->captures,
                ctx->capture_count * sizeof(script_variable_t)
            );
        uint32_t locals = ctx->variables.size - ctx->variable_base;
        if (locals)
            memcpy(
                template->captures + ctx->capture_count,
                (script_variable_t *)ctx->variables.data + ctx->variable_base,
                locals * sizeof(script_variable_t)
            );
    }
    ctx->templates = template;
    return 0;
}

static int eval_template_body(
    script_eval_t *ctx,
    script_template_t *definition,
    const sireflect_fields_t *root_fields,
    void *root_value,
    bool properties_bound
) {
    if (definition->base &&
        eval_template_body(ctx, definition->base, root_fields, root_value, false))
        return -1;
    ctx->script = definition->program;
    ctx->captures = definition->captures;
    ctx->capture_count = definition->capture_count;
    ctx->templates = definition;
    ctx->functions = definition->functions;
    ctx->imports = definition->imports;
    const ecs_component_info_t *info = script_backend_component_info(definition->component);
    const sireflect_fields_t *fields = template_fields(definition);
    bool borrowed = fields == root_fields;
    void *value = borrowed ? root_value : script_alloc(&ctx->values, (size_t)info->size);
    if (!borrowed)
        script_backend_value_init(definition->component, value, NULL);
    uint32_t variable_count = ctx->variables.size;
    script_variable_t self = { .name = "this", .value.type = script_backend_type_lookup("entity") };
    self.value.storage.entity = ctx->entity;
    if (!properties_bound)
        sicore_vec_push(&ctx->variables, &self, sizeof self);
    int status = 0;
    for (size_t i = 0; !properties_bound && i < fields->field_count && !status; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        const sireflect_field_info_t *source = find_field(root_fields, field->name);
        script_variable_t variable = { .name = field->name,
                                       .value = { .type = field->type,
                                                  .ptr = (char *)root_value + source->offset } };
        if (!borrowed)
            status = script_value_convert(
                ctx,
                definition->node->node.pos,
                &variable.value,
                field->type,
                (char *)value + field->offset
            );
        sicore_vec_push(&ctx->variables, &variable, sizeof variable);
    }
    if (!status) {
        if (info->size)
            script_backend_component_set(ctx->entity, definition->component, value);
        else
            script_backend_component_add(ctx->entity, definition->component);
        status = script_eval_scope(ctx, definition->node->scope);
    }
    ctx->variables.size = variable_count;
    if (!borrowed)
        script_backend_value_fini(definition->component, value);
    return status;
}

int script_template_eval(
    script_eval_t *ctx,
    script_template_t *template,
    const ecs_expr_node_t *arguments
) {
    if (ctx->template_depth >= 64)
        return script_eval_error(
            ctx,
            arguments ? arguments->pos : ctx->position,
            "template recursion limit exceeded"
        );
    const ecs_component_info_t info = *script_backend_component_info(template->component);
    const sireflect_fields_t empty = { 0 };
    const sireflect_fields_t *fields = info.type ? sireflect_type_fields(info.type) : &empty;
    size_t count = fields->field_count;
    void *buffer = script_alloc(&ctx->values, (size_t)info.size);
    bool *supplied = script_alloc(&ctx->values, count * sizeof(bool));
    if (!buffer || (count && !supplied))
        return script_eval_error(ctx, ctx->position, "out of memory");
    script_backend_value_init(template->component, buffer, NULL);
    int status = 0;
    /* Arguments are evaluated in the caller's lexical frame, before binding props. */
    if (arguments) {
        if (arguments->kind != EcsExprInitializer && arguments->kind != EcsExprEmptyInitializer) {
            status = script_value_write(ctx, arguments, info.type, buffer);
            if (!status)
                memset(supplied, true, count);
        } else {
            const ecs_expr_initializer_t *initializer = (const ecs_expr_initializer_t *)arguments;
            const ecs_expr_initializer_element_t *elements = initializer->elements.data;
            for (uint32_t i = 0; i < initializer->elements.size && !status; i++) {
                size_t field = i;
                if (elements[i].member) {
                    for (field = 0; field < count; field++)
                        if (!strcmp(fields->fields[field].name, elements[i].member))
                            break;
                }
                if (field >= count || supplied[field]) {
                    status = script_eval_error(
                        ctx,
                        elements[i].value->pos,
                        "unknown or duplicate template property '%s'",
                        elements[i].member ? elements[i].member : "<positional>"
                    );
                } else {
                    supplied[field] = true;
                    status = script_value_write(
                        ctx,
                        elements[i].value,
                        fields->fields[field].type,
                        (char *)buffer + fields->fields[field].offset
                    );
                }
            }
        }
    }
    if (status) {
        script_backend_value_fini(template->component, buffer);
        return status;
    }
    uint32_t previous_size = ctx->variables.size, previous_base = ctx->variable_base;
    const script_variable_t *captures = ctx->captures;
    uint32_t capture_count = ctx->capture_count;
    script_template_t *previous_templates = ctx->templates;
    script_function_t *previous_functions = ctx->functions;
    script_import_t *previous_imports = ctx->imports;
    int32_t loop_depth = ctx->loop_depth;
    const ecs_script_t *previous_program = ctx->script;
    ctx->script = template->program;
    ctx->variable_base = previous_size;
    ctx->captures = template->captures;
    ctx->capture_count = template->capture_count;
    ctx->templates = template;
    ctx->loop_depth = 0;
    ctx->template_depth++;
    script_variable_t self = { .name = "this", .value.type = script_backend_type_lookup("entity") };
    self.value.storage.entity = ctx->entity;
    sicore_vec_push(&ctx->variables, &self, sizeof self);
    for (size_t field = 0; field < count && !status; field++) {
        const sireflect_field_info_t *member = &fields->fields[field];
        script_template_t *owner = template;
        const ecs_script_var_node_t *prop = find_property(&owner, member->name);
        assert(prop != NULL);
        ctx->script = owner->program;
        ctx->captures = owner->captures;
        ctx->capture_count = owner->capture_count;
        ctx->templates = owner;
        ctx->functions = owner->functions;
        ctx->imports = owner->imports;
        void *ptr = (char *)buffer + member->offset;
        if (!supplied[field] && prop->expr)
            status = script_value_write(ctx, prop->expr, member->type, ptr);
        script_variable_t variable = { .name = prop->name,
                                       .value = { .type = member->type, .ptr = ptr } };
        sicore_vec_push(&ctx->variables, &variable, sizeof variable);
    }
    if (!status) {
        if (template->base)
            ctx->variables.size = previous_size;
        status = eval_template_body(ctx, template, fields, buffer, !template->base);
    }
    ctx->variables.size = previous_size;
    ctx->variable_base = previous_base;
    ctx->captures = captures;
    ctx->capture_count = capture_count;
    ctx->templates = previous_templates;
    ctx->functions = previous_functions;
    ctx->imports = previous_imports;
    ctx->loop_depth = loop_depth;
    ctx->template_depth--;
    ctx->script = previous_program;
    script_backend_value_fini(template->component, buffer);
    return status;
}
