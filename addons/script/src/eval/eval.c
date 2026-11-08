#include "eval.h"
#include "../../../../src/inheritance.h"
#include "../backend/transaction.h"
#include "async.h"

static int eval_id_name(script_eval_t *ctx, const ecs_expr_node_t *expr, const char **name) {
    if (!expr)
        return 0;
    const script_variable_t *variable =
        *name && (*name)[0] == '$' ? script_find_variable(ctx, *name + 1) : NULL;
    if (variable && variable->value.type == script_backend_type_lookup("entity"))
        return 0;
    script_value_t value;
    if (script_eval_expr(ctx, expr, &value))
        return -1;
    if (!sireflect_type_is_cstring(value.type))
        return script_eval_error(ctx, expr->pos, "expected string identifier");
    *name = *(const char *const *)script_value_data(&value);
    return 0;
}

static int
eval_id(script_eval_t *ctx, const ecs_script_id_t *id, const ecs_expr_node_t *value, bool partial) {
    if (!ctx->entity)
        return script_eval_error(ctx, NULL, "component outside entity scope");
    const char *first = id->first, *second = id->second;
    ecs_entity_t target = 0;
    if (second && second[0] == '$') {
        const script_variable_t *variable = script_find_variable(ctx, second + 1);
        if (variable && variable->value.type == script_backend_type_lookup("entity"))
            memcpy(&target, script_value_data(&variable->value), sizeof target);
    }
    if (eval_id_name(ctx, id->first_expr, &first) ||
        (!target && eval_id_name(ctx, id->second_expr, &second)))
        return -1;
    script_template_t *template = script_template_lookup(ctx, first);
    if (template && !second && !id->auto_override)
        return script_template_eval(ctx, template, value);
    script_symbol_t symbol = script_resolve_symbol(ctx, first);
    script_key_t component = 0;
    if (second) {
        if (id->auto_override)
            return script_eval_error(ctx, NULL, "auto_override requires a component");
        if (symbol.kind == ScriptSymbolRelation && !value) {
            if (!target)
                target = script_resolve_entity(ctx, second);
            if (!target)
                return script_eval_error(ctx, NULL, "unresolved target '%s'", second);
            if (symbol.id.relation == ecs_rid(IsA))
                script_backend_isa_add(ctx->entity, target);
            else if (script_backend_relation_add(ctx->entity, symbol.id.relation, target))
                return script_eval_error(
                    ctx,
                    NULL,
                    "exclusive Script relations accept one target per source"
                );
            return 0;
        }
        component = script_resolve_pair(ctx, first, second);
        if (!component)
            return -1;
    }
    if (symbol.kind == ScriptSymbolNone && ctx->skip_unknown) {
        script_backend_entity_ensure(first, ctx->parent);
        symbol.kind = ScriptSymbolEntity;
    }
    if (!component && symbol.kind == ScriptSymbolEntity) {
        /* An entity used as a tag maps to a real native zero-size component. */
        ecs_entity_t entity = script_resolve_entity(ctx, first);
        char *path = script_backend_entity_path(entity);
        if (!path)
            return script_eval_error(ctx, NULL, "could not resolve tag path");
        const char *name = path;
        if (!strncmp(name, "::", 2))
            name += 2;
        ecs_component_t component = script_backend_component_lookup(name);
        if (!component)
            component = script_backend_tag_register(name);
        free(path);
        symbol = (script_symbol_t){ ScriptSymbolComponent, { .component = component } };
    }
    if (!component && symbol.kind != ScriptSymbolComponent)
        return script_eval_error(ctx, NULL, "unresolved component '%s'", first);
    if (!component)
        component = symbol.id.component;
    if (id->auto_override) {
        if (component == ecs_id(Name) || component == ecs_id(Abstract))
            return script_eval_error(ctx, NULL, "component has protected inheritance semantics");
        const ecs_component_info_t *info = script_backend_component_info(component);
        size_t size = sizeof(ECS_AUTO_OVERRIDE_PREFIX) + strlen(info->name);
        char *name = script_alloc(&ctx->values, size);
        if (!name)
            return script_eval_error(ctx, NULL, "out of memory");
        snprintf(name, size, "%s%s", ECS_AUTO_OVERRIDE_PREFIX, info->name);
        ecs_component_t marker = script_backend_component_lookup(name);
        if (!marker)
            marker = script_backend_tag_register(name);
        if (!marker)
            return script_eval_error(ctx, NULL, "could not register override flag");
        script_backend_component_add(ctx->entity, marker);
    }
    if (!value) {
        script_backend_component_add(ctx->entity, component);
        return 0;
    }
    const ecs_component_info_t *info = script_backend_component_info(component);
    if (!info->type || !info->size) {
        if (ctx->skip_unknown) {
            script_backend_component_add(ctx->entity, component);
            return 0;
        }
        return script_eval_error(
            ctx,
            value->pos,
            "component '%s' has no reflected value",
            info->name
        );
    }
    /* Native reflected component metadata owns the layout; no JSON conversion. */
    sireflect_handle_t type = info->type;
    size_t size = (size_t)info->size;
    void *buffer = script_alloc(&ctx->values, size);
    if (!buffer)
        return script_eval_error(ctx, value->pos, "out of memory");
    const void *old =
        partial ? script_transaction_get(script_transaction_active, ctx->entity, component) : NULL;
    script_backend_value_init(component, buffer, old);
    int result;
    if (value->kind == EcsExprInitializer || value->kind == EcsExprEmptyInitializer) {
        result = script_value_write(ctx, value, type, buffer);
    } else {
        script_value_t source;
        result = script_eval_expr(ctx, value, &source);
        if (!result) {
            if (source.type == type)
                script_backend_value_copy(component, buffer, script_value_data(&source));
            else
                result = script_value_convert(ctx, value->pos, &source, type, buffer);
        }
    }
    if (!result) {
        uint8_t *members = NULL;
        if (partial &&
            (value->kind == EcsExprInitializer || value->kind == EcsExprEmptyInitializer)) {
            const sireflect_fields_t *fields = sireflect_type_fields(type);
            if (fields->field_count) {
                members = script_alloc(&ctx->values, fields->field_count);
                const ecs_expr_initializer_t *init = (const ecs_expr_initializer_t *)value;
                const ecs_expr_initializer_element_t *elements = init->elements.data;
                for (uint32_t i = 0; i < init->elements.size; i++) {
                    for (size_t j = 0; j < fields->field_count; j++) {
                        if (elements[i].member ? !strcmp(elements[i].member, fields->fields[j].name)
                                               : i == j) {
                            members[j] = 1;
                            break;
                        }
                    }
                }
            }
        }
        script_transaction_set_members(
            script_transaction_active,
            ctx->entity,
            component,
            buffer,
            members
        );
    }
    script_backend_value_fini(component, buffer);
    return result;
}

static int append_struct_member(
    script_eval_t *ctx,
    ecs_strbuf_t *fields,
    const char *member,
    const ecs_expr_node_t *expression
) {
    if (!member || !expression || (!isalpha((unsigned char)*member) && *member != '_'))
        return script_eval_error(ctx, expression ? expression->pos : NULL, "expected member: type");
    for (const char *cursor = member + 1; *cursor; cursor++)
        if (!isalnum((unsigned char)*cursor) && *cursor != '_')
            return script_eval_error(
                ctx,
                expression->pos,
                "invalid struct member name '%s'",
                member
            );
    uint64_t count = 0;
    if (expression->kind == EcsExprElement) {
        const ecs_expr_element_t *array = (const ecs_expr_element_t *)expression;
        const ecs_expr_value_node_t *length = (const ecs_expr_value_node_t *)array->index;
        if (array->index->kind != EcsExprValue ||
            (length->literal_kind != ScriptLiteralI64 && length->literal_kind != ScriptLiteralU64))
            return script_eval_error(
                ctx,
                array->index->pos,
                "expected a positive array length literal"
            );
        count = length->literal_kind == ScriptLiteralU64 ? length->storage.u64
                                                         : (uint64_t)length->storage.i64;
        if (!count || count > UINT32_MAX)
            return script_eval_error(ctx, array->index->pos, "array length out of range");
        expression = array->left;
    }
    if (expression->kind != EcsExprIdentifier)
        return script_eval_error(ctx, expression->pos, "expected a member type name");
    const char *name = ((const ecs_expr_identifier_t *)expression)->value;
    sireflect_handle_t type = script_resolve_type(ctx, name);
    if (!type)
        return script_eval_error(ctx, expression->pos, "unknown member type '%s'", name);
    if (count && sireflect_type_size(type) > SIZE_MAX / count)
        return script_eval_error(ctx, expression->pos, "array size out of range");
    ecs_strbuf_appendstr(fields, sireflect_type_name(type));
    ecs_strbuf_appendch(fields, ' ');
    ecs_strbuf_appendstr(fields, member);
    if (count) {
        char suffix[32];
        snprintf(suffix, sizeof suffix, "[%u]", (uint32_t)count);
        ecs_strbuf_appendstr(fields, suffix);
    }
    ecs_strbuf_appendstr(fields, "; ");
    return 0;
}

static int eval_struct(script_eval_t *ctx, ecs_script_entity_t *entity) {
    if (!entity->name)
        return script_eval_error(ctx, entity->node.pos, "expected a struct name");
    const ecs_script_node_t *const *nodes = entity->scope->stmts.data;
    const ecs_expr_initializer_t *initializer = NULL;
    if (entity->kind_w_expr) {
        if (entity->scope->stmts.size != 1 || nodes[0]->kind != EcsAstComponent)
            return script_eval_error(ctx, entity->node.pos, "expected a struct member list");
        const ecs_expr_node_t *expression = ((const ecs_script_component_t *)nodes[0])->expr;
        if (!expression ||
            (expression->kind != EcsExprInitializer && expression->kind != EcsExprEmptyInitializer))
            return script_eval_error(ctx, entity->node.pos, "expected a struct member list");
        initializer = (const ecs_expr_initializer_t *)expression;
    }
    uint32_t count = initializer ? initializer->elements.size : entity->scope->stmts.size;
    const ecs_expr_initializer_element_t *members = initializer ? initializer->elements.data : NULL;
    ecs_strbuf_t fields = ECS_STRBUF_INIT;
    ecs_strbuf_appendstr(&fields, "{ ");
    for (uint32_t i = 0; i < count; i++) {
        const char *member = NULL;
        const ecs_expr_node_t *expression = NULL;
        if (initializer) {
            member = members[i].member;
            expression = members[i].value;
        } else if (nodes[i]->kind == EcsAstComponent) {
            const ecs_script_component_t *component = (const ecs_script_component_t *)nodes[i];
            if (!component->id.second && !component->id.first_expr &&
                !component->id.auto_override) {
                member = component->id.first;
                expression = component->expr;
            }
        }
        if (append_struct_member(ctx, &fields, member, expression)) {
            free(ecs_strbuf_get(&fields));
            return -1;
        }
    }
    ecs_strbuf_appendch(&fields, '}');
    char *description = ecs_strbuf_get(&fields);
    ecs_entity_t metadata = script_backend_entity_ensure(entity->name, ctx->parent);
    if (script_annotations_apply(ctx, metadata)) {
        free(description);
        return -1;
    }
    char *path = script_backend_entity_path(metadata);
    const char *name = path;
    if (name && !strncmp(name, "::", 2))
        name += 2;
    ecs_component_t component =
        name ? script_backend_component_register_dynamic(name, description) : 0;
    free(description);
    free(path);
    if (!component)
        return script_eval_error(
            ctx,
            entity->node.pos,
            "could not register struct '%s'",
            entity->name
        );
    const ecs_component_info_t *info = script_backend_component_info(component);
    const sireflect_fields_t *registered = sireflect_type_fields(info->type);
    for (size_t i = 0; i < registered->field_count; i++)
        script_backend_entity_ensure(registered->fields[i].name, metadata);
    return 0;
}

static int apply_with(script_eval_t *ctx, const script_with_frame_t *frame) {
    if (!frame)
        return 0;
    if (apply_with(ctx, frame->previous))
        return -1;
    const script_variable_t *captures = ctx->captures;
    uint32_t capture_count = ctx->capture_count, base = ctx->variable_base;
    script_function_t *functions = ctx->functions;
    script_import_t *imports = ctx->imports;
    const ecs_script_t *program = ctx->script;
    ctx->captures = frame->captures;
    ctx->capture_count = frame->capture_count;
    ctx->variable_base = ctx->variables.size;
    ctx->functions = frame->functions;
    ctx->imports = frame->imports;
    ctx->script = frame->program;
    ecs_script_node_t **nodes = frame->expressions->stmts.data;
    int status = 0;
    for (uint32_t i = 0; i < frame->expressions->stmts.size && !status; i++)
        status = script_eval_node(ctx, nodes[i]);
    ctx->captures = captures;
    ctx->capture_count = capture_count;
    ctx->variable_base = base;
    ctx->functions = functions;
    ctx->imports = imports;
    ctx->script = program;
    return status;
}

ecs_script_scope_t *
script_pair_expressions(script_eval_t *ctx, const ecs_script_pair_scope_t *pair) {
    ecs_script_tag_t *tag = script_alloc(&ctx->values, sizeof(*tag));
    tag->node =
        (ecs_script_node_t){ .kind = EcsAstTag, .pos = pair->node.pos, .end = pair->node.end };
    tag->id = pair->id;
    ecs_script_node_t **item = script_alloc(&ctx->values, sizeof(*item));
    *item = &tag->node;
    ecs_script_scope_t *scope = script_alloc(&ctx->values, sizeof(*scope));
    scope->stmts = (sicore_vec_t){ .data = item, .size = 1 };
    return scope;
}

void script_with_scope_push(
    script_eval_t *ctx,
    ecs_script_scope_t *expressions,
    const ecs_script_pair_scope_t *pair
) {
    script_with_frame_t *frame = script_alloc(&ctx->values, sizeof(*frame));
    *frame = (script_with_frame_t){
        .previous = ctx->with,
        .expressions = pair ? script_pair_expressions(ctx, pair) : expressions,
        .capture_count = ctx->capture_count + ctx->variables.size - ctx->variable_base,
        .pair = pair,
        .program = ctx->script,
        .functions = ctx->functions,
        .imports = ctx->imports
    };
    frame->captures = script_alloc(&ctx->values, frame->capture_count * sizeof(script_variable_t));
    if (ctx->capture_count)
        memcpy(frame->captures, ctx->captures, ctx->capture_count * sizeof(script_variable_t));
    uint32_t locals = ctx->variables.size - ctx->variable_base;
    if (locals)
        memcpy(
            frame->captures + ctx->capture_count,
            (script_variable_t *)ctx->variables.data + ctx->variable_base,
            locals * sizeof(script_variable_t)
        );
    ctx->with = frame;
}

int script_eval_with(script_eval_t *ctx, const ecs_script_with_t *with) {
    script_with_frame_t *previous = ctx->with;
    script_with_scope_push(ctx, with->expressions, NULL);
    int status = script_eval_scope(ctx, with->scope);
    ctx->with = previous;
    return status;
}

int script_entity_scope_begin(script_eval_t *ctx, const ecs_script_entity_t *entity) {
    const char *name = entity->name;
    if (entity->name_expr) {
        script_value_t value;
        if (script_eval_expr(ctx, entity->name_expr, &value))
            return -1;
        if (!sireflect_type_is_cstring(value.type))
            return script_eval_error(ctx, entity->node.pos, "expected string entity name");
        name = value.storage.string;
    }
    if (script_transaction_active && script_transaction_active->region) {
        script_region_t *regions = script_transaction_active->regions.data;
        script_region_t *region = &regions[script_transaction_active->region - 1];
        if (region->node == &entity->node)
            region->name = name ? script_strdup(name) : NULL;
    }
    ecs_entity_t output = !name && ctx->instance && script_transaction_active
                              ? script_instance_anonymous(
                                    ctx->instance,
                                    script_transaction_active,
                                    ctx->script,
                                    entity,
                                    ctx->parent
                                )
                              : script_backend_entity_ensure(name, ctx->parent);
    if (!output)
        return script_eval_error(ctx, entity->node.pos, "invalid entity name");
    if (script_annotations_apply(ctx, output))
        return -1;
    bool prefab = entity->kind && !strcmp(entity->kind, "prefab");
    if (entity->kind && !entity->kind_w_expr) {
        if (!prefab) {
            ecs_script_id_t id = { .first = entity->kind };
            ecs_entity_t previous = ctx->entity;
            ctx->entity = output;
            int result = eval_id(ctx, &id, NULL, false);
            ctx->entity = previous;
            if (result)
                return result;
        }
    }
    ctx->entity = ctx->parent = output;
    return apply_with(ctx, ctx->with);
}

static int eval_entity(script_eval_t *ctx, ecs_script_entity_t *entity) {
    if (entity->kind && !strcmp(entity->kind, "struct"))
        return eval_struct(ctx, entity);
    ecs_entity_t previous_entity = ctx->entity, previous_parent = ctx->parent;
    int result = script_entity_scope_begin(ctx, entity);
    if (!result)
        result = script_eval_scope(ctx, entity->scope);
    if (!result && entity->kind && !strcmp(entity->kind, "prefab"))
        script_backend_prefab(ctx->entity);
    ctx->entity = previous_entity;
    ctx->parent = previous_parent;
    return result;
}

static int eval_var(script_eval_t *ctx, ecs_script_var_node_t *node) {
    script_variable_t variable = { .name = node->name,
                                   .mutable = node->node.kind == EcsAstMut ||
                                              node->node.kind == EcsAstExportMut };
    if (node->type) {
        variable.value.type = script_resolve_type(ctx, node->type);
        if (!variable.value.type)
            return script_eval_error(ctx, node->node.pos, "unknown variable type '%s'", node->type);
        if (node->type_is_vector) {
            script_value_t source = { .collection = true };
            if (node->expr && script_eval_expr(ctx, node->expr, &source))
                return -1;
            if (script_collection_convert(
                    ctx,
                    node->node.pos,
                    &source,
                    variable.value.type,
                    &variable.value
                ))
                return -1;
        } else {
            void *storage = script_value_storage(ctx, variable.value.type, &variable.value);
            if (node->expr && script_value_write(ctx, node->expr, variable.value.type, storage))
                return -1;
        }
    } else {
        script_value_t initial;
        if (script_eval_expr(ctx, node->expr, &initial))
            return -1;
        if (variable.mutable && initial.type && ecs_reflected_ops(initial.type).reflected_type) {
            void *storage = script_value_storage(ctx, initial.type, &variable.value);
            if (script_value_convert(ctx, node->node.pos, &initial, initial.type, storage))
                return -1;
        } else
            variable.value = initial;
    }
    sicore_vec_push(&ctx->variables, &variable, sizeof variable);
    return 0;
}

static int eval_for(script_eval_t *ctx, ecs_script_for_t *node) {
    if (!node->from || !node->to) {
        script_value_t collection;
        if (script_eval_expr(ctx, node->expr, &collection))
            return -1;
        if (!collection.collection || node->loop_var_count > 2)
            return script_eval_error(
                ctx,
                node->node.pos,
                "expected collection with one value or (index, value) binding"
            );
        uint32_t previous_size = ctx->variables.size;
        ctx->loop_depth++;
        int result = 0;
        for (uint32_t i = 0; i < collection.count; i++) {
            if (!ctx->instruction_budget) {
                result = script_eval_error(ctx, node->node.pos, "instruction budget exhausted");
                break;
            }
            ctx->instruction_budget--;
            if (node->loop_var_count == 2) {
                script_variable_t index = {
                    .name = node->loop_vars[0],
                    .value = { .type = script_backend_type_lookup("uint64_t"), .storage.u64 = i }
                };
                sicore_vec_push(&ctx->variables, &index, sizeof index);
            }
            script_variable_t item = { .name = node->loop_vars[node->loop_var_count - 1],
                                       .value = script_collection_at(&collection, i) };
            sicore_vec_push(&ctx->variables, &item, sizeof item);
            result = script_eval_scope(ctx, node->scope);
            ctx->variables.size = previous_size;
            if (result < 0)
                break;
            result = 0;
        }
        ctx->loop_depth--;
        return result;
    }
    if (node->loop_var_count != 1)
        return script_eval_error(ctx, node->node.pos, "range requires one iteration variable");
    int32_t from = 0, to = 0;
    sireflect_handle_t type = script_backend_type_lookup("int32_t");
    if (script_value_write(ctx, node->from, type, &from) ||
        script_value_write(ctx, node->to, type, &to))
        return -1;
    uint32_t count = ctx->variables.size;
    script_variable_t variable = { .name = node->loop_vars[0], .value.type = type };
    sicore_vec_push(&ctx->variables, &variable, sizeof variable);
    ctx->loop_depth++;
    int result = 0;
    for (int32_t i = from; i < to; i++) {
        if (!ctx->instruction_budget) {
            result = script_eval_error(ctx, node->node.pos, "instruction budget exhausted");
            break;
        }
        ctx->instruction_budget--;
        script_variable_t *variables = ctx->variables.data;
        memcpy(&variables[count].value.storage, &i, sizeof i);
        result = script_eval_scope(ctx, node->scope);
        if (result < 0)
            break;
        result = 0; /* Continue exits only the current loop body. */
    }
    ctx->loop_depth--;
    ctx->variables.size = count;
    return result;
}

typedef int (*script_execute_t)(script_eval_t *ctx, const ecs_script_node_t *node);

typedef struct script_instruction_t {
    script_execute_t execute;
    const ecs_script_node_t *operand;
} script_instruction_t;

/* Bind only referenced lexical values when executing a compiled region. */
static bool compile_region_expression(
    ecs_script_impl_t *impl,
    const ecs_expr_node_t *expr,
    sicore_vec_t *names
) {
    if (!expr)
        return true;
    switch (expr->kind) {
    case EcsExprValue:
        return true;
    case EcsExprIdentifier:
    case EcsExprVariable: {
        const char *name = expr->kind == EcsExprVariable
                               ? ((const ecs_expr_variable_t *)expr)->name
                               : ((const ecs_expr_identifier_t *)expr)->value;
        const char *const *existing = names->data;
        for (uint32_t i = 0; i < names->size; i++)
            if (!strcmp(existing[i], name))
                return true;
        *(const char **)script_vec_append(&impl->allocator, names, sizeof(name)) = name;
        return true;
    }
    case EcsExprInterpolatedString: {
        const ecs_expr_interpolated_string_t *string = (const ecs_expr_interpolated_string_t *)expr;
        const ecs_expr_fragment_t *fragments = string->fragments.data;
        for (uint32_t i = 0; i < string->fragments.size; i++)
            if (!compile_region_expression(impl, fragments[i].expr, names))
                return false;
        return true;
    }
    case EcsExprInitializer:
    case EcsExprEmptyInitializer: {
        const ecs_expr_initializer_t *init = (const ecs_expr_initializer_t *)expr;
        const ecs_expr_initializer_element_t *items = init->elements.data;
        for (uint32_t i = 0; i < init->elements.size; i++)
            if (!compile_region_expression(impl, items[i].value, names))
                return false;
        return true;
    }
    case EcsExprMember:
        return compile_region_expression(impl, ((const ecs_expr_member_t *)expr)->left, names);
    case EcsExprElement: {
        const ecs_expr_element_t *element = (const ecs_expr_element_t *)expr;
        return compile_region_expression(impl, element->left, names) &&
               compile_region_expression(impl, element->index, names) &&
               compile_region_expression(impl, element->second, names);
    }
    case EcsExprUnary:
        return compile_region_expression(impl, ((const ecs_expr_unary_t *)expr)->expr, names);
    case EcsExprBinary: {
        const ecs_expr_binary_t *binary = (const ecs_expr_binary_t *)expr;
        return compile_region_expression(impl, binary->left, names) &&
               compile_region_expression(impl, binary->right, names);
    }
    case EcsExprFunction: {
        const ecs_expr_function_t *call = (const ecs_expr_function_t *)expr;
        return call->function_name &&
               compile_region_expression(impl, (const ecs_expr_node_t *)call->args, names);
    }
    default:
        return false;
    }
}

static void compile_entity_region(ecs_script_impl_t *impl, ecs_script_entity_t *entity) {
    if (entity->kind || !entity->name ||
        !compile_region_expression(impl, entity->name_expr, &entity->capture_names))
        return;
    const ecs_script_node_t *const *nodes = entity->scope->stmts.data;
    for (uint32_t i = 0; i < entity->scope->stmts.size; i++) {
        if (nodes[i]->kind != EcsAstComponent)
            return;
        const ecs_script_component_t *component = (const ecs_script_component_t *)nodes[i];
        if (component->id.second || component->id.first_expr || component->id.auto_override ||
            !compile_region_expression(impl, component->expr, &entity->capture_names))
            return;
    }
    entity->cacheable = true;
}

static bool
capture_entity(script_eval_t *ctx, const ecs_script_entity_t *entity, script_eval_t *captures) {
    if (!entity->cacheable || ctx->annotations || !ctx->instance || ctx->async_block ||
        ctx->templates || ctx->functions || ctx->with || !script_transaction_active)
        return false;
    const char *const *names = entity->capture_names.data;
    for (uint32_t i = 0; i < entity->capture_names.size; i++) {
        const script_variable_t *variable = script_find_variable(ctx, names[i]);
        if (variable)
            sicore_vec_push(&captures->variables, variable, sizeof(*variable));
    }
    return true;
}

static int execute_entity(script_eval_t *ctx, const ecs_script_node_t *node) {
    return eval_entity(ctx, (ecs_script_entity_t *)node);
}

static int execute_tag(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_tag_t *tag = (const ecs_script_tag_t *)node;
    return eval_id(ctx, &tag->id, NULL, false);
}

static int execute_component(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_component_t *component = (const ecs_script_component_t *)node;
    return eval_id(ctx, &component->id, component->expr, component->is_partial);
}

static int execute_const(script_eval_t *ctx, const ecs_script_node_t *node) {
    return eval_var(ctx, (ecs_script_var_node_t *)node);
}

static int execute_template(script_eval_t *ctx, const ecs_script_node_t *node) {
    return script_template_declare(ctx, (const ecs_script_template_node_t *)node);
}

static int execute_prop(script_eval_t *ctx, const ecs_script_node_t *node) {
    return ctx->template_depth ? 0 : script_eval_error(ctx, node->pos, "prop outside template");
}

static int execute_if(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_if_t *condition = (const ecs_script_if_t *)node;
    bool value;
    if (script_value_write(ctx, condition->expr, script_backend_type_lookup("bool"), &value))
        return -1;
    return script_eval_scope(ctx, value ? condition->if_true : condition->if_false);
}

static int execute_for(script_eval_t *ctx, const ecs_script_node_t *node) {
    return eval_for(ctx, (ecs_script_for_t *)node);
}

static int execute_continue(script_eval_t *ctx, const ecs_script_node_t *node) {
    return ctx->loop_depth ? 1 : script_eval_error(ctx, node->pos, "continue outside loop");
}

static int execute_annotation(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_annot_t *annotation = (const ecs_script_annot_t *)node;
    script_annotation_t *pending = script_alloc(&ctx->values, sizeof(*pending));
    *pending = (script_annotation_t){ .name = annotation->name,
                                      .value = annotation->expr,
                                      .next = ctx->annotations };
    ctx->annotations = pending;
    return 0;
}

static int execute_using(script_eval_t *ctx, const ecs_script_node_t *node) {
    const char *name = ((const ecs_script_using_t *)node)->name;
    if (!strcmp(name, "siecs.meta") || !strcmp(name, "siecs"))
        return 0;
    ecs_entity_t scope = script_resolve_entity(ctx, name);
    if (!scope)
        return script_eval_error(ctx, node->pos, "unresolved namespace '%s'", name);
    script_import_t *import = script_alloc(&ctx->values, sizeof(*import));
    *import = (script_import_t){ .scope = scope, .next = ctx->imports };
    ctx->imports = import;
    return 0;
}

static int execute_unsupported(script_eval_t *ctx, const ecs_script_node_t *node) {
    return script_eval_error(ctx, node->pos, "statement kind %d not implemented", node->kind);
}

static int execute_include(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_include_t *include = (const ecs_script_include_t *)node;
    const ecs_script_t *previous_script = ctx->script;
    ctx->script = include->program;
    ecs_script_scope_t *root = siecs_script_impl(include->program)->root;
    const ecs_script_node_t *const *nodes = root->stmts.data;
    int status = 0;
    for (uint32_t i = 0; i < root->stmts.size && !status; i++)
        status = script_eval_node(ctx, nodes[i]);
    ctx->script = previous_script;
    return status;
}

static int execute_module(script_eval_t *ctx, const ecs_script_node_t *node) {
    const char *name = ((const ecs_script_module_t *)node)->name;
    ctx->parent = script_backend_entity_ensure(name, ctx->parent);
    return ctx->parent ? 0 : script_eval_error(ctx, node->pos, "invalid module path");
}

static int execute_pair_scope(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_pair_scope_t *pair = (const ecs_script_pair_scope_t *)node;
    script_with_frame_t *previous = ctx->with;
    script_with_scope_push(ctx, NULL, pair);
    int status = script_eval_scope(ctx, pair->scope);
    ctx->with = previous;
    return status;
}

static int execute_with(script_eval_t *ctx, const ecs_script_node_t *node) {
    return script_eval_with(ctx, (const ecs_script_with_t *)node);
}

static int execute_async(script_eval_t *ctx, const ecs_script_node_t *node) {
    return script_async_spawn(ctx, (const ecs_script_async_t *)node);
}

static int execute_assign(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_assign_t *assignment = (const ecs_script_assign_t *)node;
    const script_variable_t *variable = script_find_variable(ctx, assignment->name);
    if (!variable) {
        if (ctx->entity) {
            ecs_script_id_t id = { .first = assignment->name };
            return eval_id(ctx, &id, assignment->expr, true);
        }
        return script_eval_error(
            ctx,
            node->pos,
            "unknown assignment variable '%s'",
            assignment->name
        );
    }
    if (!variable->mutable)
        return script_eval_error(
            ctx,
            node->pos,
            "cannot assign immutable variable '%s'",
            variable->name
        );
    sireflect_handle_t type = variable->value.type;
    sireflect_handle_t item_type = variable->value.item_type;
    bool collection = variable->value.collection;
    script_value_t evaluated = { 0 };
    if (collection) {
        if (script_eval_expr(ctx, assignment->expr, &evaluated))
            return -1;
    } else {
        void *storage = script_value_storage(ctx, type, &evaluated);
        if (script_value_write(ctx, assignment->expr, type, storage))
            return -1;
    }
    /* Calls in the RHS can grow or restore the variable vector. Resolve the
     * destination only after evaluation has finished. */
    variable = script_find_variable(ctx, assignment->name);
    if (collection) {
        if (!evaluated.collection)
            return script_eval_error(ctx, node->pos, "expected collection value");
        script_value_t converted = evaluated;
        if (item_type &&
            script_collection_convert(ctx, node->pos, &evaluated, item_type, &converted))
            return -1;
        ((script_variable_t *)variable)->value = converted;
        return 0;
    }
    void *destination =
        variable->value.ptr ? (void *)variable->value.ptr : (void *)&variable->value.storage;
    return script_value_convert(ctx, node->pos, &evaluated, type, destination);
}

ecs_script_scope_t *script_matching_catch(const ecs_script_try_t *attempt, const char *message) {
    const ecs_script_catch_t *clauses = attempt->catches.data;
    const char *separator = message ? strchr(message, ':') : NULL;
    size_t length = separator ? (size_t)(separator - message) : strlen("RuntimeError");
    const char *type = separator ? message : "RuntimeError";
    for (uint32_t i = 0; i < attempt->catches.size; i++) {
        if (!clauses[i].error ||
            (strlen(clauses[i].error) == length && !strncmp(clauses[i].error, type, length)))
            return clauses[i].scope;
    }
    return NULL;
}

static int execute_try(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_try_t *attempt = (const ecs_script_try_t *)node;
    script_transaction_t *transaction = script_transaction_active;
    script_savepoint_t savepoint;
    script_transaction_savepoint(transaction, &savepoint);
    script_eval_t snapshot;
    script_context_copy(&snapshot, ctx);
    uint32_t owned_count = ctx->owned_values.size;
    siecs_log_capture_push(true);
    int status = script_eval_scope(ctx, attempt->try_scope);
    char *message = siecs_log_capture_pop();
    if (status < 0 || transaction->failed) {
        ecs_script_scope_t *handler = script_matching_catch(attempt, message);
        script_context_restore(ctx, &snapshot, owned_count);
        script_transaction_rewind(transaction, &savepoint);
        if (handler)
            status = script_eval_scope(ctx, handler);
        else
            status = script_eval_error(
                ctx,
                node->pos,
                "%s",
                message ? message : "RuntimeError: transaction failed"
            );
    } else {
        script_context_fini(&snapshot);
        script_transaction_discard_savepoint(&savepoint);
    }
    free(message);
    return status;
}

static int execute_while(script_eval_t *ctx, const ecs_script_node_t *node) {
    const ecs_script_while_t *loop = (const ecs_script_while_t *)node;
    ctx->loop_depth++;
    int result = 0;
    for (;;) {
        if (!ctx->instruction_budget) {
            result = script_eval_error(ctx, node->pos, "instruction budget exhausted");
            break;
        }
        ctx->instruction_budget--;
        bool condition = false;
        if (script_value_write(ctx, loop->expr, script_backend_type_lookup("bool"), &condition)) {
            result = -1;
            break;
        }
        if (!condition)
            break;
        result = script_eval_scope(ctx, loop->scope);
        if (result < 0)
            break;
        result = 0;
    }
    ctx->loop_depth--;
    return result;
}

static int execute_function(script_eval_t *ctx, const ecs_script_node_t *node) {
    script_function_t *function = script_alloc(&ctx->values, sizeof(*function));
    function->imports = ctx->imports;
    function->node = (const ecs_script_function_node_t *)node;
    function->program = ctx->script;
    function->next = ctx->functions;
    function->capture_count = ctx->capture_count + ctx->variables.size - ctx->variable_base;
    if (function->capture_count) {
        function->captures =
            script_alloc(&ctx->values, function->capture_count * sizeof(script_variable_t));
        if (ctx->capture_count)
            memcpy(
                function->captures,
                ctx->captures,
                ctx->capture_count * sizeof(script_variable_t)
            );
        uint32_t locals = ctx->variables.size - ctx->variable_base;
        if (locals)
            memcpy(
                function->captures + ctx->capture_count,
                (script_variable_t *)ctx->variables.data + ctx->variable_base,
                locals * sizeof(script_variable_t)
            );
    }
    ctx->functions = function;
    return 0;
}

static int execute_expression(script_eval_t *ctx, const ecs_script_node_t *node) {
    script_value_t unused;
    return script_eval_expr(ctx, ((const ecs_script_await_t *)node)->expr, &unused);
}

static script_execute_t instruction_handler(ecs_script_node_kind_t kind) {
    switch (kind) {
    case EcsAstEntity:
        return execute_entity;
    case EcsAstTag:
        return execute_tag;
    case EcsAstComponent:
        return execute_component;
    case EcsAstConst:
    case EcsAstMut:
    case EcsAstExportConst:
    case EcsAstExportMut:
        return execute_const;
    case EcsAstAsync:
        return execute_async;
    case EcsAstTry:
        return execute_try;
    case EcsAstInclude:
        return execute_include;
    case EcsAstAssign:
        return execute_assign;
    case EcsAstWhile:
        return execute_while;
    case EcsAstFunction:
        return execute_function;
    case EcsAstExpression:
        return execute_expression;
    case EcsAstTemplate:
        return execute_template;
    case EcsAstProp:
        return execute_prop;
    case EcsAstIf:
        return execute_if;
    case EcsAstFor:
        return execute_for;
    case EcsAstContinue:
        return execute_continue;
    case EcsAstUsing:
        return execute_using;
    case EcsAstAnnotation:
        return execute_annotation;
    case EcsAstModule:
        return execute_module;
    case EcsAstWith:
        return execute_with;
    case EcsAstPairScope:
        return execute_pair_scope;
    default:
        return execute_unsupported;
    }
}

/* Persist control flow and its lexical transition, as well as leaf entities.
 * Children remain independently reusable when an enclosing condition changes. */
static int
execute_region(script_eval_t *ctx, const ecs_script_node_t *node, script_execute_t execute) {
    script_transaction_t *transaction = script_transaction_active;
    bool eligible = ctx->instance && !ctx->async_block && transaction &&
                    (!transaction->region ||
                     !((script_region_t *)transaction->regions.data)[transaction->region - 1].leaf);
    if (node->kind == EcsAstComponent || node->kind == EcsAstTag || node->kind == EcsAstProp ||
        node->kind == EcsAstContinue || node->kind == EcsAstAwait || node->kind == EcsAstAsync ||
        node->kind == EcsAstTemplate ||
        (node->kind == EcsAstEntity && ((const ecs_script_entity_t *)node)->kind &&
         !strcmp(((const ecs_script_entity_t *)node)->kind, "struct")))
        eligible = false;
    if (!eligible)
        return execute(ctx, node);
    script_eval_t captures = { .entity = ctx->entity,
                               .parent = ctx->parent,
                               .imports = ctx->imports };
    sicore_vec_init(&captures.variables, sizeof(script_variable_t));
    bool leaf = node->kind == EcsAstEntity &&
                capture_entity(ctx, (const ecs_script_entity_t *)node, &captures);
    script_eval_t *snapshot = script_context_clone(leaf ? &captures : ctx);
    sicore_vec_fini(&captures.variables);
    uint32_t parent = transaction->region;
    script_region_t region = { .program = ctx->script,
                               .captures = snapshot,
                               .node = node,
                               .parent = ctx->parent,
                               .leaf = leaf,
                               .values_begin = transaction->values.size,
                               .reads_begin = transaction->reads.size,
                               .lookups_begin = transaction->lookups.size,
                               .produced_begin = transaction->produced.size,
                               .relations_begin = transaction->relations.size,
                               .prefabs_begin = transaction->prefabs.size,
                               .identities_begin = transaction->identities.size,
                               .reusable = true };
    sicore_vec_push(&transaction->regions, &region, sizeof region);
    uint32_t id = transaction->regions.size;
    uint32_t blocks = transaction->async_blocks.size, launches = transaction->async_launches.size;
    transaction->region = id;
    int status = 0;
    bool replayed = script_instance_region_replay(
        ctx->instance,
        transaction,
        ctx->script,
        node,
        ctx->parent,
        ctx
    );
    if (!replayed)
        status = execute(ctx, node);
    transaction->region = parent;
    script_region_t *saved = &((script_region_t *)transaction->regions.data)[id - 1];
    saved->values_end = transaction->values.size;
    saved->reads_end = transaction->reads.size;
    saved->lookups_end = transaction->lookups.size;
    saved->produced_end = transaction->produced.size;
    saved->relations_end = transaction->relations.size;
    saved->prefabs_end = transaction->prefabs.size;
    saved->identities_end = transaction->identities.size;
    saved->children_end = transaction->regions.size;
    saved->reusable &= !status && blocks == transaction->async_blocks.size &&
                       launches == transaction->async_launches.size;
    if (!leaf && !replayed) {
        saved->result = script_context_clone(ctx);
    }
    return status;
}

int script_eval_node(script_eval_t *ctx, const ecs_script_node_t *node) {
    if (!ctx->instruction_budget--)
        return script_eval_error(ctx, node->pos, "instruction budget exhausted");
    ctx->position = node->pos;
    return execute_region(ctx, node, instruction_handler(node->kind));
}

static int compile_scope(ecs_script_scope_t *scope, void *data) {
    if (scope->instructions)
        return 0;
    ecs_script_impl_t *impl = data;
    if (!scope->stmts.size)
        return 0;
    scope->instructions =
        script_alloc(&impl->allocator, scope->stmts.size * sizeof(script_instruction_t));
    if (!scope->instructions)
        return -1;
    ecs_script_node_t **nodes = scope->stmts.data;
    for (uint32_t i = 0; i < scope->stmts.size; i++) {
        if (nodes[i]->kind == EcsAstAnnotation &&
            (i + 1 == scope->stmts.size ||
             (nodes[i + 1]->kind != EcsAstAnnotation && nodes[i + 1]->kind != EcsAstEntity &&
              nodes[i + 1]->kind != EcsAstTemplate))) {
            ecs_parser_error(
                impl->pub.name,
                impl->pub.code,
                nodes[i]->pos - impl->pub.code,
                "annotation must precede an entity, struct or template"
            );
            return -1;
        }
        scope->instructions[i] =
            (script_instruction_t){ instruction_handler(nodes[i]->kind), nodes[i] };
        if (nodes[i]->kind == EcsAstEntity)
            compile_entity_region(impl, (ecs_script_entity_t *)nodes[i]);
    }
    return 0;
}

int script_ir_compile(ecs_script_t *script) {
    ecs_script_impl_t *impl = siecs_script_impl(script);
    for (ecs_script_scope_t *scope = impl->scopes; scope; scope = scope->next)
        if (compile_scope(scope, impl))
            return -1;
    return 0;
}

int script_eval_scope(script_eval_t *ctx, ecs_script_scope_t *scope) {
    uint32_t variable_count = ctx->variables.size;
    script_template_t *templates = ctx->templates;
    script_function_t *functions = ctx->functions;
    script_import_t *imports = ctx->imports;
    ecs_script_node_t **statements = scope->stmts.data;
    int result = 0;
    for (uint32_t i = 0; i < scope->stmts.size && !result; i++) {
        const ecs_script_node_t *node = statements[i];
        if (!ctx->instruction_budget--)
            return script_eval_error(ctx, node->pos, "instruction budget exhausted");
        ctx->position = node->pos;
        if (ctx->ir) {
            const script_instruction_t *instruction = &scope->instructions[i];
            result = execute_region(ctx, instruction->operand, instruction->execute);
        } else {
            result = execute_region(ctx, node, instruction_handler(node->kind));
        }
    }
    ctx->variables.size = variable_count;
    ctx->templates = templates;
    ctx->functions = functions;
    ctx->imports = imports;
    return result;
}

int ecs_script_eval(
    const ecs_script_t *script,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
) {
    if (result)
        *result = (ecs_script_eval_result_t){ 0 };
    if (result)
        siecs_log_capture_push(true);
    ecs_script_runtime_t *runtime = desc ? desc->runtime : NULL;
    script_eval_t ctx = { .script = script,
                          .instance = script_current_instance,
                          .parent = desc ? desc->parent : 0,
                          .instruction_budget = desc && desc->instruction_budget
                                                    ? desc->instruction_budget
                                                    : 1000000 };
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
    script_transaction_t local;
    bool owns_transaction = script_transaction_active == NULL;
    if (owns_transaction) {
        script_transaction_init(&local);
        script_transaction_active = &local;
    }
    const ecs_script_impl_t *impl = (const ecs_script_impl_t *)script;
    ctx.skip_unknown = (desc && desc->skip_unknown) || (impl && impl->skip_unknown);
    int status;
    if (!script) {
        ecs_parser_error(NULL, NULL, 0, "script must not be null");
        status = -1;
    } else if (impl->has_async && !ctx.instance) {
        ecs_parser_error(
            script->name,
            script->code,
            0,
            "suspending script requires a persistent instance"
        );
        status = -1;
    } else {
        ctx.ir = (desc && desc->ir) || impl->ir_enabled;
        status = script_eval_scope(&ctx, impl->root);
    }
    script_context_release_values(&ctx, 0);
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
    if (owns_transaction) {
        script_transaction_active = NULL;
        if (status >= 0 && script_transaction_commit(&local)) {
            ecs_parser_error(script ? script->name : NULL, NULL, 0, "transaction commit failed");
            status = -1;
        }
        script_transaction_fini(&local);
    }
    if (result) {
        result->error = siecs_log_capture_pop();
        siecs_log_get_captured_error_pos(&result->line, &result->column);
    }
    return status;
}

int ecs_script_run_w_desc(
    const char *name,
    const char *code,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
) {
    if (result)
        *result = (ecs_script_eval_result_t){ 0 };
    ecs_script_t *script = ecs_script_parse(name, code, desc, result);
    if (!script)
        return -1;
    int status = ecs_script_eval(script, desc, result);
    ecs_script_free(script);
    return status;
}

int ecs_script_run(const char *name, const char *code, ecs_script_eval_result_t *result) {
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
    if (!runtime)
        return;
    assert(!runtime->busy);
    sicore_vec_fini(&runtime->variables);
    sicore_vec_fini(&runtime->owned_values);
    script_allocator_fini(&runtime->values);
    free(runtime);
}
