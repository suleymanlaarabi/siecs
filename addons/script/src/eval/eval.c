#include "eval.h"

static int eval_id_name(script_eval_t *ctx, const ecs_expr_node_t *expr,
    const char **name) {
    if (!expr) return 0;
    script_value_t value;
    if (script_eval_expr(ctx, expr, &value)) return -1;
    if (!sireflect_type_is_cstring(value.type))
        return script_eval_error(ctx, expr->pos, "expected string identifier");
    *name = *(const char *const*)script_value_data(&value);
    return 0;
}

static int eval_id(script_eval_t *ctx, const ecs_script_id_t *id,
    const ecs_expr_node_t *value, bool partial) {
    if (!ctx->entity) return script_eval_error(ctx, NULL, "component outside entity scope");
    if (id->flag || id->index_expr)
        return script_eval_error(ctx, NULL, "flagged or indexed component id not implemented");
    const char *first = id->first, *second = id->second;
    ecs_entity_t target = 0;
    if (second && second[0] == '$') {
        const script_variable_t *variable = script_find_variable(ctx, second + 1);
        if (variable && variable->value.type == script_backend_type_lookup("entity"))
            memcpy(&target, script_value_data(&variable->value), sizeof target);
    }
    if (eval_id_name(ctx, id->first_expr, &first) ||
        (!target && eval_id_name(ctx, id->second_expr, &second))) return -1;
    script_template_t *template = script_template_lookup(ctx, first);
    if (template && !second) return script_template_eval(ctx, template, value);
    script_symbol_t symbol = script_backend_symbol_lookup(first, ctx->parent);
    if (second) {
        if (value) return script_eval_error(ctx, value->pos, "typed pair payload requires native core support");
        if (symbol.kind != ScriptSymbolRelation)
            return script_eval_error(ctx, NULL, "unresolved relation '%s'", first);
        if (!target) target = script_backend_entity_lookup(second, ctx->parent);
        if (!target) return script_eval_error(ctx, NULL, "unresolved target '%s'", second);
        if (!strcmp(first, "IsA")) { script_backend_isa_add(ctx->entity, target); return 0; }
        if (script_backend_relation_add(ctx->entity, symbol.id.relation, target))
            return script_eval_error(ctx, NULL, "multiple relation targets require native typed pair support");
        return 0;
    }
    if (symbol.kind == ScriptSymbolNone && ctx->skip_unknown) {
        script_backend_entity_ensure(first, ctx->parent);
        symbol.kind = ScriptSymbolEntity;
    }
    if (symbol.kind == ScriptSymbolEntity) {
        /* An entity used as a tag maps to a real native zero-size component. */
        ecs_entity_t entity = script_backend_entity_lookup(first, ctx->parent);
        char *path = script_backend_entity_path(entity);
        if (!path) return script_eval_error(ctx, NULL, "could not resolve tag path");
        const char *name = path;
        if (!strncmp(name, "::", 2)) name += 2;
        ecs_component_t component = script_backend_component_lookup(name);
        if (!component) component = script_backend_tag_register(name);
        free(path);
        symbol = (script_symbol_t){ScriptSymbolComponent, {.component = component}};
    }
    if (symbol.kind != ScriptSymbolComponent)
        return script_eval_error(ctx, NULL, "unresolved component '%s'", first);
    ecs_component_t component = symbol.id.component;
    if (!value) {
        script_backend_component_add(ctx->entity, component);
        return 0;
    }
    const ecs_component_info_t *info = script_backend_component_info(component);
    if (!info->type || !info->size) {
        if (ctx->skip_unknown) { script_backend_component_add(ctx->entity, component); return 0; }
        return script_eval_error(ctx, value->pos, "component '%s' has no reflected value", info->name);
    }
    /* Native reflected component metadata owns the layout; no JSON conversion. */
    sireflect_handle_t type = info->type;
    size_t size = (size_t)info->size;
    void *buffer = script_alloc(&ctx->values, size);
    if (!buffer) return script_eval_error(ctx, value->pos, "out of memory");
    const void *old = partial ? script_backend_component_get(ctx->entity, component) : NULL;
    script_backend_value_init(component, buffer, old);
    int result;
    if (value->kind == EcsExprInitializer || value->kind == EcsExprEmptyInitializer) {
        result = script_value_write(ctx, value, type, buffer);
    } else {
        script_value_t source;
        result = script_eval_expr(ctx, value, &source);
        if (!result) {
            if (source.type == type) script_backend_value_copy(component, buffer, script_value_data(&source));
            else result = script_value_convert(ctx, value->pos, &source, type, buffer);
        }
    }
    if (!result) script_backend_component_set(ctx->entity, component, buffer);
    script_backend_value_fini(component, buffer);
    return result;
}

static int eval_struct(script_eval_t *ctx, ecs_script_entity_t *entity) {
    if (!entity->name || !entity->kind_w_expr || entity->scope->stmts.size != 1)
        return script_eval_error(ctx, entity->node.pos, "struct body syntax not implemented");
    ecs_script_node_t *node = ecs_vec_first_t(&entity->scope->stmts, ecs_script_node_t*)[0];
    if (node->kind != EcsAstComponent)
        return script_eval_error(ctx, entity->node.pos, "expected struct members");
    ecs_expr_node_t *expression = ((ecs_script_component_t*)node)->expr;
    if (expression->kind != EcsExprInitializer)
        return script_eval_error(ctx, expression->pos, "expected struct member list");
    ecs_expr_initializer_t *initializer = (ecs_expr_initializer_t*)expression;
    ecs_expr_initializer_element_t *members = initializer->elements.data;
    ecs_strbuf_t fields = ECS_STRBUF_INIT;
    ecs_strbuf_appendstr(&fields, "{ ");
    for (uint32_t i = 0; i < initializer->elements.size; i++) {
        if (!members[i].member || members[i].value->kind != EcsExprIdentifier) {
            free(ecs_strbuf_get(&fields));
            return script_eval_error(ctx, expression->pos, "expected member: type");
        }
        const char *name = ((ecs_expr_identifier_t*)members[i].value)->value;
        sireflect_handle_t type = script_backend_type_lookup(name);
        if (!type) {
            free(ecs_strbuf_get(&fields));
            return script_eval_error(ctx, members[i].value->pos, "unknown member type '%s'", name);
        }
        ecs_strbuf_appendstr(&fields, sireflect_type_name(type));
        ecs_strbuf_appendch(&fields, ' ');
        ecs_strbuf_appendstr(&fields, members[i].member);
        ecs_strbuf_appendstr(&fields, "; ");
    }
    ecs_strbuf_appendch(&fields, '}');
    char *description = ecs_strbuf_get(&fields);
    ecs_entity_t metadata = script_backend_entity_ensure(entity->name, ctx->parent);
    char *path = script_backend_entity_path(metadata);
    const char *name = path;
    if (name && !strncmp(name, "::", 2)) name += 2;
    ecs_component_t component = name ? script_backend_component_register_dynamic(name, description) : 0;
    free(description);
    free(path);
    if (!component) return script_eval_error(ctx, entity->node.pos, "could not register struct '%s'", entity->name);
    for (uint32_t i = 0; i < initializer->elements.size; i++) {
        script_backend_entity_ensure(members[i].member, metadata);
    }
    return 0;
}

static int eval_entity(script_eval_t *ctx, ecs_script_entity_t *entity) {
    if (entity->kind && !strcmp(entity->kind, "struct")) return eval_struct(ctx, entity);
    const char *name = entity->name;
    if (entity->name_expr) {
        script_value_t value;
        if (script_eval_expr(ctx, entity->name_expr, &value)) return -1;
        if (!sireflect_type_is_cstring(value.type))
            return script_eval_error(ctx, entity->node.pos, "expected string entity name");
        name = value.storage.string;
    }
    ecs_entity_t output = script_backend_entity_ensure(name, ctx->parent);
    if (!output) return script_eval_error(ctx, entity->node.pos, "invalid entity name");
    bool prefab = entity->kind && !strcmp(entity->kind, "prefab");
    if (entity->kind && !entity->kind_w_expr) {
        if (!prefab) {
            ecs_script_id_t id = { .first = entity->kind };
            ecs_entity_t previous = ctx->entity;
            ctx->entity = output;
            int result = eval_id(ctx, &id, NULL, false);
            ctx->entity = previous;
            if (result) return result;
        }
    }
    ecs_entity_t previous_entity = ctx->entity, previous_parent = ctx->parent;
    ctx->entity = ctx->parent = output;
    int result = script_eval_scope(ctx, entity->scope);
    /* Finish authoring the hierarchy before native abstraction locks additions. */
    if (!result && prefab) script_backend_prefab(output);
    ctx->entity = previous_entity;
    ctx->parent = previous_parent;
    return result;
}

static int eval_var(script_eval_t *ctx, ecs_script_var_node_t *node) {
    script_variable_t variable = {.name = node->name};
    if (node->type) {
        variable.value.type = script_backend_type_lookup(node->type);
        if (!variable.value.type || sireflect_type_size(variable.value.type) > sizeof variable.value.storage)
            return script_eval_error(ctx, node->node.pos, "variable type not implemented");
        if (script_value_write(ctx, node->expr, variable.value.type, &variable.value.storage)) return -1;
    } else if (script_eval_expr(ctx, node->expr, &variable.value)) return -1;
    sicore_vec_push(&ctx->variables, &variable, sizeof variable);
    return 0;
}

static int eval_for(script_eval_t *ctx, ecs_script_for_t *node) {
    if (!node->from || !node->to || node->loop_var_count != 1)
        return script_eval_error(ctx, node->node.pos, "collection or multiple-variable loops not implemented");
    int32_t from = 0, to = 0;
    sireflect_handle_t type = script_backend_type_lookup("int32_t");
    if (script_value_write(ctx, node->from, type, &from) || script_value_write(ctx, node->to, type, &to)) return -1;
    uint32_t count = ctx->variables.size;
    script_variable_t variable = {.name = node->loop_vars[0], .value.type = type};
    sicore_vec_push(&ctx->variables, &variable, sizeof variable);
    ctx->loop_depth++;
    int result = 0;
    for (int32_t i = from; i < to; i++) {
        script_variable_t *variables = ctx->variables.data;
        memcpy(&variables[count].value.storage, &i, sizeof i);
        result = script_eval_scope(ctx, node->scope);
        if (result < 0) break;
        result = 0; /* Continue exits only the current loop body. */
    }
    ctx->loop_depth--;
    ctx->variables.size = count;
    return result;
}

int script_eval_scope(script_eval_t *ctx, ecs_script_scope_t *scope) {
    uint32_t variable_count = ctx->variables.size;
    script_template_t *templates = ctx->templates;
    ecs_script_node_t **statements = scope->stmts.data;
    int result = 0;
    for (uint32_t i = 0; i < scope->stmts.size && !result; i++) {
        ecs_script_node_t *node = statements[i];
        ctx->position = node->pos;
        switch (node->kind) {
        case EcsAstEntity: result = eval_entity(ctx, (ecs_script_entity_t*)node); break;
        case EcsAstTag: {
            ecs_script_tag_t *tag = (ecs_script_tag_t*)node;
            result = eval_id(ctx, &tag->id, NULL, false); break;
        }
        case EcsAstComponent: {
            ecs_script_component_t *component = (ecs_script_component_t*)node;
            result = eval_id(ctx, &component->id, component->expr, component->is_partial); break;
        }
        case EcsAstConst: result = eval_var(ctx, (ecs_script_var_node_t*)node); break;
        case EcsAstTemplate:
            result = script_template_declare(ctx, (ecs_script_template_node_t*)node); break;
        case EcsAstProp:
            if (!ctx->template_depth) result = script_eval_error(ctx, node->pos, "prop outside template");
            break;
        case EcsAstIf: {
            ecs_script_if_t *condition = (ecs_script_if_t*)node;
            bool value;
            result = script_value_write(ctx, condition->expr, script_backend_type_lookup("bool"), &value);
            if (!result) result = script_eval_scope(ctx, value ? condition->if_true : condition->if_false);
            break;
        }
        case EcsAstFor: result = eval_for(ctx, (ecs_script_for_t*)node); break;
        case EcsAstContinue:
            result = ctx->loop_depth ? 1 : script_eval_error(ctx, node->pos, "continue outside loop"); break;
        case EcsAstUsing: {
            const char *name = ((ecs_script_using_t*)node)->name;
            if (strcmp(name, "flecs.meta") && strcmp(name, "flecs"))
                result = script_eval_error(ctx, node->pos, "using namespace '%s' not implemented", name);
            break;
        }
        default: result = script_eval_error(ctx, node->pos, "statement kind %d not implemented", node->kind); break;
        }
    }
    ctx->variables.size = variable_count;
    ctx->templates = templates;
    return result;
}

int ecs_script_eval(const ecs_script_t *script, const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result) {
    if (result) *result = (ecs_script_eval_result_t){0};
    if (result) siecs_log_capture_push(true);
    ecs_script_runtime_t *runtime = desc ? desc->runtime : NULL;
    script_eval_t ctx = {.script = script};
    if (runtime && runtime->busy) {
        ecs_parser_error(NULL, NULL, 0, "script runtime is already evaluating");
        if (result) {
            result->error = siecs_log_capture_pop();
            siecs_log_get_captured_error_pos(&result->line, &result->column);
        }
        return -1;
    }
    if (runtime) {
        runtime->busy = true;
        ctx.variables = runtime->variables;
        ctx.owned_values = runtime->owned_values;
        ctx.values = runtime->values;
    } else {
        sicore_vec_init(&ctx.variables, sizeof(script_variable_t));
        sicore_vec_init(&ctx.owned_values, sizeof(script_owned_value_t));
    }
    const ecs_script_impl_t *impl = (const ecs_script_impl_t*)script;
    ctx.skip_unknown = (desc && desc->skip_unknown) || (impl && impl->skip_unknown);
    int status;
    if (!script) {
        ecs_parser_error(NULL, NULL, 0, "script must not be null");
        status = -1;
    } else if ((desc && desc->ir) || impl->ir_enabled) {
        status = script_eval_error(&ctx, NULL, "IR evaluation not implemented");
    } else {
        status = script_eval_scope(&ctx, impl->root);
    }
    script_owned_value_t *owned = ctx.owned_values.data;
    for (uint32_t i = ctx.owned_values.size; i; i--)
        script_backend_value_fini(owned[i - 1].component, owned[i - 1].ptr);
    ctx.owned_values.size = 0;
    if (runtime) {
        runtime->variables = ctx.variables;
        runtime->variables.size = 0;
        runtime->owned_values = ctx.owned_values;
        runtime->values = ctx.values;
        script_allocator_reset(&runtime->values);
        runtime->busy = false;
    } else {
        sicore_vec_fini(&ctx.variables);
        sicore_vec_fini(&ctx.owned_values);
        script_allocator_fini(&ctx.values);
    }
    if (result) {
        result->error = siecs_log_capture_pop();
        siecs_log_get_captured_error_pos(&result->line, &result->column);
    }
    return status;
}

int ecs_script_run_w_desc(const char *name, const char *code,
    const ecs_script_eval_desc_t *desc, ecs_script_eval_result_t *result) {
    if (result) *result = (ecs_script_eval_result_t){0};
    ecs_script_t *script = ecs_script_parse(name, code, desc, result);
    if (!script) return -1;
    int status = ecs_script_eval(script, desc, result);
    ecs_script_free(script);
    return status;
}

int ecs_script_run(const char *name, const char *code,
    ecs_script_eval_result_t *result) {
    return ecs_script_run_w_desc(name, code, NULL, result);
}

ecs_script_runtime_t *ecs_script_runtime_new(void) {
    ecs_script_runtime_t *runtime = calloc(1, sizeof(*runtime));
    if (runtime) {
        sicore_vec_init(&runtime->variables, sizeof(script_variable_t));
        sicore_vec_init(&runtime->owned_values, sizeof(script_owned_value_t));
    }
    return runtime;
}

void ecs_script_runtime_free(ecs_script_runtime_t *runtime) {
    if (!runtime) return;
    assert(!runtime->busy);
    sicore_vec_fini(&runtime->variables);
    sicore_vec_fini(&runtime->owned_values);
    script_allocator_fini(&runtime->values);
    free(runtime);
}
