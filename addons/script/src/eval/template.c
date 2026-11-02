#include "eval.h"

script_template_t *script_template_lookup(script_eval_t *ctx, const char *name) {
    for (script_template_t *it = ctx->templates; it; it = it->next)
        if (!strcmp(it->node->name, name)) return it;
    return NULL;
}

/* Template properties use the native dynamic component's reflected layout.
 * Owning pointer/collection properties require native reflected lifecycle hooks. */
static bool property_type_supported(sireflect_handle_t type) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info) return false;
    if (sireflect_type_is_struct(info)) {
        const sireflect_fields_t *fields = sireflect_type_fields(type);
        for (size_t i = 0; i < fields->field_count; i++)
            if (!property_type_supported(fields->fields[i].type)) return false;
        return true;
    }
    return sireflect_is_numeric(info->kind) || info->kind == sireflect_kind_bool ||
        info->kind == sireflect_kind_char || info->kind == sireflect_kind_enum;
}

int script_template_declare(script_eval_t *ctx, const ecs_script_template_node_t *node) {
    if (!node->name)
        return script_eval_error(ctx, node->node.pos, "anonymous templates not implemented");
    if (node->base || node->parent)
        return script_eval_error(ctx, node->node.pos, "template inheritance and parent constraints not implemented");
    uint32_t prop_count = 0;
    ecs_strbuf_t description = ECS_STRBUF_INIT;
    ecs_strbuf_appendstr(&description, "{ ");
    ecs_script_node_t **statements = node->scope->stmts.data;
    for (uint32_t i = 0; i < node->scope->stmts.size; i++) {
        if (statements[i]->kind != EcsAstProp) continue;
        prop_count++;
        const ecs_script_var_node_t *prop = (ecs_script_var_node_t*)statements[i];
        sireflect_handle_t type = prop->type ? script_backend_type_lookup(prop->type) : 0;
        if (!prop->type && prop->expr) {
            script_value_t value;
            if (script_eval_expr(ctx, prop->expr, &value)) {
                free(ecs_strbuf_get(&description)); return -1;
            }
            type = value.type;
        }
        if (prop->type_is_template || prop->type_is_vector || !property_type_supported(type)) {
            free(ecs_strbuf_get(&description));
            return script_eval_error(ctx, prop->node.pos, "unsupported template property type for '%s'", prop->name);
        }
        ecs_strbuf_appendstr(&description, sireflect_type_name(type));
        ecs_strbuf_appendch(&description, ' ');
        ecs_strbuf_appendstr(&description, prop->name);
        ecs_strbuf_appendstr(&description, "; ");
    }
    ecs_strbuf_appendch(&description, '}');
    char *fields = ecs_strbuf_get(&description);
    ecs_entity_t entity = script_backend_entity_ensure(node->name, ctx->parent);
    char *path = script_backend_entity_path(entity);
    const char *name = path && !strncmp(path, "::", 2) ? path + 2 : path;
    ecs_component_t component = !name ? 0 : prop_count ?
        script_backend_component_register_dynamic(name, fields) : script_backend_tag_register(name);
    free(path);
    free(fields);
    if (!component) return script_eval_error(ctx, node->node.pos, "could not register template '%s'", node->name);
    script_template_t *template = script_alloc(&ctx->values, sizeof(*template));
    if (!template) return script_eval_error(ctx, node->node.pos, "out of memory");
    *template = (script_template_t){.node = node, .component = component, .next = ctx->templates};
    template->capture_count = ctx->capture_count + ctx->variables.size - ctx->variable_base;
    if (template->capture_count) {
        template->captures = script_alloc(&ctx->values, template->capture_count * sizeof(script_variable_t));
        if (!template->captures) return script_eval_error(ctx, node->node.pos, "out of memory");
        if (ctx->capture_count)
            memcpy(template->captures, ctx->captures, ctx->capture_count * sizeof(script_variable_t));
        uint32_t locals = ctx->variables.size - ctx->variable_base;
        if (locals) memcpy(template->captures + ctx->capture_count,
            (script_variable_t*)ctx->variables.data + ctx->variable_base,
            locals * sizeof(script_variable_t));
    }
    ctx->templates = template;
    return 0;
}

int script_template_eval(script_eval_t *ctx, script_template_t *template,
    const ecs_expr_node_t *arguments) {
    if (ctx->template_depth >= 64)
        return script_eval_error(ctx, arguments ? arguments->pos : ctx->position, "template recursion limit exceeded");
    const ecs_component_info_t *info = script_backend_component_info(template->component);
    const sireflect_fields_t empty = {0};
    const sireflect_fields_t *fields = info->type ? sireflect_type_fields(info->type) : &empty;
    size_t count = fields->field_count;
    void *buffer = script_alloc(&ctx->values, (size_t)info->size);
    bool *supplied = script_alloc(&ctx->values, count * sizeof(bool));
    if (!buffer || (count && !supplied)) return script_eval_error(ctx, ctx->position, "out of memory");
    script_backend_value_init(template->component, buffer, NULL);
    int status = 0;
    /* Arguments are evaluated in the caller's lexical frame, before binding props. */
    if (arguments) {
        if (arguments->kind != EcsExprInitializer && arguments->kind != EcsExprEmptyInitializer) {
            status = script_value_write(ctx, arguments, info->type, buffer);
            if (!status) memset(supplied, true, count);
        } else {
            const ecs_expr_initializer_t *initializer = (const ecs_expr_initializer_t*)arguments;
            const ecs_expr_initializer_element_t *elements = initializer->elements.data;
            for (uint32_t i = 0; i < initializer->elements.size && !status; i++) {
                size_t field = i;
                if (elements[i].member) {
                    for (field = 0; field < count; field++)
                        if (!strcmp(fields->fields[field].name, elements[i].member)) break;
                }
                if (field >= count || supplied[field]) {
                    status = script_eval_error(ctx, elements[i].value->pos,
                        "unknown or duplicate template property '%s'", elements[i].member ? elements[i].member : "<positional>");
                } else {
                    supplied[field] = true;
                    status = script_value_write(ctx, elements[i].value, fields->fields[field].type,
                        (char*)buffer + fields->fields[field].offset);
                }
            }
        }
    }
    if (status) { script_backend_value_fini(template->component, buffer); return status; }
    uint32_t previous_size = ctx->variables.size, previous_base = ctx->variable_base;
    const script_variable_t *captures = ctx->captures;
    uint32_t capture_count = ctx->capture_count;
    script_template_t *previous_templates = ctx->templates;
    int32_t loop_depth = ctx->loop_depth;
    ctx->variable_base = previous_size;
    ctx->captures = template->captures;
    ctx->capture_count = template->capture_count;
    ctx->templates = template;
    ctx->loop_depth = 0;
    ctx->template_depth++;
    script_variable_t self = {.name = "this", .value.type = script_backend_type_lookup("entity")};
    self.value.storage.entity = ctx->entity;
    sicore_vec_push(&ctx->variables, &self, sizeof self);
    ecs_script_node_t **statements = template->node->scope->stmts.data;
    size_t field = 0;
    for (uint32_t i = 0; i < template->node->scope->stmts.size && !status; i++) {
        if (statements[i]->kind != EcsAstProp) continue;
        const ecs_script_var_node_t *prop = (ecs_script_var_node_t*)statements[i];
        const sireflect_field_info_t *member = &fields->fields[field];
        void *ptr = (char*)buffer + member->offset;
        if (!supplied[field] && prop->expr)
            status = script_value_write(ctx, prop->expr, member->type, ptr);
        script_variable_t variable = {.name = prop->name, .value = {.type = member->type, .ptr = ptr}};
        sicore_vec_push(&ctx->variables, &variable, sizeof variable);
        field++;
    }
    if (!status) {
        if (info->size) script_backend_component_set(ctx->entity, template->component, buffer);
        else script_backend_component_add(ctx->entity, template->component);
        status = script_eval_scope(ctx, template->node->scope);
    }
    ctx->variables.size = previous_size;
    ctx->variable_base = previous_base;
    ctx->captures = captures;
    ctx->capture_count = capture_count;
    ctx->templates = previous_templates;
    ctx->loop_depth = loop_depth;
    ctx->template_depth--;
    script_backend_value_fini(template->component, buffer);
    return status;
}
