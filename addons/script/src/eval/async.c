#include "async.h"
#include "../backend/transaction.h"
#include <math.h>

SCRIPT_THREAD_LOCAL ecs_script_instance_t *script_current_instance;

/* Prepared on the main thread. Workers use this immutable ownership tree and
 * never consult a registry that can grow while a schema is being compiled. */
typedef struct script_result_layout_t {
    struct script_result_layout_t *children, *next;
    size_t size, offset, count;
    sireflect_kind_t kind;
    bool string;
} script_result_layout_t;

static void result_layout_free(script_result_layout_t *layout) {
    while (layout) {
        script_result_layout_t *next = layout->next;
        result_layout_free(layout->children);
        free(layout);
        layout = next;
    }
}

static script_result_layout_t *result_layout_new(sireflect_handle_t type, unsigned depth) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info || depth > 256)
        return NULL;
    script_result_layout_t *layout = calloc(1, sizeof(*layout));
    if (!layout)
        return NULL;
    layout->size = info->size;
    layout->kind = info->kind;
    layout->count = info->element_count;
    layout->string = sireflect_type_is_cstring(type);
    size_t count = info->kind == sireflect_kind_struct ? info->fields.field_count
                   : info->kind == sireflect_kind_array || info->kind == sireflect_kind_vector ? 1
                                                                                               : 0;
    script_result_layout_t **tail = &layout->children;
    for (size_t i = 0; i < count; i++) {
        bool field = info->kind == sireflect_kind_struct;
        *tail =
            result_layout_new(field ? info->fields.fields[i].type : info->element_type, depth + 1);
        if (!*tail) {
            result_layout_free(layout);
            return NULL;
        }
        (*tail)->offset = field ? info->fields.fields[i].offset : 0;
        tail = &(*tail)->next;
    }
    return layout;
}

static void result_value_fini(const script_result_layout_t *layout, void *value) {
    if (layout->string)
        free(*(char **)value);
    else if (layout->kind == sireflect_kind_vector || layout->kind == sireflect_kind_array) {
        sireflect_vector_t *vector = value;
        void *data = layout->kind == sireflect_kind_vector ? vector->data : value;
        size_t count = layout->kind == sireflect_kind_vector ? vector->count : layout->count;
        for (size_t i = 0; i < count; i++)
            result_value_fini(layout->children, (char *)data + i * layout->children->size);
        if (layout->kind == sireflect_kind_vector)
            free(data);
    } else
        for (const script_result_layout_t *child = layout->children; child; child = child->next)
            result_value_fini(child, (char *)value + child->offset);
    memset(value, 0, layout->size);
}

/* Copy construction into zeroed, unpublished storage. Failure leaves a valid
 * partially constructed value that result_value_fini can always destroy. */
static bool result_value_copy(const script_result_layout_t *layout, void *dst, const void *src) {
    if (layout->string) {
        const char *text = *(char *const *)src;
        *(char **)dst = text ? script_strdup(text) : NULL;
        return !text || *(char **)dst;
    }
    if (layout->kind == sireflect_kind_vector || layout->kind == sireflect_kind_array) {
        size_t count = layout->count, stride = layout->children->size;
        if (layout->kind == sireflect_kind_vector) {
            const sireflect_vector_t *source = src;
            sireflect_vector_t *destination = dst;
            count = source->count;
            if (!stride || count > SIZE_MAX / stride || (count && !source->data))
                return false;
            destination->data = count ? calloc(count, stride) : NULL;
            if (count && !destination->data)
                return false;
            destination->count = count;
            dst = destination->data;
            src = source->data;
        }
        for (size_t i = 0; i < count; i++)
            if (!result_value_copy(
                    layout->children,
                    (char *)dst + i * stride,
                    (const char *)src + i * stride
                ))
                return false;
    } else if (layout->kind == sireflect_kind_struct) {
        for (const script_result_layout_t *child = layout->children; child; child = child->next)
            if (!result_value_copy(
                    child,
                    (char *)dst + child->offset,
                    (const char *)src + child->offset
                ))
                return false;
    } else
        memcpy(dst, src, layout->size);
    return true;
}

typedef struct script_host_function_t {
    struct script_host_function_t *next;
    ecs_script_function_desc_t desc;
    script_result_layout_t *result_layout;
} script_host_function_t;
typedef struct script_async_block_t script_async_block_t;
typedef struct script_future_t {
    ecs_script_task_t handle;
    ecs_script_instance_t *instance;
    script_host_function_t *function;
    script_async_block_t *owner;
    script_eval_t arguments_context;
    ecs_script_value_t *arguments;
    uint32_t argument_count;
    void *result;
    char *error;
    char *error_type;
    double delay;
    double deadline;
    bool timer;
    bool started;
    bool settled;
    bool initialized;
    uint32_t references;
    uint32_t heap_index;
    size_t result_size;
    sicore_vec_t waiters;
} script_future_t;
typedef struct script_task_frame_t {
    ecs_script_scope_t *scope;
    const ecs_script_for_t *loop;
    uint32_t cursor;
    uint32_t variable_count;
    uint32_t import_count, function_count, template_count;
    script_iteration_t iteration;
    ecs_entity_t entity;
    ecs_entity_t parent;
    bool restore_entity;
    bool prefab;
    bool with_scope;
    bool while_body;
    const ecs_script_try_t *attempt;
} script_task_frame_t;
typedef struct script_task_observer_t {
    script_async_block_t *block;
    ecs_observer_id_t id;
    ecs_entity_t entity;
    script_key_t component;
} script_task_observer_t;
struct script_async_block_t {
    script_async_block_t *next;
    script_async_block_t *parent_block;
    ecs_script_instance_t *instance;
    const ecs_script_async_t *node;
    const ecs_script_t *program;
    script_eval_t original;
    script_eval_t context;
    sicore_vec_t frames;
    sicore_vec_t observers;
    sicore_vec_t futures;
    sicore_vec_t programs;
    ecs_script_task_t waiting;
    uint64_t hash;
    bool queued;
    bool done;
    bool pending_names;
    bool matched;
    int reported_state;
};
static struct {
    script_host_function_t *functions;
    script_async_block_t *blocks;
    sicore_vec_t ready;
    sicore_vec_t futures;
    sicore_vec_t completed;
    sicore_vec_t timers;
    sicore_vec_t deferred;
    ecs_script_clock_t clock;
    void *clock_context;
    double clock_sample;
    bool initialized;
    bool flushing;
} async_state;
static atomic_flag future_lock = ATOMIC_FLAG_INIT;
static atomic_uint_fast64_t future_sequence;

static void lock_futures(void) {
    while (atomic_flag_test_and_set_explicit(&future_lock, memory_order_acquire)) {
    }
}
static void unlock_futures(void) { atomic_flag_clear_explicit(&future_lock, memory_order_release); }

static script_value_t copy_script_value(script_eval_t *destination, const script_value_t *source) {
    script_value_t copy = *source;
    if (source->collection && !source->range) {
        copy.items = script_alloc(&destination->values, source->count * sizeof(script_value_t));
        for (uint32_t i = 0; i < source->count; i++)
            copy.items[i] = copy_script_value(destination, &source->items[i]);
    } else if (source->type) {
        const sireflect_type_info_t *info = sireflect_type_info(source->type);
        if (!source->ops && ecs_reflected_ops(source->type).reflected_type) {
            copy = (script_value_t){ 0 };
            void *data = script_value_storage(destination, source->type, &copy);
            ecs_reflected_value_copy(source->type, data, script_value_data(source), false);
        } else if (source->ptr || source->ops) {
            void *data = script_alloc(&destination->values, info->size);
            if (source->ops) {
                if (source->ops->copy_ctor)
                    source->ops->copy_ctor(data, script_value_data(source), 1);
                else if (source->ops->reflected_type)
                    ecs_reflected_value_copy(
                        source->ops->reflected_type,
                        data,
                        script_value_data(source),
                        true
                    );
                else
                    memcpy(data, script_value_data(source), info->size);
                ecs_type_ops_t *saved_ops = script_alloc(&destination->values, sizeof(*saved_ops));
                *saved_ops = *source->ops;
                copy.ops = saved_ops;
                script_owned_value_t owned = { .ptr = data, .ops = *source->ops, .custom = true };
                sicore_vec_push(&destination->owned_values, &owned, sizeof owned);
            } else {
                ecs_component_t component = script_backend_component_lookup(info->name);
                if (component && script_backend_component_info(component)->type == source->type) {
                    script_backend_value_init(component, data, script_value_data(source));
                    script_owned_value_t owned = { .ptr = data,
                                                   .ops = *script_backend_value_ops(component),
                                                   .custom = true };
                    sicore_vec_push(&destination->owned_values, &owned, sizeof owned);
                } else
                    memcpy(data, script_value_data(source), info->size);
            }
            copy.ptr = data;
        }
    }
    copy.source_entity = source->source_entity;
    copy.source_component = source->source_component;
    copy.source_offset = source->source_offset;
    return copy;
}

static script_variable_t *
copy_variables(script_eval_t *destination, const script_variable_t *source, uint32_t count) {
    if (!count)
        return NULL;
    script_variable_t *variables = script_alloc(&destination->values, count * sizeof(*variables));
    for (uint32_t i = 0; i < count; i++) {
        variables[i] = source[i];
        variables[i].value = copy_script_value(destination, &source[i].value);
    }
    return variables;
}

static script_annotation_t *
copy_annotations(script_eval_t *ctx, const script_annotation_t *source) {
    script_annotation_t *head = NULL, **tail = &head;
    for (; source; source = source->next) {
        *tail = script_alloc(&ctx->values, sizeof(**tail));
        **tail = (script_annotation_t){ .name = siecs_strdup(&ctx->values, source->name),
                                        .value = siecs_strdup(&ctx->values, source->value) };
        tail = &(*tail)->next;
    }
    return head;
}

static bool same_annotations(const script_annotation_t *left, const script_annotation_t *right) {
    for (; left && right; left = left->next, right = right->next)
        if (strcmp(left->name, right->name) || strcmp(left->value, right->value))
            return false;
    return !left && !right;
}

static script_import_t *copy_imports(script_eval_t *ctx, const script_import_t *source) {
    script_import_t *head = NULL, **tail = &head;
    for (; source; source = source->next) {
        *tail = script_alloc(&ctx->values, sizeof(**tail));
        **tail = *source;
        (*tail)->next = NULL;
        tail = &(*tail)->next;
    }
    return head;
}

static bool same_imports(const script_import_t *left, const script_import_t *right) {
    for (; left && right; left = left->next, right = right->next)
        if (left->scope != right->scope)
            return false;
    return !left && !right;
}

static void remap_imports(script_transaction_t *transaction, script_import_t *imports) {
    for (; imports; imports = imports->next)
        imports->scope = script_transaction_committed_entity(transaction, imports->scope);
}

typedef struct script_function_copy_t {
    const script_function_t *source;
    script_function_t *copy;
} script_function_copy_t;

static script_function_t *
copy_functions(script_eval_t *destination, const script_function_t *source, sicore_vec_t *copies) {
    script_function_t *head = NULL, **tail = &head;
    for (const script_function_t *item = source; item; item = item->next) {
        const script_function_copy_t *existing = copies->data;
        for (uint32_t i = 0; i < copies->size; i++)
            if (existing[i].source == item) {
                *tail = existing[i].copy;
                return head;
            }
        script_function_t *copy = script_alloc(&destination->values, sizeof(*copy));
        *copy = *item;
        copy->captures = copy_variables(destination, item->captures, item->capture_count);
        copy->imports = copy_imports(destination, item->imports);
        copy->next = NULL;
        script_function_copy_t pair = { item, copy };
        sicore_vec_push(copies, &pair, sizeof pair);
        *tail = copy;
        tail = &copy->next;
    }
    return head;
}

script_eval_t *script_context_clone(const script_eval_t *source) {
    if (!source)
        return NULL;
    script_eval_t *copy = malloc(sizeof(*copy));
    assert(copy != NULL);
    script_context_copy(copy, source);
    return copy;
}

void script_context_copy(script_eval_t *destination, const script_eval_t *source) {
    *destination = *source;
    destination->values = (ecs_allocator_t){ 0 };
    destination->annotations = copy_annotations(destination, source->annotations);
    sicore_vec_init(&destination->variables, sizeof(script_variable_t));
    sicore_vec_init(&destination->owned_values, sizeof(script_owned_value_t));
    script_variable_t *variables =
        copy_variables(destination, source->variables.data, source->variables.size);
    for (uint32_t i = 0; i < source->variables.size; i++)
        sicore_vec_push(&destination->variables, &variables[i], sizeof(*variables));
    destination->captures = copy_variables(destination, source->captures, source->capture_count);
    destination->imports = copy_imports(destination, source->imports);
    sicore_vec_t function_copies;
    sicore_vec_init(&function_copies, sizeof(script_function_copy_t));
    destination->functions = copy_functions(destination, source->functions, &function_copies);
    destination->templates = NULL;
    script_template_t **template_tail = &destination->templates;
    for (const script_template_t *item = source->templates; item; item = item->next) {
        script_template_t *copy = script_alloc(&destination->values, sizeof(*copy));
        *copy = *item;
        copy->captures = copy_variables(destination, item->captures, item->capture_count);
        copy->imports = copy_imports(destination, item->imports);
        copy->functions = copy_functions(destination, item->functions, &function_copies);
        copy->next = NULL;
        *template_tail = copy;
        template_tail = &copy->next;
    }
    const script_template_t *original = source->templates;
    for (script_template_t *copy = destination->templates; copy; copy = copy->next) {
        if (original->base) {
            const script_template_t *base = source->templates;
            copy->base = destination->templates;
            while (base != original->base) {
                base = base->next;
                copy->base = copy->base->next;
            }
        }
        original = original->next;
    }
    destination->with = NULL;
    /* Scoped defaults are materialized when entering entities. Async blocks
     * capture their lexical values; the program owns expression scopes. */
    script_with_frame_t **with_tail = &destination->with;
    for (const script_with_frame_t *item = source->with; item; item = item->previous) {
        script_with_frame_t *copy = script_alloc(&destination->values, sizeof(*copy));
        *copy = *item;
        copy->captures = copy_variables(destination, item->captures, item->capture_count);
        copy->imports = copy_imports(destination, item->imports);
        copy->functions = copy_functions(destination, item->functions, &function_copies);
        if (item->pair)
            copy->expressions = script_pair_expressions(destination, item->pair);
        copy->previous = NULL;
        *with_tail = copy;
        with_tail = &copy->previous;
    }
    sicore_vec_fini(&function_copies);
}

typedef bool (*script_value_visitor_t)(const script_value_t *value, void *context);

static bool visit_capture_values(
    const script_variable_t *values,
    uint32_t count,
    script_value_visitor_t visit,
    void *context
) {
    for (uint32_t i = 0; i < count; i++)
        if (visit(&values[i].value, context))
            return true;
    return false;
}

static bool visit_function_values(
    const script_function_t *functions,
    script_value_visitor_t visit,
    void *context
) {
    for (; functions; functions = functions->next)
        if (visit_capture_values(functions->captures, functions->capture_count, visit, context))
            return true;
    return false;
}

static bool
visit_context_values(const script_eval_t *ctx, script_value_visitor_t visit, void *context) {
    if (visit_capture_values(ctx->variables.data, ctx->variables.size, visit, context) ||
        visit_capture_values(ctx->captures, ctx->capture_count, visit, context) ||
        visit_function_values(ctx->functions, visit, context))
        return true;
    for (const script_template_t *item = ctx->templates; item; item = item->next)
        if (visit_capture_values(item->captures, item->capture_count, visit, context) ||
            visit_function_values(item->functions, visit, context))
            return true;
    for (const script_with_frame_t *item = ctx->with; item; item = item->previous)
        if (visit_capture_values(item->captures, item->capture_count, visit, context) ||
            visit_function_values(item->functions, visit, context))
            return true;
    return false;
}

void script_context_release_values(script_eval_t *ctx, uint32_t keep) {
    const script_owned_value_t *owned = ctx->owned_values.data;
    for (uint32_t i = ctx->owned_values.size; i > keep; i--) {
        const script_owned_value_t *value = &owned[i - 1];
        if (value->custom)
            script_custom_value_fini(&value->ops, value->ptr);
        else
            script_backend_value_fini(value->component, value->ptr);
    }
    ctx->owned_values.size = keep;
}

void script_context_fini(script_eval_t *ctx) {
    script_context_release_values(ctx, 0);
    sicore_vec_fini(&ctx->owned_values);
    sicore_vec_fini(&ctx->variables);
    script_allocator_fini(&ctx->values);
    *ctx = (script_eval_t){ 0 };
}

void script_context_restore(script_eval_t *ctx, script_eval_t *snapshot, uint32_t owned_count) {
    uint32_t budget = ctx->instruction_budget;
    script_context_release_values(ctx, owned_count);
    script_allocator_adopt(&ctx->values, &snapshot->values);
    script_allocator_retire_vector(&ctx->values, ctx->variables);
    script_owned_value_t *restored = snapshot->owned_values.data;
    for (uint32_t i = 0; i < snapshot->owned_values.size; i++)
        sicore_vec_push(&ctx->owned_values, &restored[i], sizeof(*restored));
    sicore_vec_fini(&snapshot->owned_values);
    snapshot->values = ctx->values;
    snapshot->owned_values = ctx->owned_values;
    *ctx = *snapshot;
    ctx->instruction_budget = budget;
    *snapshot = (script_eval_t){ 0 };
}

static void async_init(void) {
    if (async_state.initialized)
        return;
    async_state.initialized = true;
    async_state.clock_sample = script_monotonic_time();
    sicore_vec_init(&async_state.ready, sizeof(script_async_block_t *));
    sicore_vec_init(&async_state.deferred, sizeof(script_async_block_t *));
    sicore_vec_init(&async_state.timers, sizeof(script_future_t *));
    sicore_vec_init(&async_state.completed, sizeof(ecs_script_task_t));
    lock_futures();
    sicore_vec_init(&async_state.futures, sizeof(script_future_t *));
    unlock_futures();
    script_instance_runtime_init();
}

static double async_now(void) {
    double sample =
        async_state.clock ? async_state.clock(async_state.clock_context) : script_monotonic_time();
    if (isfinite(sample) && sample > async_state.clock_sample)
        async_state.clock_sample = sample;
    return async_state.clock_sample;
}

int ecs_script_clock_set(ecs_script_clock_t clock, void *context) {
    if (async_state.flushing)
        return -1;
    double sample = clock ? clock(context) : script_monotonic_time();
    if (!isfinite(sample))
        return -1;
    async_init();
    double previous = async_now();
    script_future_t **timers = async_state.timers.data;
    for (uint32_t i = 0; i < async_state.timers.size; i++) {
        double remaining = fmax(0, timers[i]->deadline - previous);
        timers[i]->deadline = sample + remaining;
    }
    async_state.clock = clock;
    async_state.clock_context = context;
    async_state.clock_sample = sample;
    return 0;
}

static script_future_t *find_future(ecs_script_task_t handle) {
    uint32_t slot = (uint32_t)handle - 1;
    if (slot >= async_state.futures.size)
        return NULL;
    script_future_t *future = ((script_future_t **)async_state.futures.data)[slot];
    return future && future->handle == handle ? future : NULL;
}

static void timer_swap(uint32_t a, uint32_t b) {
    script_future_t **timers = async_state.timers.data;
    script_future_t *temporary = timers[a];
    timers[a] = timers[b];
    timers[b] = temporary;
    timers[a]->heap_index = a;
    timers[b]->heap_index = b;
}

static void timer_up(uint32_t at) {
    script_future_t **timers = async_state.timers.data;
    while (at && timers[at]->deadline < timers[(at - 1) / 2]->deadline) {
        uint32_t parent = (at - 1) / 2;
        timer_swap(at, parent);
        at = parent;
    }
}

static void timer_remove(script_future_t *future) {
    uint32_t at = future->heap_index;
    if (at == UINT32_MAX)
        return;
    script_future_t **timers = async_state.timers.data;
    uint32_t last = --async_state.timers.size;
    future->heap_index = UINT32_MAX;
    if (at == last)
        return;
    timers[at] = timers[last];
    timers[at]->heap_index = at;
    if (at && timers[at]->deadline < timers[(at - 1) / 2]->deadline) {
        timer_up(at);
        return;
    }
    while (at * 2 + 1 < async_state.timers.size) {
        uint32_t child = at * 2 + 1;
        if (child + 1 < async_state.timers.size &&
            timers[child + 1]->deadline < timers[child]->deadline)
            child++;
        if (timers[at]->deadline <= timers[child]->deadline)
            break;
        timer_swap(at, child);
        at = child;
    }
}

static script_future_t *new_future(
    script_eval_t *ctx,
    script_host_function_t *function,
    const ecs_expr_initializer_t *arguments
) {
    script_future_t *future = calloc(1, sizeof(*future));
    if (!future)
        return NULL;
    future->references = 1;
    future->heap_index = UINT32_MAX;
    future->instance = ctx->instance;
    future->owner = ctx->async_block;
    future->function = function;
    sicore_vec_init(&future->waiters, sizeof(script_async_block_t *));
    sicore_vec_init(&future->arguments_context.variables, sizeof(script_variable_t));
    sicore_vec_init(&future->arguments_context.owned_values, sizeof(script_owned_value_t));
    if (function) {
        future->argument_count = arguments->elements.size;
        future->arguments = script_alloc(
            &future->arguments_context.values,
            future->argument_count * sizeof(ecs_script_value_t)
        );
        const ecs_expr_initializer_element_t *elements = arguments->elements.data;
        for (uint32_t i = 0; i < future->argument_count; i++) {
            sireflect_handle_t type = function->desc.parameters[i];
            script_value_t argument = { 0 };
            void *value = script_value_storage(ctx, type, &argument);
            if (script_value_write(ctx, elements[i].value, type, value))
                goto error;
            script_value_t copy = copy_script_value(&future->arguments_context, &argument);
            future->arguments[i] = (ecs_script_value_t){ type, script_value_data(&copy) };
            /* Inline scalars need stable storage after copy leaves this frame. */
            if (!copy.ptr) {
                void *stable =
                    script_alloc(&future->arguments_context.values, sireflect_type_size(type));
                memcpy(stable, &copy.storage, sireflect_type_size(type));
                future->arguments[i].data = stable;
            }
        }
        future->result_size = sireflect_type_size(function->desc.result_type);
        future->result = calloc(1, future->result_size);
        if (!future->result)
            goto error;
        if (function->desc.result_ops.ctor)
            function->desc.result_ops.ctor(future->result, 1);
        future->initialized = true;
    }
    lock_futures();
    script_future_t **futures = async_state.futures.data;
    uint32_t index = 0;
    while (index < async_state.futures.size && futures[index])
        index++;
    uint64_t generation = atomic_fetch_add_explicit(&future_sequence, 1, memory_order_relaxed) + 1;
    if (generation > UINT32_MAX)
        abort();
    future->handle = (generation << 32) | (index + 1);
    if (index < async_state.futures.size)
        futures[index] = future;
    else
        sicore_vec_push(&async_state.futures, &future, sizeof future);
    /* Every live task can settle once. Reserve on the main thread so worker
     * completion and failure cannot allocate while publishing notifications. */
    if (async_state.completed.size > UINT32_MAX - async_state.futures.size)
        abort();
    uint32_t completion_capacity = async_state.completed.size + async_state.futures.size;
    while (async_state.completed.capacity < completion_capacity)
        sicore_vec_grow(&async_state.completed, sizeof(ecs_script_task_t));
    unlock_futures();
    if (future->owner)
        sicore_vec_push(&future->owner->futures, &future->handle, sizeof future->handle);
    sicore_vec_push(
        &script_transaction_active->async_launches,
        &future->handle,
        sizeof future->handle
    );
    return future;
error:
    script_context_fini(&future->arguments_context);
    sicore_vec_fini(&future->waiters);
    free(future->result);
    free(future);
    return NULL;
}

bool ecs_script_task_complete(ecs_script_task_t task, const void *value) {
    lock_futures();
    script_future_t *future = find_future(task);
    if (!future || !future->started || future->settled || !future->function || !value) {
        unlock_futures();
        return false;
    }
    const ecs_script_function_desc_t *desc = &future->function->desc;
    if (desc->result_ops.copy)
        desc->result_ops.copy(future->result, value, 1);
    else if (desc->result_ops.copy_ctor) {
        if (desc->result_ops.dtor)
            desc->result_ops.dtor(future->result, 1);
        desc->result_ops.copy_ctor(future->result, value, 1);
    } else if (future->function->result_layout) {
        const script_result_layout_t *layout = future->function->result_layout;
        if (!result_value_copy(layout, future->result, value)) {
            result_value_fini(layout, future->result);
            unlock_futures();
            return false;
        }
    } else
        memcpy(future->result, value, future->result_size);
    future->settled = true;
    sicore_vec_push(&async_state.completed, &future->handle, sizeof future->handle);
    unlock_futures();
    return true;
}

bool ecs_script_task_fail(ecs_script_task_t task, const char *error_type, const char *message) {
    lock_futures();
    script_future_t *future = find_future(task);
    if (!future || !future->started || future->settled) {
        unlock_futures();
        return false;
    }
    char *type = script_strdup(error_type ? error_type : "TaskError");
    char *error = script_strdup(message ? message : "asynchronous operation failed");
    if (!type || !error) {
        free(type);
        free(error);
        unlock_futures();
        return false;
    }
    future->error_type = type;
    future->error = error;
    future->settled = true;
    sicore_vec_push(&async_state.completed, &future->handle, sizeof future->handle);
    unlock_futures();
    return true;
}

int ecs_script_function_register(const ecs_script_function_desc_t *desc) {
    if (!desc || !desc->name || !desc->result_type || desc->parameter_count > 16 ||
        (desc->parameter_count && !desc->parameters) || (!!desc->callback == !!desc->start))
        return -1;
    async_init();
    for (script_host_function_t *item = async_state.functions; item; item = item->next)
        if (!strcmp(item->desc.name, desc->name))
            return -1;
    script_host_function_t *function = calloc(1, sizeof(*function));
    if (!function)
        return -1;
    function->desc = *desc;
    if (!desc->result_ops.ctor && !desc->result_ops.dtor && !desc->result_ops.copy_ctor &&
        !desc->result_ops.copy && !desc->result_ops.move_ctor && !desc->result_ops.move)
        function->desc.result_ops = ecs_reflected_ops(desc->result_type);
    function->desc.name = script_strdup(desc->name);
    sireflect_handle_t *parameters = malloc(desc->parameter_count * sizeof(*parameters));
    if (desc->parameter_count && !parameters) {
        free((char *)function->desc.name);
        free(function);
        return -1;
    }
    if (desc->parameter_count)
        memcpy(parameters, desc->parameters, desc->parameter_count * sizeof(*parameters));
    function->desc.parameters = parameters;
    if (function->desc.result_ops.reflected_type) {
        function->result_layout = result_layout_new(function->desc.result_ops.reflected_type, 0);
        if (!function->result_layout) {
            free((char *)function->desc.name);
            free((void *)function->desc.parameters);
            free(function);
            return -1;
        }
    }
    function->next = async_state.functions;
    async_state.functions = function;
    return 0;
}

int script_async_host_call(
    script_eval_t *ctx,
    const ecs_expr_function_t *call,
    script_value_t *out
) {
    if (!strcmp(call->function_name, "sleep")) {
        if (!ctx->instance || !ctx->async_block || !script_transaction_active ||
            call->args->elements.size != 1)
            return script_eval_error(
                ctx,
                call->node.pos,
                "sleep requires a persistent async instance and one duration"
            );
        const ecs_expr_initializer_element_t *argument = call->args->elements.data;
        double duration = 0;
        if (script_value_write(
                ctx,
                argument->value,
                script_backend_type_lookup("double"),
                &duration
            ))
            return -1;
        if (!isfinite(duration) || duration < 0)
            return script_eval_error(ctx, call->node.pos, "invalid sleep duration");
        async_init();
        script_future_t *future = new_future(ctx, NULL, call->args);
        if (!future)
            return script_eval_error(ctx, call->node.pos, "out of memory");
        future->timer = true;
        future->delay = duration;
        out->future = future->handle;
        return 0;
    }
    script_host_function_t *function = async_state.functions;
    while (function && strcmp(function->desc.name, call->function_name))
        function = function->next;
    if (!function)
        return script_eval_error(ctx, call->node.pos, "unknown function '%s'", call->function_name);
    if (call->args->elements.size != function->desc.parameter_count)
        return script_eval_error(ctx, call->node.pos, "host function argument count mismatch");
    if (function->desc.start) {
        if (!ctx->instance || !script_transaction_active)
            return script_eval_error(
                ctx,
                call->node.pos,
                "asynchronous host call requires a persistent instance"
            );
        script_future_t *future = new_future(ctx, function, call->args);
        if (!future)
            return script_eval_error(ctx, call->node.pos, "cannot prepare asynchronous arguments");
        out->future = future->handle;
        return 0;
    }
    ecs_script_value_t arguments[16];
    script_value_t input_values[16] = { 0 };
    const ecs_expr_initializer_element_t *elements = call->args->elements.data;
    for (uint32_t i = 0; i < call->args->elements.size; i++) {
        sireflect_handle_t type = function->desc.parameters[i];
        void *data = script_value_storage(ctx, type, &input_values[i]);
        if (script_value_write(ctx, elements[i].value, type, data))
            return -1;
        arguments[i] = (ecs_script_value_t){ type, data };
    }
    out->type = function->desc.result_type;
    out->ops = &function->desc.result_ops;
    void *value = script_alloc(&ctx->values, sireflect_type_size(out->type));
    if (out->ops->ctor)
        out->ops->ctor(value, 1);
    script_owned_value_t owned = { .ptr = value, .ops = *out->ops, .custom = true };
    sicore_vec_push(&ctx->owned_values, &owned, sizeof owned);
    out->ptr = value;
    const char *error = NULL;
    if (function->desc.callback(
            arguments,
            function->desc.parameter_count,
            value,
            function->desc.context,
            &error
        ))
        return script_eval_error(ctx, call->node.pos, "%s", error ? error : "host function failed");
    return 0;
}

static void queue_block(script_async_block_t *block) {
    if (block->done || block->queued)
        return;
    block->queued = true;
    sicore_vec_push(&async_state.ready, &block, sizeof block);
}

static void task_changed(ecs_observer_event_t *event) {
    script_task_observer_t *observer = (void *)event->user_data;
    if (script_key_event(observer->component, event))
        queue_block(observer->block);
}
static void task_observer_removed(uintptr_t data) {
    ((script_task_observer_t *)data)->id = UINT32_MAX;
}
static void clear_task_observers(script_async_block_t *block) {
    script_task_observer_t **observers = block->observers.data;
    for (uint32_t i = 0; i < block->observers.size; i++) {
        if (observers[i]->id != UINT32_MAX)
            ecs_observer_fini(observers[i]->id);
        free(observers[i]);
    }
    block->observers.size = 0;
}

static void observe_task(script_async_block_t *block, const script_transaction_t *transaction) {
    clear_task_observers(block);
    const script_read_t *reads = transaction->reads.data;
    for (uint32_t i = 0; i < transaction->reads.size; i++) {
        if (!ecs_is_alive(reads[i].entity))
            continue;
        for (uint32_t j = 0; j < (reads[i].component ? 3u : 2u); j++) {
            script_task_observer_t *observer = calloc(1, sizeof(*observer));
            if (!observer)
                abort();
            *observer = (script_task_observer_t){ .block = block,
                                                  .entity = reads[i].entity,
                                                  .component = reads[i].component };
            observer->id = script_key_observe(
                observer->entity,
                observer->component,
                j,
                task_changed,
                (uintptr_t)observer,
                task_observer_removed
            );
            sicore_vec_push(&block->observers, &observer, sizeof observer);
        }
    }
}

static void push_frame(sicore_vec_t *frames, ecs_script_scope_t *scope, const script_eval_t *ctx) {
    script_task_frame_t frame = { .scope = scope, .variable_count = ctx->variables.size };
    for (const script_import_t *import = ctx->imports; import; import = import->next)
        frame.import_count++;
    for (const script_function_t *function = ctx->functions; function; function = function->next)
        frame.function_count++;
    for (const script_template_t *template = ctx->templates; template; template = template->next)
        frame.template_count++;
    sicore_vec_push(frames, &frame, sizeof frame);
}

static void restore_frame_scope(script_eval_t *ctx, const script_task_frame_t *frame) {
    if (!frame->scope)
        return;
    uint32_t count = 0;
    for (const script_import_t *import = ctx->imports; import; import = import->next)
        count++;
    while (count-- > frame->import_count)
        ctx->imports = ctx->imports->next;
    count = 0;
    for (const script_function_t *function = ctx->functions; function; function = function->next)
        count++;
    while (count-- > frame->function_count)
        ctx->functions = ctx->functions->next;
    count = 0;
    for (const script_template_t *template = ctx->templates; template; template = template->next)
        count++;
    while (count-- > frame->template_count)
        ctx->templates = ctx->templates->next;
}

static bool same_value(const script_value_t *left, const script_value_t *right) {
    if (left->type != right->type || left->item_type != right->item_type ||
        left->future != right->future || left->collection != right->collection ||
        left->count != right->count || left->source_entity != right->source_entity ||
        left->source_component != right->source_component ||
        left->source_offset != right->source_offset)
        return false;
    if (left->collection) {
        if (left->range && right->range)
            return left->range_start == right->range_start;
        for (uint32_t i = 0; i < left->count; i++) {
            script_value_t a = script_collection_at(left, i);
            script_value_t b = script_collection_at(right, i);
            if (!same_value(&a, &b))
                return false;
        }
        return true;
    }
    return !left->type ||
           ecs_reflected_value_equal(left->type, script_value_data(left), script_value_data(right));
}

static bool same_capture_list(
    const script_variable_t *left,
    uint32_t left_count,
    const script_variable_t *right,
    uint32_t right_count
) {
    if (left_count != right_count)
        return false;
    for (uint32_t i = 0; i < left_count; i++)
        if (strcmp(left[i].name, right[i].name) || left[i].mutable != right[i].mutable ||
            !same_value(&left[i].value, &right[i].value))
            return false;
    return true;
}

static bool same_source(const ecs_script_node_t *left, const ecs_script_node_t *right) {
    return left == right ||
           (left->kind == right->kind && left->pos && left->end && right->pos && right->end &&
            script_source_hash(left->pos, left->end) == script_source_hash(right->pos, right->end));
}

static bool same_expression(const ecs_expr_node_t *left, const ecs_expr_node_t *right) {
    return left == right ||
           (left && right && left->kind == right->kind && left->pos && left->end && right->pos &&
            right->end &&
            script_source_hash(left->pos, left->end) == script_source_hash(right->pos, right->end));
}

static bool same_name(const char *left, const char *right) {
    return left == right || (left && right && !strcmp(left, right));
}

static bool same_functions(const script_function_t *left, const script_function_t *right) {
    for (; left && right; left = left->next, right = right->next)
        if (!same_imports(left->imports, right->imports) ||
            !same_source(&left->node->node, &right->node->node) ||
            !same_capture_list(
                left->captures,
                left->capture_count,
                right->captures,
                right->capture_count
            ))
            return false;
    return !left && !right;
}

static bool same_defaults(const ecs_script_scope_t *left, const ecs_script_scope_t *right) {
    if (left->stmts.size != right->stmts.size)
        return false;
    const ecs_script_node_t *const *a = left->stmts.data, *const *b = right->stmts.data;
    for (uint32_t i = 0; i < left->stmts.size; i++) {
        if (a[i]->kind != b[i]->kind || (a[i]->kind != EcsAstComponent && a[i]->kind != EcsAstTag))
            return false;
        const ecs_script_tag_t *first = (const ecs_script_tag_t *)a[i];
        const ecs_script_tag_t *second = (const ecs_script_tag_t *)b[i];
        if (!same_name(first->id.first, second->id.first) ||
            !same_name(first->id.second, second->id.second) ||
            first->id.auto_override != second->id.auto_override)
            return false;
        if (a[i]->kind == EcsAstComponent) {
            const ecs_script_component_t *x = (const ecs_script_component_t *)a[i];
            const ecs_script_component_t *y = (const ecs_script_component_t *)b[i];
            if (x->is_partial != y->is_partial || !same_expression(x->expr, y->expr))
                return false;
        }
    }
    return true;
}

bool script_context_same_captures(const script_eval_t *left, const script_eval_t *right) {
    if (!same_annotations(left->annotations, right->annotations) ||
        !same_imports(left->imports, right->imports) || left->entity != right->entity ||
        left->parent != right->parent || left->variable_base != right->variable_base ||
        left->loop_depth != right->loop_depth || left->call_depth != right->call_depth ||
        left->template_depth != right->template_depth ||
        !same_capture_list(
            left->variables.data,
            left->variables.size,
            right->variables.data,
            right->variables.size
        ) ||
        !same_capture_list(
            left->captures,
            left->capture_count,
            right->captures,
            right->capture_count
        ))
        return false;
    const script_template_t *a = left->templates, *b = right->templates;
    for (; a && b; a = a->next, b = b->next)
        if (!same_imports(a->imports, b->imports) || a->component != b->component ||
            !same_source(&a->node->node, &b->node->node) ||
            !same_functions(a->functions, b->functions) ||
            !same_capture_list(a->captures, a->capture_count, b->captures, b->capture_count))
            return false;
    if (a || b)
        return false;
    if (!same_functions(left->functions, right->functions))
        return false;
    const script_with_frame_t *first = left->with, *second = right->with;
    for (; first && second; first = first->previous, second = second->previous)
        if (!same_imports(first->imports, second->imports) ||
            !same_defaults(first->expressions, second->expressions) ||
            !same_functions(first->functions, second->functions) ||
            !same_capture_list(
                first->captures,
                first->capture_count,
                second->captures,
                second->capture_count
            ))
            return false;
    return !first && !second;
}

static void retain_block_program(script_async_block_t *block, const ecs_script_t *program) {
    if (!program || program == block->program)
        return;
    const ecs_script_t *const *programs = block->programs.data;
    for (uint32_t i = 0; i < block->programs.size; i++)
        if (programs[i] == program)
            return;
    program = ecs_script_retain(program);
    sicore_vec_push(&block->programs, &program, sizeof program);
}

static void
retain_function_programs(script_async_block_t *block, const script_function_t *functions) {
    for (; functions; functions = functions->next)
        retain_block_program(block, functions->program);
}

static void capture_future_refs(script_async_block_t *block, const script_value_t *value) {
    if (value->future) {
        const ecs_script_task_t *held = block->futures.data;
        for (uint32_t i = 0; i < block->futures.size; i++)
            if (held[i] == value->future)
                return;
        lock_futures();
        script_future_t *future = find_future(value->future);
        if (future)
            future->references++;
        unlock_futures();
        if (future)
            sicore_vec_push(&block->futures, &value->future, sizeof(value->future));
    }
    for (uint32_t i = 0; value->collection && !value->range && i < value->count; i++)
        capture_future_refs(block, &value->items[i]);
}

static bool retain_captured_future(const script_value_t *value, void *context) {
    capture_future_refs(context, value);
    return false;
}

int script_async_spawn(script_eval_t *ctx, const ecs_script_async_t *node) {
    if (!ctx->instance || !script_transaction_active)
        return script_eval_error(
            ctx,
            node->node.pos,
            "async requires a persistent script instance"
        );
    async_init();
    script_async_block_t *block = calloc(1, sizeof(*block));
    if (!block)
        return script_eval_error(ctx, node->node.pos, "out of memory");
    block->instance = ctx->instance;
    block->parent_block = ctx->async_block;
    block->node = node;
    block->program = ecs_script_retain(ctx->script);
    sicore_vec_init(&block->programs, sizeof(ecs_script_t *));
    block->hash = script_source_hash(node->node.pos, node->node.end);
    for (const script_function_t *function = ctx->functions; function; function = function->next) {
        block->hash ^= script_source_hash(function->node->node.pos, function->node->node.end);
        retain_block_program(block, function->program);
    }
    for (const script_template_t *template = ctx->templates; template; template = template->next) {
        block->hash ^= script_source_hash(template->node->node.pos, template->node->node.end);
        retain_block_program(block, template->program);
        retain_function_programs(block, template->functions);
    }
    for (const script_with_frame_t *with = ctx->with; with; with = with->previous) {
        retain_block_program(block, with->program);
        retain_function_programs(block, with->functions);
    }
    script_context_copy(&block->original, ctx);
    script_context_copy(&block->context, ctx);
    block->context.async_block = block;
    sicore_vec_init(&block->frames, sizeof(script_task_frame_t));
    sicore_vec_init(&block->observers, sizeof(script_task_observer_t *));
    sicore_vec_init(&block->futures, sizeof(ecs_script_task_t));
    visit_context_values(ctx, retain_captured_future, block);
    push_frame(&block->frames, node->scope, &block->context);
    sicore_vec_push(&script_transaction_active->async_blocks, &block, sizeof block);
    return 0;
}

static void forget_future(script_future_t *future) {
    lock_futures();
    script_future_t **futures = async_state.futures.data;
    futures[(uint32_t)future->handle - 1] = NULL;
    timer_remove(future);
    bool cancel = future->started && !future->settled;
    unlock_futures();
    if (cancel && future->function && future->function->desc.cancel)
        future->function->desc.cancel(future->handle, future->function->desc.context);
    if (future->initialized)
        script_custom_value_fini(&future->function->desc.result_ops, future->result);
    script_context_fini(&future->arguments_context);
    sicore_vec_fini(&future->waiters);
    free(future->result);
    free(future->error);
    free(future->error_type);
    free(future);
}

static void release_future(ecs_script_task_t handle) {
    lock_futures();
    script_future_t *future = find_future(handle);
    bool destroy = future && --future->references == 0;
    unlock_futures();
    if (destroy)
        forget_future(future);
}

static bool value_holds_future(const script_value_t *value, ecs_script_task_t future) {
    if (value->future == future)
        return true;
    for (uint32_t i = 0; value->collection && !value->range && i < value->count; i++)
        if (value_holds_future(&value->items[i], future))
            return true;
    return false;
}

static bool holds_captured_future(const script_value_t *value, void *context) {
    return value_holds_future(value, *(const ecs_script_task_t *)context);
}

static bool context_holds_future(const script_eval_t *ctx, ecs_script_task_t future) {
    return visit_context_values(ctx, holds_captured_future, &future);
}

static void collect_block_futures(script_async_block_t *block) {
    ecs_script_task_t *futures = block->futures.data;
    uint32_t index = 0;
    while (index < block->futures.size) {
        ecs_script_task_t handle = futures[index];
        if (!block->done &&
            (block->waiting == handle || context_holds_future(&block->context, handle))) {
            index++;
            continue;
        }
        futures[index] = futures[--block->futures.size];
        release_future(handle);
    }
}

static void destroy_block(script_async_block_t *block) {
    if (block->reported_state)
        script_instance_async_state(block->instance, block->reported_state, 0, NULL, 0, 0);
    clear_task_observers(block);
    script_async_block_t **ready = async_state.ready.data;
    for (uint32_t i = 0; i < async_state.ready.size; i++)
        if (ready[i] == block)
            ready[i] = NULL;
    lock_futures();
    script_future_t **futures = async_state.futures.data;
    for (uint32_t i = 0; i < async_state.futures.size; i++) {
        if (!futures[i])
            continue;
        script_async_block_t **waiters = futures[i]->waiters.data;
        for (uint32_t j = 0; j < futures[i]->waiters.size; j++)
            if (waiters[j] == block)
                waiters[j] = NULL;
    }
    unlock_futures();
    const ecs_script_task_t *owned_futures = block->futures.data;
    for (uint32_t i = 0; i < block->futures.size; i++)
        release_future(owned_futures[i]);
    script_context_fini(&block->original);
    script_context_fini(&block->context);
    ecs_script_free((ecs_script_t *)block->program);
    const ecs_script_t *const *programs = block->programs.data;
    for (uint32_t i = 0; i < block->programs.size; i++)
        ecs_script_free((ecs_script_t *)programs[i]);
    sicore_vec_fini(&block->programs);
    sicore_vec_fini(&block->frames);
    sicore_vec_fini(&block->observers);
    sicore_vec_fini(&block->futures);
    script_async_block_t **deferred = async_state.deferred.data;
    for (uint32_t i = 0; i < async_state.deferred.size; i++)
        if (deferred[i] == block)
            deferred[i] = NULL;
    free(block);
}

static void remap_script_value(script_transaction_t *transaction, script_value_t *value) {
    value->source_entity = script_transaction_committed_entity(transaction, value->source_entity);
    value->source_component =
        script_transaction_committed_component(transaction, value->source_component);
    if (value->collection && !value->range) {
        for (uint32_t i = 0; i < value->count; i++)
            remap_script_value(transaction, &value->items[i]);
    } else if (value->type) {
        script_transaction_remap_value(
            transaction,
            value->type,
            value->ptr ? (void *)value->ptr : &value->storage
        );
    }
}

static bool remap_captured_value(const script_value_t *value, void *context) {
    remap_script_value(context, (script_value_t *)value);
    return false;
}

void script_context_remap(script_transaction_t *transaction, script_eval_t *ctx) {
    ctx->entity = script_transaction_committed_entity(transaction, ctx->entity);
    ctx->parent = script_transaction_committed_entity(transaction, ctx->parent);
    visit_context_values(ctx, remap_captured_value, transaction);
    remap_imports(transaction, ctx->imports);
    for (script_function_t *function = ctx->functions; function; function = function->next)
        remap_imports(transaction, function->imports);
    for (script_template_t *template = ctx->templates; template; template = template->next) {
        template->component =
            script_transaction_committed_component(transaction, template->component);
        remap_imports(transaction, template->imports);
        for (script_function_t *function = template->functions; function; function = function->next)
            remap_imports(transaction, function->imports);
    }
    for (script_with_frame_t *frame = ctx->with; frame; frame = frame->previous) {
        remap_imports(transaction, frame->imports);
        for (script_function_t *function = frame->functions; function; function = function->next)
            remap_imports(transaction, function->imports);
    }
    script_owned_value_t *owned = ctx->owned_values.data;
    for (uint32_t i = 0; i < ctx->owned_values.size; i++)
        owned[i].component =
            script_transaction_committed_component(transaction, owned[i].component);
}

static void retain_descendant_blocks(script_async_block_t *parent) {
    for (script_async_block_t *child = async_state.blocks; child; child = child->next) {
        if (child->instance == parent->instance && child->parent_block == parent) {
            child->matched = true;
            retain_descendant_blocks(child);
        }
    }
}

void script_async_transaction_commit(script_transaction_t *transaction) {
    if (transaction->reconcile_async) {
        for (script_async_block_t *block = async_state.blocks; block; block = block->next)
            if (block->instance == transaction->instance)
                block->matched = false;
    }
    script_async_block_t **blocks = transaction->async_blocks.data;
    for (uint32_t i = 0; i < transaction->async_blocks.size; i++) {
        script_async_block_t *block = blocks[i];
        script_context_remap(transaction, &block->original);
        script_context_remap(transaction, &block->context);
        script_async_block_t *existing = async_state.blocks;
        while (existing) {
            if (!existing->matched && existing->instance == block->instance &&
                existing->hash == block->hash &&
                script_context_same_captures(&existing->original, &block->original))
                break;
            existing = existing->next;
        }
        if (existing) {
            existing->matched = true;
            retain_descendant_blocks(existing);
            destroy_block(block);
        } else {
            block->matched = true;
            block->next = async_state.blocks;
            async_state.blocks = block;
            queue_block(block);
        }
    }
    transaction->async_blocks.size = 0;
    if (transaction->reconcile_async) {
        script_async_block_t **link = &async_state.blocks;
        while (*link) {
            script_async_block_t *block = *link;
            if (block->instance == transaction->instance && !block->matched) {
                *link = block->next;
                destroy_block(block);
            } else
                link = &block->next;
        }
    }
    const ecs_script_task_t *launches = transaction->async_launches.data;
    double now = transaction->async_launches.size ? async_now() : 0;
    for (uint32_t i = 0; i < transaction->async_launches.size; i++) {
        lock_futures();
        script_future_t *future = find_future(launches[i]);
        if (future) {
            future->started = true;
            if (future->timer) {
                future->deadline = now + future->delay;
                future->heap_index = async_state.timers.size;
                sicore_vec_push(&async_state.timers, &future, sizeof future);
                timer_up(future->heap_index);
            }
        }
        unlock_futures();
        if (future && future->function)
            future->function->desc.start(
                future->handle,
                future->arguments,
                future->argument_count,
                future->function->desc.context
            );
    }
    transaction->async_launches.size = 0;
}

void script_async_transaction_fini(script_transaction_t *transaction) {
    script_async_block_t **blocks = transaction->async_blocks.data;
    for (uint32_t i = 0; i < transaction->async_blocks.size; i++)
        destroy_block(blocks[i]);
    transaction->async_blocks.size = 0;
    const ecs_script_task_t *launches = transaction->async_launches.data;
    for (uint32_t i = 0; i < transaction->async_launches.size; i++) {
        lock_futures();
        script_future_t *future = find_future(launches[i]);
        unlock_futures();
        if (future)
            forget_future(future);
    }
    transaction->async_launches.size = 0;
}

static void pop_frame(script_eval_t *ctx, sicore_vec_t *frames) {
    script_task_frame_t frame = ((script_task_frame_t *)frames->data)[frames->size - 1];
    frames->size--;
    ctx->variables.size = frame.variable_count;
    restore_frame_scope(ctx, &frame);
    if (frame.with_scope)
        ctx->with = ctx->with->previous;
    if (frame.prefab)
        script_backend_prefab(ctx->entity);
    if (frame.restore_entity) {
        ctx->entity = frame.entity;
        ctx->parent = frame.parent;
    }
}

static void abandon_frame(script_eval_t *ctx, sicore_vec_t *frames) {
    script_task_frame_t frame = ((script_task_frame_t *)frames->data)[--frames->size];
    ctx->variables.size = frame.variable_count;
    restore_frame_scope(ctx, &frame);
    if (frame.with_scope)
        ctx->with = ctx->with->previous;
    if (frame.restore_entity) {
        ctx->entity = frame.entity;
        ctx->parent = frame.parent;
    }
}

static int await_value(
    script_async_block_t *block,
    script_eval_t *ctx,
    const ecs_script_node_t *node,
    ecs_script_task_t *waiting,
    bool *suspended
) {
    const ecs_expr_node_t *expression;
    const ecs_script_var_node_t *variable = NULL;
    if (node->kind == EcsAstAwait)
        expression = ((const ecs_script_await_t *)node)->expr;
    else {
        variable = (const ecs_script_var_node_t *)node;
        expression = variable->expr;
    }
    script_value_t value = { 0 };
    if (*waiting)
        value.future = *waiting;
    else if (script_eval_expr(ctx, expression, &value))
        return -1;
    if (value.future) {
        lock_futures();
        script_future_t *future = find_future(value.future);
        if (!future) {
            unlock_futures();
            return script_eval_error(ctx, node->pos, "task was cancelled");
        }
        if (!future->settled) {
            *waiting = value.future;
            script_async_block_t **waiters = future->waiters.data;
            bool found = false;
            for (uint32_t i = 0; i < future->waiters.size; i++)
                if (waiters[i] == block)
                    found = true;
            if (!found)
                sicore_vec_push(&future->waiters, &block, sizeof block);
            unlock_futures();
            *suspended = true;
            return 0;
        }
        if (future->error) {
            unlock_futures();
            return script_eval_error(ctx, node->pos, "%s: %s", future->error_type, future->error);
        }
        script_value_t result = future->function
                                    ? (script_value_t){ .type = future->function->desc.result_type,
                                                        .ptr = future->result,
                                                        .ops = &future->function->desc.result_ops }
                                    : (script_value_t){ .type = script_backend_type_lookup("bool"),
                                                        .storage.bool_ = true };
        value = copy_script_value(ctx, &result);
        unlock_futures();
        *waiting = 0;
    } else {
        bool condition = false;
        if (script_value_convert(
                ctx,
                node->pos,
                &value,
                script_backend_type_lookup("bool"),
                &condition
            ))
            return -1;
        if (!condition) {
            *suspended = true;
            return 0;
        }
    }
    if (variable) {
        script_variable_t binding = { .name = variable->name,
                                      .value = value,
                                      .mutable = node->kind == EcsAstMut ||
                                                 node->kind == EcsAstExportMut };
        sicore_vec_push(&ctx->variables, &binding, sizeof binding);
    }
    return 0;
}

static int run_block(script_async_block_t *block) {
    if (!script_instance_async_begin(block->instance))
        return 0;
    struct {
        const ecs_script_try_t *attempt;
        ecs_script_scope_t *handler;
    } handled[64];
    uint32_t handled_count = 0;
restart_segment:;
    script_eval_t next;
    script_context_copy(&next, &block->context);
    next.instruction_budget = 10000;
    sicore_vec_t frames;
    sicore_vec_init(&frames, sizeof(script_task_frame_t));
    const script_task_frame_t *old_frames = block->frames.data;
    for (uint32_t i = 0; i < block->frames.size; i++) {
        script_task_frame_t frame = old_frames[i];
        if (frame.iteration.collection.collection)
            frame.iteration.collection = copy_script_value(&next, &frame.iteration.collection);
        sicore_vec_push(&frames, &frame, sizeof frame);
    }
    script_transaction_t transaction;
    script_transaction_init(&transaction);
    script_transaction_active = &transaction;
    script_current_instance = block->instance;
    bool suspended = false;
    bool yielded = false;
    int status = transaction.failed ? -1 : 0;
    ecs_script_task_t waiting = block->waiting;
    siecs_log_capture_push(true);
    /* A handler already active at the previous suspension can be entered
     * directly. Otherwise replay the uncommitted prefix and replace its try. */
    if (handled_count) {
        for (uint32_t i = frames.size; i; i--) {
            script_task_frame_t *frame = &((script_task_frame_t *)frames.data)[i - 1];
            if (frame->attempt != handled[handled_count - 1].attempt)
                continue;
            while (frames.size >= i)
                abandon_frame(&next, &frames);
            push_frame(&frames, handled[handled_count - 1].handler, &next);
            waiting = 0;
            break;
        }
    }
    uint32_t steps = 0;
    while (frames.size && !suspended && !status) {
        if (++steps == 10000) {
            yielded = true;
            break;
        }
        script_task_frame_t *frame = &((script_task_frame_t *)frames.data)[frames.size - 1];
        if (frame->loop) {
            const ecs_script_for_t *loop = frame->loop;
            next.variables.size = frame->variable_count;
            if (!script_iteration_next(&next, loop, &frame->iteration)) {
                pop_frame(&next, &frames);
                continue;
            }
            uint32_t base = frame->variable_count;
            push_frame(&frames, loop->scope, &next);
            ((script_task_frame_t *)frames.data)[frames.size - 1].variable_count = base;
            continue;
        }
        if (frame->cursor == frame->scope->stmts.size) {
            pop_frame(&next, &frames);
            continue;
        }
        const ecs_script_node_t *node =
            ((ecs_script_node_t **)frame->scope->stmts.data)[frame->cursor];
        next.position = node->pos;
        bool awaited_variable =
            (node->kind == EcsAstConst || node->kind == EcsAstMut ||
             node->kind == EcsAstExportConst || node->kind == EcsAstExportMut) &&
            ((const ecs_script_var_node_t *)node)->is_await;
        if (node->kind == EcsAstTry) {
            const ecs_script_try_t *attempt = (const ecs_script_try_t *)node;
            ecs_script_scope_t *scope = attempt->try_scope;
            bool caught = false;
            for (uint32_t i = handled_count; i; i--) {
                if (handled[i - 1].attempt == attempt) {
                    scope = handled[i - 1].handler;
                    caught = true;
                    waiting = 0;
                    break;
                }
            }
            frame->cursor++;
            push_frame(&frames, scope, &next);
            if (!caught)
                ((script_task_frame_t *)frames.data)[frames.size - 1].attempt = attempt;
        } else if (node->kind == EcsAstWith || node->kind == EcsAstPairScope) {
            ecs_script_scope_t *scope;
            if (node->kind == EcsAstWith) {
                const ecs_script_with_t *with = (const ecs_script_with_t *)node;
                script_with_scope_push(&next, with->expressions, NULL);
                scope = with->scope;
            } else {
                const ecs_script_pair_scope_t *pair = (const ecs_script_pair_scope_t *)node;
                script_with_scope_push(&next, NULL, pair);
                scope = pair->scope;
            }
            frame->cursor++;
            push_frame(&frames, scope, &next);
            ((script_task_frame_t *)frames.data)[frames.size - 1].with_scope = true;
        } else if (node->kind == EcsAstAwait || awaited_variable) {
            status = await_value(block, &next, node, &waiting, &suspended);
            if (!status && !suspended)
                frame->cursor++;
        } else if (node->kind == EcsAstIf || node->kind == EcsAstWhile) {
            const ecs_expr_node_t *condition = node->kind == EcsAstIf
                                                   ? ((const ecs_script_if_t *)node)->expr
                                                   : ((const ecs_script_while_t *)node)->expr;
            bool result = false;
            status =
                script_value_write(&next, condition, script_backend_type_lookup("bool"), &result);
            if (status)
                break;
            ecs_script_scope_t *scope = NULL;
            if (node->kind == EcsAstIf) {
                const ecs_script_if_t *branch = (const ecs_script_if_t *)node;
                scope = result ? branch->if_true : branch->if_false;
                frame->cursor++;
            } else if (result)
                scope = ((const ecs_script_while_t *)node)->scope;
            else
                frame->cursor++;
            if (scope) {
                push_frame(&frames, scope, &next);
                if (node->kind == EcsAstWhile)
                    ((script_task_frame_t *)frames.data)[frames.size - 1].while_body = true;
            }
        } else if (node->kind == EcsAstFor) {
            const ecs_script_for_t *loop = (const ecs_script_for_t *)node;
            script_task_frame_t iteration = { .loop = loop, .variable_count = next.variables.size };
            status = script_iteration_init(&next, loop, &iteration.iteration);
            frame->cursor++;
            if (!status)
                sicore_vec_push(&frames, &iteration, sizeof iteration);
        } else if (
            node->kind == EcsAstEntity &&
            (!((const ecs_script_entity_t *)node)->kind ||
             strcmp(((const ecs_script_entity_t *)node)->kind, "struct"))
        ) {
            const ecs_script_entity_t *entity = (const ecs_script_entity_t *)node;
            ecs_entity_t previous_entity = next.entity, previous_parent = next.parent;
            frame->cursor++;
            status = script_entity_scope_begin(&next, entity);
            if (!status) {
                push_frame(&frames, entity->scope, &next);
                script_task_frame_t *entered =
                    &((script_task_frame_t *)frames.data)[frames.size - 1];
                entered->restore_entity = true;
                entered->entity = previous_entity;
                entered->parent = previous_parent;
                entered->prefab = entity->kind && !strcmp(entity->kind, "prefab");
            }
        } else if (node->kind == EcsAstContinue) {
            bool loop = false;
            while (frames.size) {
                script_task_frame_t *candidate =
                    &((script_task_frame_t *)frames.data)[frames.size - 1];
                if (candidate->loop) {
                    loop = true;
                    break;
                }
                bool while_body = candidate->while_body;
                pop_frame(&next, &frames);
                if (while_body) {
                    loop = true;
                    break;
                }
            }
            if (!loop)
                status = script_eval_error(&next, node->pos, "continue outside async loop");
        } else {
            status = script_eval_node(&next, node);
            if (!status)
                frame->cursor++;
        }
    }
    char *error = siecs_log_capture_pop();
    if ((status < 0 || transaction.failed) && handled_count < 64) {
        const script_task_frame_t *stack = frames.data;
        for (uint32_t i = frames.size; i; i--) {
            if (!stack[i - 1].attempt)
                continue;
            ecs_script_scope_t *handler = script_matching_catch(stack[i - 1].attempt, error);
            if (!handler)
                continue;
            handled[handled_count].attempt = stack[i - 1].attempt;
            handled[handled_count++].handler = handler;
            script_context_fini(&next);
            sicore_vec_fini(&frames);
            script_transaction_fini(&transaction);
            script_transaction_active = NULL;
            script_current_instance = NULL;
            free(error);
            goto restart_segment;
        }
    }
    script_current_instance = NULL;
    script_transaction_active = NULL;
    if (!status && transaction.failed)
        status = -1;
    if (!status)
        status = script_instance_async_prepare(block->instance, &transaction);
    if (!status)
        status = script_transaction_commit(&transaction);
    if (!status) {
        script_context_remap(&transaction, &next);
        script_task_frame_t *new_frames = frames.data;
        for (uint32_t i = 0; i < frames.size; i++) {
            new_frames[i].entity =
                script_transaction_committed_entity(&transaction, new_frames[i].entity);
            new_frames[i].parent =
                script_transaction_committed_entity(&transaction, new_frames[i].parent);
        }
        script_context_fini(&block->context);
        block->context = next;
        sicore_vec_fini(&block->frames);
        block->frames = frames;
        block->waiting = waiting;
        block->done = !frames.size;
        block->pending_names = false;
        observe_task(block, &transaction);
        if (suspended)
            script_instance_async_activity(block->instance, true);
        if (yielded)
            sicore_vec_push(&async_state.deferred, &block, sizeof block);
        collect_block_futures(block);
    } else {
        script_transaction_active = &transaction;
        script_context_fini(&next);
        script_transaction_active = NULL;
        sicore_vec_fini(&frames);
        observe_task(block, &transaction);
        block->pending_names = error && (strstr(error, "unresolved identifier") ||
                                         strstr(error, "has no requested component or pair"));
    }
    int32_t line = 0, column = 0;
    siecs_log_get_captured_error_pos(&line, &column);
    int report = status ? (block->pending_names ? 1 : 2) : 0;
    script_instance_async_state(
        block->instance,
        block->reported_state,
        report,
        status ? (error ? error : "asynchronous transaction failed") : NULL,
        line,
        column
    );
    block->reported_state = report;
    free(error);
    script_transaction_fini(&transaction);
    script_instance_async_end(block->instance);
    return status;
}

int script_async_flush(bool resume_yields) {
    if (!async_state.initialized || async_state.flushing)
        return 0;
    async_state.flushing = true;
    if (resume_yields) {
        script_async_block_t **deferred = async_state.deferred.data;
        for (uint32_t i = 0; i < async_state.deferred.size; i++)
            if (deferred[i])
                queue_block(deferred[i]);
        async_state.deferred.size = 0;
    }
    double now = resume_yields && async_state.timers.size ? async_now() : 0;
    lock_futures();
    if (resume_yields && async_state.timers.size) {
        while (async_state.timers.size) {
            script_future_t *future = ((script_future_t **)async_state.timers.data)[0];
            if (future->deadline > now)
                break;
            timer_remove(future);
            future->settled = true;
            sicore_vec_push(&async_state.completed, &future->handle, sizeof future->handle);
        }
    }
    const ecs_script_task_t *completed = async_state.completed.data;
    for (uint32_t i = 0; i < async_state.completed.size; i++) {
        script_future_t *future = find_future(completed[i]);
        if (!future)
            continue;
        script_async_block_t **waiters = future->waiters.data;
        for (uint32_t j = 0; j < future->waiters.size; j++)
            if (waiters[j])
                queue_block(waiters[j]);
        future->waiters.size = 0;
    }
    async_state.completed.size = 0;
    unlock_futures();
    int status = 0;
    uint32_t index = 0;
    while (index < async_state.ready.size && index < 10000) {
        script_async_block_t *block = ((script_async_block_t **)async_state.ready.data)[index++];
        if (!block || block->done)
            continue;
        block->queued = false;
        script_instance_async_activity(block->instance, false);
        if (run_block(block))
            status = -1;
    }
    if (index < async_state.ready.size) {
        script_async_block_t **ready = async_state.ready.data;
        memmove(ready, ready + index, (async_state.ready.size - index) * sizeof(*ready));
        async_state.ready.size -= index;
    } else
        async_state.ready.size = 0;
    async_state.flushing = false;
    return status;
}

void script_async_names_changed(void) {
    if (!async_state.initialized)
        return;
    for (script_async_block_t *block = async_state.blocks; block; block = block->next)
        if (block->pending_names)
            queue_block(block);
}

void script_async_instance_free(ecs_script_instance_t *instance) {
    script_async_block_t **link = &async_state.blocks;
    while (*link) {
        script_async_block_t *block = *link;
        if (block->instance != instance) {
            link = &block->next;
            continue;
        }
        *link = block->next;
        destroy_block(block);
    }
    for (uint32_t i = 0; i < async_state.futures.size; i++) {
        script_future_t *future = ((script_future_t **)async_state.futures.data)[i];
        if (future && future->instance == instance)
            forget_future(future);
    }
}

void script_async_fini(void) {
    if (!async_state.initialized)
        return;
    while (async_state.blocks) {
        script_async_block_t *block = async_state.blocks;
        async_state.blocks = block->next;
        destroy_block(block);
    }
    for (uint32_t i = 0; i < async_state.futures.size; i++) {
        script_future_t *future = ((script_future_t **)async_state.futures.data)[i];
        if (future)
            forget_future(future);
    }
    script_host_function_t *function = async_state.functions;
    while (function) {
        script_host_function_t *next = function->next;
        if (function->desc.context_free)
            function->desc.context_free(function->desc.context);
        free((char *)function->desc.name);
        free((void *)function->desc.parameters);
        result_layout_free(function->result_layout);
        free(function);
        function = next;
    }
    sicore_vec_fini(&async_state.ready);
    sicore_vec_fini(&async_state.deferred);
    sicore_vec_fini(&async_state.timers);
    lock_futures();
    sicore_vec_fini(&async_state.completed);
    sicore_vec_fini(&async_state.futures);
    unlock_futures();
    lock_futures();
    memset(&async_state, 0, sizeof async_state);
    unlock_futures();
}
