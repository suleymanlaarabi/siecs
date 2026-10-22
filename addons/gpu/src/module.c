#include "sigpu_internal.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

ECS_MODULE_DEFINE(sigpu);
ECS_COMPONENT_DEFINE(GpuDevice, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuBuffer, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuTexture, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuSampler, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuShader, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuProgram, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuGeometry, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuSurface, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuSurfaceTargets);
ECS_COMPONENT_DEFINE(GpuRenderGraph, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuGraphState);
ECS_COMPONENT_DEFINE(GpuPass, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuPassIO, .mutation = EcsSetOnly);
ECS_COMPONENT_DEFINE(GpuView);
ECS_COMPONENT_DEFINE(GpuViewStats);
ECS_COMPONENT_DEFINE(GpuFeature);
ECS_RELATION_DEFINE(DrawGeometry, { .storage = EcsRelationByTarget, .acyclic = true,
                                    .on_delete_target = EcsRemoveRelation });
ECS_RELATION_DEFINE(DrawMaterial, { .storage = EcsRelationDense,
                                    .on_delete_target = EcsRemoveRelation });
ECS_RESOURCE_DEFINE(GpuSession);
ECS_RESOURCE_DEFINE(GpuStats);

ecs_phase_t GpuSync;
ecs_phase_t GpuExtract;
ecs_phase_t GpuPrepare;
ecs_phase_t GpuRecord;
ecs_phase_t GpuSubmit;

_Static_assert(sizeof(((GpuResourceState *)0)->message) == GPU_STATE_MESSAGE_SIZE, "message size");
_Static_assert(sizeof(((GpuShader *)0)->path) == GPU_PATH_SIZE, "shader path size");
_Static_assert(sizeof(((GpuProgram *)0)->vertex) == GPU_ENTRY_SIZE, "program entry size");
_Static_assert(GPU_ENTRY_SIZE == GPU_PACK_NAME_SIZE, "program entries hold any pack entry name");

/*
 * GpuResourceState lifecycle. IsA instantiation assigns the base's state
 * with `copy`: a copy is a new resource, Pending without the base's SDL
 * objects (RFC section 4.1). The destructor hands the record to GpuSync,
 * which releases it. There is no copy_ctor or move_ctor on purpose: without
 * them the core moves the column with memcpy, so a table migration keeps the
 * state and its record.
 */
static void state_dtor(void *ptr, uint32_t count) {
    GpuResourceState *states = ptr;
    for (uint32_t i = 0; i < count; i++) {
        if (gpu_session.open && states[i].record) {
            gpu_doom(states[i].record);
        }
        states[i].record = 0;
    }
}

static void state_copy(void *dst, const void *src, uint32_t count) {
    (void)src;
    state_dtor(dst, count);
    memset(dst, 0, (size_t)count * sizeof(GpuResourceState));
}

ECS_COMPONENT_DEFINE(GpuResourceState, .ops = { .dtor = state_dtor, .copy = state_copy });

static const struct {
    gpu_resource_kind_t kind;
    ecs_component_t *id;
} descriptors[] = {
    { GPU_RESOURCE_DEVICE, &ecs_id(GpuDevice) },   { GPU_RESOURCE_BUFFER, &ecs_id(GpuBuffer) },
    { GPU_RESOURCE_TEXTURE, &ecs_id(GpuTexture) }, { GPU_RESOURCE_SAMPLER, &ecs_id(GpuSampler) },
    { GPU_RESOURCE_SHADER, &ecs_id(GpuShader) },   { GPU_RESOURCE_PROGRAM, &ecs_id(GpuProgram) },
    { GPU_RESOURCE_GEOMETRY, &ecs_id(GpuGeometry) }, { GPU_RESOURCE_SURFACE, &ecs_id(GpuSurface) },
};

gpu_resource_kind_t gpu_descriptor_kind(ecs_component_t component) {
    for (uint32_t i = 0; i < sizeof(descriptors) / sizeof(descriptors[0]); i++) {
        if (component == *descriptors[i].id) {
            return descriptors[i].kind;
        }
    }
    return component && gpu_params_of(component) ? GPU_RESOURCE_MATERIAL : GPU_RESOURCE_NONE;
}

gpu_resource_kind_t gpu_resource_kind(ecs_entity_t entity) {
    gpu_resource_kind_t kind = GPU_RESOURCE_NONE;
    for (uint32_t i = 0; i < sizeof(descriptors) / sizeof(descriptors[0]); i++) {
        if (ecs_has_cid(entity, *descriptors[i].id)) {
            if (kind) {
                return GPU_RESOURCE_MANY;
            }
            kind = descriptors[i].kind;
        }
    }
    for (uint32_t i = 0; i < gpu_session.params.count; i++) {
        if (ecs_has_cid(entity, gpu_session.params.items[i].params)) {
            if (kind) {
                return GPU_RESOURCE_MANY;
            }
            kind = GPU_RESOURCE_MATERIAL;
        }
    }
    return kind;
}

/* The observers match every entity with a state, so they see any component
 * of a resource entity; event->component tells which. An IsA instance gets
 * OnAdd for its copied descriptor and state, without OnSet. Any change of a
 * GpuDevice makes GpuSync reconsider the devices. */
static void resource_added(ecs_observer_event_t *event) {
    const gpu_resource_kind_t kind = gpu_descriptor_kind(event->component);
    if (kind || event->component == ecs_id(GpuResourceState)) {
        gpu_session.device_dirty |=
            kind == GPU_RESOURCE_DEVICE || ecs_has(event->entity, GpuDevice);
        gpu_touch(event->entity, false);
    }
}

static void resource_set(ecs_observer_event_t *event) {
    const gpu_resource_kind_t kind = gpu_descriptor_kind(event->component);
    if (kind) {
        gpu_session.device_dirty |= kind == GPU_RESOURCE_DEVICE;
        gpu_descriptor_changed(event->entity);
    }
}

static void resource_removed(ecs_observer_event_t *event) {
    const gpu_resource_kind_t kind = gpu_descriptor_kind(event->component);
    if (kind) {
        gpu_session.device_dirty |= kind == GPU_RESOURCE_DEVICE;
        if (kind == GPU_RESOURCE_MATERIAL) {
            gpu_material_removed(event->entity);
        }
        gpu_descriptor_removed(event->entity);
    }
}

/* Graph topology: any change of a graph, of a pass or of its accesses
 * recompiles the graph at the next GpuPrepare. */
static void graph_changed(ecs_observer_event_t *event) {
    if (event->component == ecs_id(GpuRenderGraph)) {
        gpu_graph_mark_dirty(event->entity);
    } else if (event->component == ecs_id(GpuPass) || event->component == ecs_id(GpuPassIO)) {
        gpu_graph_mark_dirty(ecs_target(event->entity, ChildOf));
        if (event->event == EcsOnRemove && event->component == ecs_id(GpuPass)) {
            gpu_callbacks_drop(event->entity);
        }
    }
}

static void pass_moved(ecs_observer_event_t *event) {
    const ecs_relation_event_t *change = event->trigger_data;
    if (change->relation == ecs_rid(ChildOf)) {
        gpu_graph_mark_dirty(change->old_target);
        gpu_graph_mark_dirty(change->new_target);
    }
}

/* A drawable changed material without changing table: its row is
 * re-resolved in GpuPrepare. */
static void material_retargeted(ecs_observer_event_t *event) {
    const ecs_relation_event_t *change = event->trigger_data;
    if (change->relation == ecs_rid(DrawMaterial)) {
        gpu_vec_push(gpu_session.retargeted, event->entity);
    }
}

ecs_event_t sigpu_event_id(uint32_t kind) {
    return kind < GpuEventCount ? gpu_session.events[kind] : 0;
}

bool sigpu_ready(ecs_entity_t resource) {
    const GpuResourceState *state = gpu_state(resource);
    return state && state->status == GpuReady;
}

uint32_t sigpu_format_size(uint8_t format) { return gpu_format_size(format); }

const gpu_asset_t *gpu_asset_find(const char *name) {
    for (uint32_t i = 0; i < gpu_session.assets.count; i++) {
        if (strcmp(gpu_session.assets.items[i].name, name) == 0) {
            return &gpu_session.assets.items[i];
        }
    }
    return NULL;
}

/* Shaders that name the asset are rebuilt from the new bytes. */
static void requeue_shaders(const char *name) {
    if (!gpu_session.open || !ecs_id(GpuShader)) {
        return;
    }
    GPU_VEC(ecs_entity_t) shaders = { 0 };
    ecs_query_each(it, i, ecs_in(GpuShader), ecs_in_optional(Abstract), ecs_in_optional(Disabled)) {
        const GpuShader *shader = ecs_field(&it, 0);
        if (strcmp(shader[i].path, name) == 0) {
            gpu_vec_push(shaders, it.entities[i]);
        }
    }
    for (uint32_t i = 0; i < shaders.count; i++) {
        gpu_touch(shaders.items[i], true);
    }
    gpu_vec_free(shaders);
}

bool sigpu_asset_register(const char *name, const void *data, size_t size) {
    const size_t length = name ? strlen(name) : 0;
    if (!length || length >= GPU_PATH_SIZE || (size && !data)) {
        return false;
    }
    void *copy = malloc(size ? size : 1);
    if (!copy) {
        return false;
    }
    memcpy(copy, data, size);
    gpu_asset_t *asset = (gpu_asset_t *)gpu_asset_find(name);
    if (asset) {
        free(asset->data);
    } else {
        gpu_asset_t added = { .data = NULL };
        memcpy(added.name, name, length + 1);
        if (!gpu_vec_push(gpu_session.assets, added)) {
            free(copy);
            return false;
        }
        asset = &gpu_session.assets.items[gpu_session.assets.count - 1];
    }
    asset->data = copy;
    asset->size = size;
    requeue_shaders(name);
    return true;
}

void sigpu_import(const sigpu_props_t *props) {
    /* Assets may be registered before the import; they live until fini. */
    GPU_VEC(gpu_asset_t) assets = { gpu_session.assets.items, gpu_session.assets.count,
                                    gpu_session.assets.capacity };
    gpu_session = (gpu_session_t){ .open = true };
    gpu_session.assets.items = assets.items;
    gpu_session.assets.count = assets.count;
    gpu_session.assets.capacity = assets.capacity;
    gpu_session.budget = props && props->budget ? props->budget : 64;

    gpu_session.pipeline_budget =
        props && props->pipeline_budget ? props->pipeline_budget : 16;

    sireflect(GpuRaster);
    sireflect(GpuProgramVariant);
    sireflect(GpuVertexAttribute);
    sireflect(GpuGeometryLod);
    sireflect(GpuAccess);
    ECS_COMPONENT_REGISTER(GpuDevice, GpuResourceState, GpuBuffer, GpuTexture, GpuSampler);
    ECS_COMPONENT_REGISTER(GpuShader, GpuProgram, GpuGeometry, GpuSurface, GpuSurfaceTargets);
    ECS_COMPONENT_REGISTER(GpuRenderGraph, GpuGraphState, GpuPass, GpuPassIO, GpuView);
    ECS_COMPONENT_REGISTER(GpuFeature, GpuViewStats);
    ECS_RELATION_REGISTER(DrawGeometry, DrawMaterial);
    ECS_RESOURCE_REGISTER(GpuSession);
    ECS_RESOURCE_REGISTER(GpuStats);
    ecs_set_resource(GpuSession, { 0 });
    gpu_internal_assets();
    ecs_set_resource(GpuStats, { 0 });
    ecs_with(GpuDevice, GpuResourceState);
    ecs_with(GpuBuffer, GpuResourceState);
    ecs_with(GpuTexture, GpuResourceState);
    ecs_with(GpuSampler, GpuResourceState);
    ecs_with(GpuShader, GpuResourceState);
    ecs_with(GpuProgram, GpuResourceState);
    ecs_with(GpuGeometry, GpuResourceState);
    ecs_with(GpuSurface, GpuResourceState);
    ecs_with(GpuRenderGraph, GpuGraphState);
    ecs_with(GpuPass, GpuPassIO);
    for (uint32_t i = 0; i < GpuEventCount; i++) {
        gpu_session.events[i] = ecs_event();
    }

    GpuSync = ecs_phase({ .name = "GpuSync", .after = EcsPostUpdate, .before = EcsPreRender });
    GpuExtract = ecs_phase({ .name = "GpuExtract", .after = EcsPreRender, .before = EcsOnRender });
    GpuPrepare = ecs_phase({ .name = "GpuPrepare", .after = GpuExtract, .before = EcsOnRender });
    GpuRecord = ecs_phase({ .name = "GpuRecord", .after = GpuPrepare, .before = EcsOnRender });
    GpuSubmit = ecs_phase({ .name = "GpuSubmit", .after = EcsOnRender, .before = EcsPostRender });

    const ecs_query_desc_t resources = { .components = { ecs_filter(GpuResourceState) } };
    ecs_observer({ .on = EcsOnAdd, .query = resources, .callback = resource_added });
    ecs_observer({ .on = EcsOnSet, .query = resources, .callback = resource_set });
    ecs_observer({ .on = EcsOnRemove, .query = resources, .callback = resource_removed });
    const ecs_component_t graph_components[] = { ecs_id(GpuRenderGraph), ecs_id(GpuPass) };
    for (uint32_t i = 0; i < 2; i++) {
        const ecs_query_desc_t query = { .components = { { graph_components[i], EcsFilter } } };
        ecs_observer({ .on = EcsOnAdd, .query = query, .callback = graph_changed });
        ecs_observer({ .on = EcsOnSet, .query = query, .callback = graph_changed });
        ecs_observer({ .on = EcsOnRemove, .query = query, .callback = graph_changed });
    }
    ecs_observer({ .on = EcsOnRelationSet, .query.components = { ecs_filter(GpuPass) },
                   .callback = pass_moved });
    ecs_observer({ .on = EcsOnRelationRemove, .query.components = { ecs_filter(GpuPass) },
                   .callback = pass_moved });
    ecs_observer({ .on = EcsOnRelationSet, .callback = material_retargeted });

    ecs_system({ .name = "GpuSyncResources", .phase = GpuSync, .callback = gpu_sync,
                 .main_thread_only = true, .no_defer = true });
    gpu_session.sizing_system = ecs_system({ .name = "GpuSizeFeatures", .phase = GpuExtract,
                                             .callback = gpu_features_size,
                                             .main_thread_only = true, .no_defer = true });
    ecs_system({ .name = "GpuPrepareFrame", .phase = GpuPrepare, .callback = gpu_prepare,
                 .main_thread_only = true, .no_defer = true });
    ecs_system({ .name = "GpuRecordFrame", .phase = GpuRecord, .callback = gpu_record,
                 .main_thread_only = true, .no_defer = true });
    ecs_system({ .name = "GpuSubmitFrame", .phase = GpuSubmit, .callback = gpu_submit,
                 .main_thread_only = true, .no_defer = true });
    ecs_at_fini({ .callback = gpu_session_shutdown });
}
