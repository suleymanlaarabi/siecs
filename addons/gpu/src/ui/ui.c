#include "render/render_internal.h"

#include <stdlib.h>

typedef struct {
    ecs_entity_t entity;
    ecs_entity_t parent;
    UiNode node;
    Color background;
    UiBorder border;
    uint32_t child_first;
    uint32_t child_count;
    float measured_width;
    float measured_height;
    float x;
    float y;
    float width;
    float height;
    bool has_background;
    bool has_border;
} sigpu_ui_entry_t;

typedef struct {
    ecs_entity_t entity;
    uint32_t index;
} sigpu_ui_entity_ref_t;

typedef struct {
    ecs_entity_t parent;
    ecs_entity_t entity;
    int32_t order;
    uint32_t index;
} sigpu_ui_child_ref_t;

static ecs_query_id_t ui_query;
static sigpu_ui_entry_t *ui_entries;
static sigpu_ui_entity_ref_t *ui_entity_refs;
static sigpu_ui_child_ref_t *ui_children;
static sigpu_axis_instance_t *ui_instances;
static uint32_t ui_entry_count;
static uint32_t ui_entry_capacity;
static uint32_t ui_entity_ref_capacity;
static uint32_t ui_child_capacity;
static uint32_t ui_instance_count;
static uint32_t ui_instance_capacity;

static SDL_GPUBuffer *ui_instance_buffer;
static SDL_GPUTransferBuffer *ui_instance_transfer;
static SDL_GPUGraphicsPipeline *ui_pipeline;
static uint32_t ui_gpu_capacity;

ECS_COMPONENT_DEFINE(UiNode);
ECS_COMPONENT_DEFINE(UiBackground, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(UiBorder, .inheritance = EcsInheritShared);

static const sireflect_enum_desc_t ui_value_type_reflection = {
    .name = "SiUiValueType",
    .values = "{ Px = 0, Percent = 1, Auto = 2 }",
    .size = sizeof(SiUiValueType),
    .align = _Alignof(SiUiValueType),
};
static const sireflect_enum_desc_t ui_direction_reflection = {
    .name = "SiUiDirection",
    .values = "{ Column = 0, Row = 1 }",
    .size = sizeof(SiUiDirection),
    .align = _Alignof(SiUiDirection),
};
static const sireflect_enum_desc_t ui_justify_reflection = {
    .name = "SiUiJustify",
    .values = "{ Start = 0, Center = 1, End = 2, SpaceBetween = 3, SpaceAround = 4 }",
    .size = sizeof(SiUiJustify),
    .align = _Alignof(SiUiJustify),
};
static const sireflect_enum_desc_t ui_align_reflection = {
    .name = "SiUiAlign",
    .values = "{ Start = 0, Center = 1, End = 2, SpaceBetween = 3, SpaceAround = 4 }",
    .size = sizeof(SiUiAlign),
    .align = _Alignof(SiUiAlign),
};
static const sireflect_struct_desc_t ui_value_reflection = {
    .name = "SiUiValue",
    .fields = "{ SiUiValueType type; float value; }",
    .size = sizeof(SiUiValue),
    .align = _Alignof(SiUiValue),
};
static const sireflect_struct_desc_t ui_insets_reflection = {
    .name = "SiUiInsets",
    .fields = "{ float left; float top; float right; float bottom; }",
    .size = sizeof(SiUiInsets),
    .align = _Alignof(SiUiInsets),
};

void sigpu_ui_types_register(void) {
    sireflect_register_enum(&ui_value_type_reflection);
    sireflect_register_enum(&ui_direction_reflection);
    sireflect_register_enum(&ui_justify_reflection);
    sireflect_register_enum(&ui_align_reflection);
    sireflect_register_struct(&ui_value_reflection);
    sireflect_register_struct(&ui_insets_reflection);
    ECS_COMPONENT_REGISTER(UiNode, UiBackground, UiBorder);
}

static float ui_max(float a, float b) { return a > b ? a : b; }
static float ui_min(float a, float b) { return a < b ? a : b; }
static float ui_nonnegative(float value) { return value > 0.0f ? value : 0.0f; }

static float ui_resolve_value(SiUiValue value, float available, float auto_value) {
    switch (value.type) {
    case SI_UI_VALUE_PX:
        return ui_nonnegative(value.value);
    case SI_UI_VALUE_PERCENT:
        return ui_nonnegative(available * value.value * 0.01f);
    case SI_UI_VALUE_AUTO:
    default:
        return ui_nonnegative(auto_value);
    }
}

static float ui_gap_value(SiUiValue value, float available) {
    return value.type == SI_UI_VALUE_AUTO ? 0.0f : ui_resolve_value(value, available, 0.0f);
}

static void *ui_grow_array(void *data, uint32_t *capacity, uint32_t count, size_t stride) {
    if (*capacity >= count)
        return data;
    uint32_t next = *capacity ? *capacity : 64;
    while (next < count)
        next *= 2;
    void *grown = SDL_realloc(data, (size_t)next * stride);
    if (!grown) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "sigpu ui: out of memory");
        abort();
    }
    *capacity = next;
    return grown;
}

static int ui_compare_entity_ref(const void *left, const void *right) {
    const sigpu_ui_entity_ref_t *a = left;
    const sigpu_ui_entity_ref_t *b = right;
    return a->entity < b->entity ? -1 : a->entity > b->entity;
}

static int ui_compare_child_ref(const void *left, const void *right) {
    const sigpu_ui_child_ref_t *a = left;
    const sigpu_ui_child_ref_t *b = right;
    if (a->parent != b->parent)
        return a->parent < b->parent ? -1 : 1;
    if (a->order != b->order)
        return a->order < b->order ? -1 : 1;
    return a->entity < b->entity ? -1 : a->entity > b->entity;
}

static int32_t ui_find_entry(ecs_entity_t entity) {
    uint32_t first = 0;
    uint32_t count = ui_entry_count;
    while (count) {
        const uint32_t step = count / 2;
        const uint32_t at = first + step;
        if (ui_entity_refs[at].entity < entity) {
            first = at + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first < ui_entry_count && ui_entity_refs[first].entity == entity
        ? (int32_t)ui_entity_refs[first].index
        : -1;
}

static void ui_collect(void) {
    ui_entry_count = 0;

    for (ecs_iter_t it = ecs_query_iter(ui_query); ecs_iter_next(&it);) {
        const UiNode *nodes = ecs_field(&it, 0);
        const bool shared = ecs_field_is_shared(&it, 0);
        ui_entries = ui_grow_array(
            ui_entries,
            &ui_entry_capacity,
            ui_entry_count + it.count,
            sizeof(*ui_entries)
        );

        for (uint32_t i = 0; i < it.count; i++) {
            const ecs_entity_t entity = it.entities[i];
            const UiBackground *background = ecs_try_get(entity, UiBackground);
            const UiBorder *border = ecs_try_get(entity, UiBorder);
            ui_entries[ui_entry_count++] = (sigpu_ui_entry_t){
                .entity = entity,
                .parent = ecs_target(entity, ChildOf),
                .node = nodes[shared ? 0 : i],
                .background = background ? background->color : (Color){ 0, 0, 0, 0 },
                .border = border ? *border : (UiBorder){ 0 },
                .has_background = background && background->color.a,
                .has_border = border && border->width > 0.0f && border->color.a,
            };
        }
    }

    if (!ui_entry_count)
        return;

    ui_entity_refs = ui_grow_array(
        ui_entity_refs,
        &ui_entity_ref_capacity,
        ui_entry_count,
        sizeof(*ui_entity_refs)
    );
    ui_children =
        ui_grow_array(ui_children, &ui_child_capacity, ui_entry_count, sizeof(*ui_children));

    for (uint32_t i = 0; i < ui_entry_count; i++) {
        ui_entity_refs[i] =
            (sigpu_ui_entity_ref_t){ .entity = ui_entries[i].entity, .index = i };
        ui_children[i] = (sigpu_ui_child_ref_t){
            .parent = ui_entries[i].parent,
            .entity = ui_entries[i].entity,
            .order = ui_entries[i].node.order,
            .index = i,
        };
        ui_entries[i].child_first = 0;
        ui_entries[i].child_count = 0;
    }

    qsort(ui_entity_refs, ui_entry_count, sizeof(*ui_entity_refs), ui_compare_entity_ref);
    qsort(ui_children, ui_entry_count, sizeof(*ui_children), ui_compare_child_ref);

    uint32_t first = 0;
    while (first < ui_entry_count) {
        const ecs_entity_t parent = ui_children[first].parent;
        uint32_t end = first + 1;
        while (end < ui_entry_count && ui_children[end].parent == parent)
            end++;

        const int32_t parent_index = ui_find_entry(parent);
        if (parent_index >= 0) {
            ui_entries[parent_index].child_first = first;
            ui_entries[parent_index].child_count = end - first;
        }
        first = end;
    }
}

static void
ui_measure(uint32_t index, float available_width, float available_height, uint32_t depth) {
    sigpu_ui_entry_t *entry = &ui_entries[index];
    if (depth > ui_entry_count) {
        entry->measured_width = 0.0f;
        entry->measured_height = 0.0f;
        return;
    }

    const float inner_width =
        ui_nonnegative(available_width - entry->node.padding.left - entry->node.padding.right);
    const float inner_height =
        ui_nonnegative(available_height - entry->node.padding.top - entry->node.padding.bottom);
    const bool row = entry->node.direction == SI_UI_DIRECTION_ROW;
    const float gap = ui_gap_value(entry->node.gap, row ? inner_width : inner_height);

    float content_main = 0.0f;
    float content_cross = 0.0f;
    for (uint32_t i = 0; i < entry->child_count; i++) {
        const uint32_t child_index = ui_children[entry->child_first + i].index;
        ui_measure(child_index, inner_width, inner_height, depth + 1);
        const sigpu_ui_entry_t *child = &ui_entries[child_index];
        const float child_main = row ? child->measured_width : child->measured_height;
        const float child_cross = row ? child->measured_height : child->measured_width;
        content_main += child_main;
        content_cross = ui_max(content_cross, child_cross);
    }
    if (entry->child_count > 1)
        content_main += gap * (float)(entry->child_count - 1);

    const float auto_width = entry->node.padding.left + entry->node.padding.right +
        (row ? content_main : content_cross);
    const float auto_height = entry->node.padding.top + entry->node.padding.bottom +
        (row ? content_cross : content_main);

    entry->measured_width = ui_resolve_value(entry->node.width, available_width, auto_width);
    entry->measured_height = ui_resolve_value(entry->node.height, available_height, auto_height);
}

static float ui_cross_offset(SiUiAlign align, float free_space) {
    switch (align) {
    case SI_UI_ALIGN_CENTER:
        return free_space * 0.5f;
    case SI_UI_ALIGN_END:
        return free_space;
    default:
        return 0.0f;
    }
}

static void
ui_layout(uint32_t index, float x, float y, float width, float height, uint32_t depth) {
    if (depth > ui_entry_count)
        return;

    sigpu_ui_entry_t *entry = &ui_entries[index];
    entry->x = x;
    entry->y = y;
    entry->width = ui_nonnegative(width);
    entry->height = ui_nonnegative(height);

    if (!entry->child_count)
        return;

    const float inner_x = x + entry->node.padding.left;
    const float inner_y = y + entry->node.padding.top;
    const float inner_width =
        ui_nonnegative(width - entry->node.padding.left - entry->node.padding.right);
    const float inner_height =
        ui_nonnegative(height - entry->node.padding.top - entry->node.padding.bottom);
    const bool row = entry->node.direction == SI_UI_DIRECTION_ROW;
    const float inner_main = row ? inner_width : inner_height;
    const float inner_cross = row ? inner_height : inner_width;

    float gap = ui_gap_value(entry->node.gap, inner_main);
    float used_main = 0.0f;
    for (uint32_t i = 0; i < entry->child_count; i++) {
        const sigpu_ui_entry_t *child = &ui_entries[ui_children[entry->child_first + i].index];
        used_main += row ? child->measured_width : child->measured_height;
    }
    if (entry->child_count > 1)
        used_main += gap * (float)(entry->child_count - 1);

    const float free_main = ui_nonnegative(inner_main - used_main);
    float cursor = 0.0f;
    switch (entry->node.justify) {
    case SI_UI_JUSTIFY_CENTER:
        cursor = free_main * 0.5f;
        break;
    case SI_UI_JUSTIFY_END:
        cursor = free_main;
        break;
    case SI_UI_JUSTIFY_SPACE_BETWEEN:
        if (entry->child_count > 1)
            gap += free_main / (float)(entry->child_count - 1);
        break;
    case SI_UI_JUSTIFY_SPACE_AROUND:
        if (entry->child_count) {
            const float extra = free_main / (float)entry->child_count;
            cursor = extra * 0.5f;
            gap += extra;
        }
        break;
    default:
        break;
    }

    for (uint32_t i = 0; i < entry->child_count; i++) {
        const uint32_t child_index = ui_children[entry->child_first + i].index;
        sigpu_ui_entry_t *child = &ui_entries[child_index];
        const float child_width = child->measured_width;
        const float child_height = child->measured_height;
        const float child_cross = row ? child_height : child_width;
        const float cross =
            ui_cross_offset(entry->node.align, ui_nonnegative(inner_cross - child_cross));

        ui_layout(
            child_index,
            row ? inner_x + cursor : inner_x + cross,
            row ? inner_y + cross : inner_y + cursor,
            child_width,
            child_height,
            depth + 1
        );
        cursor += (row ? child_width : child_height) + gap;
    }
}

static void ui_reserve_instances(uint32_t additional) {
    const uint32_t needed = ui_instance_count + additional;
    if (ui_instance_capacity >= needed)
        return;

    uint32_t next = ui_instance_capacity ? ui_instance_capacity : 256;
    while (next < needed)
        next *= 2;

    sigpu_axis_instance_t *grown =
        SDL_realloc(ui_instances, (size_t)next * sizeof(*ui_instances));
    if (!grown) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "sigpu ui: out of memory");
        abort();
    }
    ui_instances = grown;
    ui_instance_capacity = next;
}

static uint8_t ui_color_channel(uint8_t channel) {
    uint8_t linear = SIGPU_RENDERSETTINGS->linear_lut[channel];
    return SIGPU_GPUCONTEXT->linear_swapchain ? SIGPU_RENDERSETTINGS->linear_lut[linear] : linear;
}

static void ui_emit_rect(float x, float y, float width, float height, Color color) {
    if (width <= 0.0f || height <= 0.0f || !color.a)
        return;

    ui_reserve_instances(1);
    ui_instances[ui_instance_count++] = (sigpu_axis_instance_t){
        .x = x + width * 0.5f,
        .y = y + height * 0.5f,
        .z = 0.0f,
        .width = width,
        .height = height,
        .depth = 0.0f,
        .r = ui_color_channel(color.r),
        .g = ui_color_channel(color.g),
        .b = ui_color_channel(color.b),
        .a = color.a,
        .bloom = 0.0f,
    };
}

static void ui_emit_entry(const sigpu_ui_entry_t *entry) {
    if (entry->has_background)
        ui_emit_rect(entry->x, entry->y, entry->width, entry->height, entry->background);

    if (!entry->has_border)
        return;

    const float border =
        ui_min(entry->border.width, ui_min(entry->width, entry->height) * 0.5f);
    if (border <= 0.0f)
        return;

    const Color color = entry->border.color;
    ui_emit_rect(entry->x, entry->y, entry->width, border, color);
    ui_emit_rect(entry->x, entry->y + entry->height - border, entry->width, border, color);
    ui_emit_rect(
        entry->x,
        entry->y + border,
        border,
        ui_nonnegative(entry->height - border * 2.0f),
        color
    );
    ui_emit_rect(
        entry->x + entry->width - border,
        entry->y + border,
        border,
        ui_nonnegative(entry->height - border * 2.0f),
        color
    );
}

static void ui_build_layout(void) {
    ui_collect();
    ui_instance_count = 0;

    if (!ui_entry_count || !SIGPU_FRAMECONTEXT->frame_width || !SIGPU_FRAMECONTEXT->frame_height)
        return;

    const float frame_width = (float)SIGPU_FRAMECONTEXT->frame_width;
    const float frame_height = (float)SIGPU_FRAMECONTEXT->frame_height;

    for (uint32_t i = 0; i < ui_entry_count; i++) {
        if (ui_find_entry(ui_entries[i].parent) >= 0)
            continue;

        ui_measure(i, frame_width, frame_height, 0);
        ui_layout(
            i,
            0.0f,
            0.0f,
            ui_entries[i].measured_width,
            ui_entries[i].measured_height,
            0
        );
    }

    for (uint32_t i = 0; i < ui_entry_count; i++)
        ui_emit_entry(&ui_entries[i]);
}

static void ui_layout_system(ecs_iter_t *it) {
    (void)it;
    ui_build_layout();
}

static SDL_GPUGraphicsPipeline *ui_create_pipeline(void) {
    SDL_GPUShader *vertex_shader =
        sigpu_shader_load(SIGPU_SHADER("primitive.vert.spv"), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    SDL_GPUShader *fragment_shader =
        sigpu_shader_load(SIGPU_SHADER("cube_sdr.frag.spv"), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);

    SDL_GPUVertexBufferDescription buffers[2] = {
        {
            .slot = 0,
            .pitch = sizeof(sigpu_mesh_vertex_t),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        },
        {
            .slot = 1,
            .pitch = sizeof(sigpu_axis_instance_t),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
        },
    };
    SDL_GPUVertexAttribute attributes[6] = {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(sigpu_mesh_vertex_t, x),
        },
        {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM,
            .offset = offsetof(sigpu_mesh_vertex_t, nx),
        },
        {
            .location = 2,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(sigpu_axis_instance_t, x),
        },
        {
            .location = 3,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(sigpu_axis_instance_t, width),
        },
        {
            .location = 4,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
            .offset = offsetof(sigpu_axis_instance_t, r),
        },
        {
            .location = 5,
            .buffer_slot = 1,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
            .offset = offsetof(sigpu_axis_instance_t, bloom),
        },
    };
    SDL_GPUColorTargetDescription target = {
        .format = SIGPU_GPUCONTEXT->swapchain_format,
        .blend_state = {
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .enable_blend = true,
        },
    };
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = {
            .vertex_buffer_descriptions = buffers,
            .num_vertex_buffers = 2,
            .vertex_attributes = attributes,
            .num_vertex_attributes = 6,
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        },
        .target_info = {
            .color_target_descriptions = &target,
            .num_color_targets = 1,
        },
    };

    SDL_GPUGraphicsPipeline *pipeline =
        SDL_CreateGPUGraphicsPipeline(SIGPU_GPUCONTEXT->device, &info);
    SDL_ReleaseGPUShader(SIGPU_GPUCONTEXT->device, vertex_shader);
    SDL_ReleaseGPUShader(SIGPU_GPUCONTEXT->device, fragment_shader);
    return pipeline;
}

static void ui_resize_gpu_buffer(uint32_t capacity) {
    if (ui_instance_buffer)
        SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, ui_instance_buffer);
    if (ui_instance_transfer)
        SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, ui_instance_transfer);

    ui_instance_buffer = SDL_CreateGPUBuffer(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = capacity * sizeof(sigpu_axis_instance_t),
        }
    );
    ui_instance_transfer = SDL_CreateGPUTransferBuffer(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = capacity * sizeof(sigpu_axis_instance_t),
        }
    );
    ui_gpu_capacity = capacity;
}

static void ui_reserve_gpu(uint32_t count) {
    if (ui_gpu_capacity >= count)
        return;
    uint32_t next = ui_gpu_capacity ? ui_gpu_capacity : 256;
    while (next < count)
        next *= 2;
    ui_resize_gpu_buffer(next);
}

void sigpu_ui_pass(void) {
    if (!SIGPU_FRAMECONTEXT->swapchain || !ui_instance_count)
        return;

    ui_reserve_gpu(ui_instance_count);

    void *mapped =
        SDL_MapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, ui_instance_transfer, true);
    SDL_memcpy(mapped, ui_instances, (size_t)ui_instance_count * sizeof(*ui_instances));
    SDL_UnmapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, ui_instance_transfer);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(SIGPU_FRAMECONTEXT->command_buffer);
    SDL_UploadToGPUBuffer(
        copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = ui_instance_transfer },
        &(SDL_GPUBufferRegion){
            .buffer = ui_instance_buffer,
            .size = ui_instance_count * sizeof(*ui_instances),
        },
        true
    );
    SDL_EndGPUCopyPass(copy);

    SDL_GPUColorTargetInfo color_target = {
        .texture = SIGPU_FRAMECONTEXT->swapchain,
        .load_op = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(SIGPU_FRAMECONTEXT->command_buffer, &color_target, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, ui_pipeline);

    SDL_GPUBufferBinding vertex_bindings[2] = {
        { .buffer = SIGPU_GPUCONTEXT->vertex_buffer },
        { .buffer = ui_instance_buffer },
    };
    SDL_BindGPUVertexBuffers(pass, 0, vertex_bindings, 2);
    SDL_BindGPUIndexBuffer(
        pass,
        &(SDL_GPUBufferBinding){ .buffer = SIGPU_GPUCONTEXT->index_buffer },
        SDL_GPU_INDEXELEMENTSIZE_16BIT
    );

    sigpu_transform_uniform_t transforms = { 0 };
    transforms.view_projection = sigpu_mat4_orthographic_lh(
        0.0f,
        (float)SIGPU_FRAMECONTEXT->frame_width,
        (float)SIGPU_FRAMECONTEXT->frame_height,
        0.0f,
        0.0f,
        1.0f
    );
    SDL_PushGPUVertexUniformData(
        SIGPU_FRAMECONTEXT->command_buffer,
        0,
        &transforms,
        sizeof(transforms)
    );

    sigpu_lighting_uniform_t lighting = { 0 };
    lighting.ambient_color_intensity[0] = 1.0f;
    lighting.ambient_color_intensity[1] = 1.0f;
    lighting.ambient_color_intensity[2] = 1.0f;
    lighting.ambient_color_intensity[3] = 1.0f;
    SDL_PushGPUFragmentUniformData(
        SIGPU_FRAMECONTEXT->command_buffer,
        0,
        &lighting,
        sizeof(lighting)
    );

    SDL_GPUTextureSamplerBinding shadow = {
        .texture = SIGPU_GPUTARGETS->shadow_texture,
        .sampler = SIGPU_GPUTARGETS->shadow_sampler,
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow, 1);

    const sigpu_mesh_t *quad = &SIGPU_GPUCONTEXT->meshes[SIGPU_MESH_CUBE];
    SDL_DrawGPUIndexedPrimitives(
        pass,
        6,
        ui_instance_count,
        quad->first_index,
        quad->vertex_offset,
        0
    );
    SDL_EndGPURenderPass(pass);

    SIGPU_RENDERSTATS->draw_calls++;
    SIGPU_RENDERSTATS->drawn_instances += ui_instance_count;
}

void sigpu_ui_register(void) {
    ui_query = ecs_query({ .components = { ecs_in(UiNode) } });
    ui_pipeline = ui_create_pipeline();
    ui_resize_gpu_buffer(256);
    ecs_system(
        {
            .name = "UiLayout",
            .phase = EcsOnRender,
            .callback = ui_layout_system,
            .main_thread_only = true,
        }
    );
}

void sigpu_ui_fini(void) {
    if (ui_query) {
        ecs_query_fini(ui_query);
        ui_query = 0;
    }

    if (ui_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(SIGPU_GPUCONTEXT->device, ui_pipeline);
    if (ui_instance_buffer)
        SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, ui_instance_buffer);
    if (ui_instance_transfer)
        SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, ui_instance_transfer);

    ui_pipeline = NULL;
    ui_instance_buffer = NULL;
    ui_instance_transfer = NULL;
    ui_gpu_capacity = 0;

    SDL_free(ui_entries);
    SDL_free(ui_entity_refs);
    SDL_free(ui_children);
    SDL_free(ui_instances);
    ui_entries = NULL;
    ui_entity_refs = NULL;
    ui_children = NULL;
    ui_instances = NULL;
    ui_entry_count = 0;
    ui_entry_capacity = ui_entity_ref_capacity = ui_child_capacity = 0;
    ui_instance_count = ui_instance_capacity = 0;
}

SiUiValue sigpu_ui_px(float value) {
    return (SiUiValue){ .type = SI_UI_VALUE_PX, .value = value };
}

SiUiValue sigpu_ui_percent(float value) {
    return (SiUiValue){ .type = SI_UI_VALUE_PERCENT, .value = value };
}

SiUiValue sigpu_ui_auto(void) {
    return (SiUiValue){ .type = SI_UI_VALUE_AUTO, .value = 0.0f };
}

UiNode sigpu_ui_hstack(void) {
    return (UiNode){
        .width = { .type = SI_UI_VALUE_PERCENT, .value = 100.0f },
        .height = { .type = SI_UI_VALUE_PERCENT, .value = 100.0f },
        .direction = SI_UI_DIRECTION_ROW,
        .gap = { .type = SI_UI_VALUE_PX, .value = 5.0f },
        .justify = SI_UI_JUSTIFY_START,
        .align = SI_UI_ALIGN_START,
    };
}

UiNode sigpu_ui_vstack(void) {
    UiNode node = sigpu_ui_hstack();
    node.direction = SI_UI_DIRECTION_COLUMN;
    node.gap = sigpu_ui_px(4.0f);
    return node;
}

ecs_entity_t sigpu_ui_create(ecs_entity_t parent, const UiNode *node) {
    ecs_entity_t entity = ecs_new();
    UiNode value = node ? *node : sigpu_ui_hstack();
    ecs_set_cid(entity, ecs_id(UiNode), &value);
    if (parent)
        ecs_relate(entity, ChildOf, parent);
    return entity;
}

void sigpu_ui_set_node(ecs_entity_t entity, const UiNode *node) {
    if (entity && node)
        ecs_set_cid(entity, ecs_id(UiNode), node);
}

void sigpu_ui_set_background(ecs_entity_t entity, Color color) {
    if (!entity)
        return;
    UiBackground background = { .color = color };
    ecs_set_cid(entity, ecs_id(UiBackground), &background);
}

void sigpu_ui_set_border(ecs_entity_t entity, float width, Color color) {
    if (!entity)
        return;
    UiBorder border = { .width = width, .color = color };
    ecs_set_cid(entity, ecs_id(UiBorder), &border);
}
