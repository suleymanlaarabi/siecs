#include "sigpu_internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------------
 * Pass helpers
 * ---------------------------------------------------------------------- */

ecs_entity_t sigpu_graph(ecs_entity_t parent, const char *name, int32_t order) {
    const ecs_entity_t graph = name ? ecs_new_named(parent, name) : ecs_new();
    if (!name && parent) {
        ecs_relate(graph, ChildOf, parent);
    }
    ecs_set(graph, GpuRenderGraph, { .order = order });
    return graph;
}

ecs_entity_t sigpu_pass(ecs_entity_t graph, const GpuPassDesc *desc) {
    if (!graph || !ecs_is_alive(graph) || !desc) {
        return 0;
    }
    const ecs_entity_t pass = desc->name ? ecs_new_named(graph, desc->name) : ecs_new();
    if (!desc->name) {
        ecs_relate(pass, ChildOf, graph);
    }
    ecs_set(pass, GpuPass, { .kind = desc->kind, .order = desc->order,
                             .side_effect = desc->side_effect, .program = desc->program,
                             .phase = desc->phase, .view = desc->view });
    if (desc->execute) {
        sigpu_pass_execute(pass, desc->execute, desc->user_data, desc->user_data_dtor);
    }
    return pass;
}

static gpu_callback_t *callback_find(ecs_entity_t pass) {
    for (uint32_t i = 0; i < gpu_session.callbacks.count; i++) {
        if (gpu_session.callbacks.items[i].pass == pass) {
            return &gpu_session.callbacks.items[i];
        }
    }
    return NULL;
}

void gpu_callbacks_drop(ecs_entity_t pass) {
    gpu_callback_t *callback = callback_find(pass);
    if (!callback) {
        return;
    }
    const gpu_callback_t dropped = *callback;
    *callback = gpu_session.callbacks.items[--gpu_session.callbacks.count];
    if (dropped.dtor) {
        dropped.dtor(dropped.user_data);
    }
}

void sigpu_pass_execute(ecs_entity_t pass, gpu_pass_fn execute, void *user_data,
                        gpu_dtor_fn user_data_dtor) {
    if (!gpu_session.open || !ecs_is_alive(pass) || !ecs_has(pass, GpuPass)) {
        if (user_data_dtor) {
            user_data_dtor(user_data);
        }
        return;
    }
    gpu_callbacks_drop(pass);
    if (execute) {
        gpu_vec_push(gpu_session.callbacks, ((gpu_callback_t){ pass, execute, user_data,
                                                               user_data_dtor }));
    } else if (user_data_dtor) {
        user_data_dtor(user_data);
    }
}

gpu_callback_t *gpu_callback_of(ecs_entity_t pass) { return callback_find(pass); }

bool sigpu_pass_access(ecs_entity_t pass, const GpuAccess *access) {
    if (!access || !ecs_is_alive(pass) || !ecs_has(pass, GpuPass)) {
        return false;
    }
    const GpuPassIO *current = ecs_try_get(pass, GpuPassIO);
    GpuPassIO io = current ? *current : (GpuPassIO){ 0 };
    if (io.count >= GPU_PASS_MAX_ACCESSES) {
        return false;
    }
    io.items[io.count++] = *access;
    ecs_set_cid(pass, ecs_id(GpuPassIO), &io);
    return true;
}

bool sigpu_pass_color(ecs_entity_t pass, ecs_entity_t texture, uint8_t load, uint8_t store,
                      const float clear[4], ecs_entity_t resolve) {
    GpuAccess access = { .resource = texture, .aux = resolve, .usage = GpuUseColor,
                         .load = load, .store = store };
    if (clear) {
        memcpy(access.clear, clear, sizeof(access.clear));
    }
    return sigpu_pass_access(pass, &access);
}

bool sigpu_pass_depth(ecs_entity_t pass, ecs_entity_t texture, uint8_t load, uint8_t store,
                      float clear_depth, uint16_t layer) {
    return sigpu_pass_access(pass, &(GpuAccess){ .resource = texture, .usage = GpuUseDepth,
                                                 .load = load, .store = store,
                                                 .clear = { clear_depth }, .layer = layer });
}

static bool named_access(ecs_entity_t pass, const char *binding, ecs_entity_t resource,
                         ecs_entity_t aux, uint8_t usage) {
    GpuAccess access = { .resource = resource, .aux = aux, .usage = usage };
    if (binding) {
        if (strlen(binding) >= sizeof(access.binding)) {
            return false;
        }
        strcpy(access.binding, binding);
    }
    return sigpu_pass_access(pass, &access);
}

bool sigpu_pass_sample(ecs_entity_t pass, const char *binding, ecs_entity_t texture,
                       ecs_entity_t sampler) {
    return named_access(pass, binding, texture, sampler, GpuUseSampled);
}

bool sigpu_pass_storage_read(ecs_entity_t pass, const char *binding, ecs_entity_t resource) {
    return named_access(pass, binding, resource, 0, GpuUseStorageRead);
}

bool sigpu_pass_storage_write(ecs_entity_t pass, const char *binding, ecs_entity_t resource) {
    return named_access(pass, binding, resource, 0, GpuUseStorageWrite);
}

bool sigpu_graph_export(ecs_entity_t graph, ecs_entity_t resource) {
    const GpuRenderGraph *current = ecs_is_alive(graph) ? ecs_try_get(graph, GpuRenderGraph) : NULL;
    if (!current) {
        return false;
    }
    GpuRenderGraph desc = *current;
    for (uint32_t i = 0; i < GPU_GRAPH_EXPORTS; i++) {
        if (desc.exports[i] == resource) {
            return true;
        }
        if (!desc.exports[i]) {
            desc.exports[i] = resource;
            ecs_set_cid(graph, ecs_id(GpuRenderGraph), &desc);
            return true;
        }
    }
    return false;
}

/* ----------------------------------------------------------------------
 * Compilation (RFC section 8.2, 8.3)
 * ---------------------------------------------------------------------- */

static gpu_graph_t *graph_find(ecs_entity_t entity) {
    for (uint32_t i = 0; i < gpu_session.graphs.count; i++) {
        if (gpu_session.graphs.items[i].entity == entity) {
            return &gpu_session.graphs.items[i];
        }
    }
    return NULL;
}

void gpu_graph_mark_dirty(ecs_entity_t graph) {
    if (!graph || !gpu_session.open) {
        return;
    }
    gpu_graph_t *found = graph_find(graph);
    if (found) {
        found->dirty = true;
    } else {
        gpu_session.graphs_dirty = true;
    }
}

static uint32_t pass_seq(ecs_entity_t pass) {
    for (uint32_t i = 0; i < gpu_session.pass_seq.count; i++) {
        if (gpu_session.pass_seq.items[i] == pass) {
            return i;
        }
    }
    gpu_vec_push(gpu_session.pass_seq, pass);
    return gpu_session.pass_seq.count - 1;
}

typedef struct {
    ecs_entity_t entity;
    uint32_t seq;
    int32_t order;
} pass_sort_t;

static int pass_compare(const void *a, const void *b) {
    const pass_sort_t *left = a, *right = b;
    if (left->order != right->order) {
        return left->order < right->order ? -1 : 1;
    }
    return left->seq < right->seq ? -1 : left->seq > right->seq;
}

typedef struct {
    uint8_t status;
    uint8_t error;
    ecs_entity_t pass;
    char message[GPU_STATE_MESSAGE_SIZE];
} compile_result_t;

static bool reject(compile_result_t *result, uint8_t status, ecs_entity_t pass, const char *format,
                   ...) {
    result->status = status;
    result->error = status == GpuFailed ? GpuErrorInterface : GpuErrorNone;
    result->pass = pass;
    va_list args;
    va_start(args, format);
    vsnprintf(result->message, sizeof(result->message), format, args);
    va_end(args);
    return false;
}

static bool is_write(const GpuAccess *access) {
    switch (access->usage) {
    case GpuUseColor:
    case GpuUseDepth:
    case GpuUseStorageWrite:
    case GpuUseStorageReadWrite:
    case GpuUseCopyDst:
        return true;
    default:
        return false;
    }
}

static bool is_read(const GpuAccess *access) {
    switch (access->usage) {
    case GpuUseColor:
    case GpuUseDepth:
        return access->load == GpuLoadLoad;
    case GpuUseStorageWrite:
    case GpuUseCopyDst:
        return false;
    default:
        return access->usage != GpuUseNone;
    }
}

/* A write that replaces the whole content: earlier writers stop mattering. */
static bool is_full_write(const GpuAccess *access) {
    return (access->usage == GpuUseColor || access->usage == GpuUseDepth) &&
           access->load != GpuLoadLoad;
}

static const char *name_of(ecs_entity_t entity) {
    return entity && ecs_is_alive(entity) ? ecs_entity_name(entity) : "a dead entity";
}

/* Checks one access against its resource. waiting is set when the resource
 * is not Ready yet. */
static bool check_access(compile_result_t *result, ecs_entity_t pass, const GpuPass *desc,
                         const GpuAccess *access, gpu_targets_t *targets, uint32_t *samples) {
    const ecs_entity_t resource = access->resource;
    if (access->usage == GpuUseNone || access->usage >= GpuUsageCount) {
        return reject(result, GpuFailed, pass, "access to %s has no usage", name_of(resource));
    }
    if (!resource || !ecs_is_alive(resource)) {
        return reject(result, GpuFailed, pass, "an access names a dead resource");
    }
    const gpu_resource_kind_t kind = gpu_resource_kind(resource);
    const GpuResourceState *state = gpu_state(resource);
    if (kind == GPU_RESOURCE_NONE || kind == GPU_RESOURCE_MANY || !state) {
        return reject(result, GpuFailed, pass, "%s is not a GPU resource", name_of(resource));
    }
    if (state->status == GpuFailed) {
        return reject(result, GpuFailed, pass, "%s failed: %s", name_of(resource), state->message);
    }
    if (state->status != GpuReady) {
        return reject(result, GpuWaiting, pass, "waiting for %s", name_of(resource));
    }
    const gpu_record_t *record = gpu_record_at(state->record);
    const bool texture = kind == GPU_RESOURCE_TEXTURE;
    const bool buffer = kind == GPU_RESOURCE_BUFFER;
    const bool surface = kind == GPU_RESOURCE_SURFACE;
    const uint32_t usage = texture ? record->as.texture.info.usage : 0;
    switch (access->usage) {
    case GpuUseColor: {
        if (desc->kind != GpuPassRender && desc->kind != GpuPassPresent) {
            return reject(result, GpuFailed, pass, "color attachments need a render pass");
        }
        if (!surface && !(texture && usage & GpuTextureColorTarget)) {
            return reject(result, GpuFailed, pass, "%s is not a color target", name_of(resource));
        }
        if (targets->color_count == GPU_MAX_COLOR_TARGETS) {
            return reject(result, GpuFailed, pass, "more than 4 color attachments");
        }
        const uint32_t count = surface ? 1 : record->as.texture.info.samples;
        if (*samples && *samples != count) {
            return reject(result, GpuFailed, pass, "attachments differ in sample count");
        }
        *samples = count;
        targets->colors[targets->color_count++] =
            surface ? record->as.surface.format : gpu_sdl_format(record->as.texture.info.format);
        if (access->aux) {
            const gpu_record_t *resolve = gpu_resource_record(access->aux, GPU_RESOURCE_TEXTURE);
            if (!resolve) {
                return reject(result, gpu_state(access->aux) ? GpuWaiting : GpuFailed, pass,
                              "resolve target %s is not a Ready texture", name_of(access->aux));
            }
            if (count == 1 || resolve->as.texture.info.samples != 1 ||
                resolve->as.texture.info.format != record->as.texture.info.format) {
                return reject(result, GpuFailed, pass,
                              "resolve needs a multisampled source and a single-sample target "
                              "of the same format");
            }
        }
        return true;
    }
    case GpuUseDepth:
        if (desc->kind != GpuPassRender) {
            return reject(result, GpuFailed, pass, "depth attachments need a render pass");
        }
        if (!texture || !(usage & GpuTextureDepthTarget)) {
            return reject(result, GpuFailed, pass, "%s is not a depth target", name_of(resource));
        }
        if (targets->depth != SDL_GPU_TEXTUREFORMAT_INVALID) {
            return reject(result, GpuFailed, pass, "more than one depth attachment");
        }
        if (*samples && *samples != record->as.texture.info.samples) {
            return reject(result, GpuFailed, pass, "attachments differ in sample count");
        }
        *samples = record->as.texture.info.samples;
        targets->depth = gpu_sdl_format(record->as.texture.info.format);
        return true;
    case GpuUseSampled:
        if (!texture || !(usage & GpuTextureSampled) || record->as.texture.info.samples > 1) {
            return reject(result, GpuFailed, pass, "%s cannot be sampled", name_of(resource));
        }
        return true;
    case GpuUseStorageRead:
    case GpuUseStorageWrite:
    case GpuUseStorageReadWrite:
        if (!texture && !buffer) {
            return reject(result, GpuFailed, pass, "%s is not storage", name_of(resource));
        }
        if (access->usage != GpuUseStorageRead && desc->kind != GpuPassCompute) {
            return reject(result, GpuFailed, pass, "storage writes need a compute pass");
        }
        return true;
    case GpuUseVertex:
    case GpuUseIndex:
    case GpuUseIndirect:
        if (!buffer) {
            return reject(result, GpuFailed, pass, "%s is not a buffer", name_of(resource));
        }
        return true;
    case GpuUseCopySrc:
    case GpuUseCopyDst:
        if (!texture && !buffer) {
            return reject(result, GpuFailed, pass, "%s cannot be copied", name_of(resource));
        }
        return true;
    default:
        return false;
    }
}

static bool check_pass(compile_result_t *result, ecs_entity_t pass, const GpuPass *desc,
                       const GpuPassIO *io) {
    if (desc->kind > GpuPassPresent) {
        return reject(result, GpuFailed, pass, "kind is not a GpuPassKind");
    }
    if (io->count > GPU_PASS_MAX_ACCESSES) {
        return reject(result, GpuFailed, pass, "more than 16 accesses");
    }
    gpu_targets_t targets = { .depth = SDL_GPU_TEXTUREFORMAT_INVALID };
    uint32_t samples = 0;
    for (uint32_t i = 0; i < io->count; i++) {
        if (!check_access(result, pass, desc, &io->items[i], &targets, &samples)) {
            return false;
        }
    }
    if (desc->kind == GpuPassRender && !targets.color_count &&
        targets.depth == SDL_GPU_TEXTUREFORMAT_INVALID) {
        return reject(result, GpuFailed, pass, "a render pass needs an attachment");
    }
    if (desc->kind == GpuPassPresent &&
        (io->count != 2 || io->items[0].usage != GpuUseSampled || io->items[1].usage != GpuUseColor ||
         gpu_resource_kind(io->items[1].resource) != GPU_RESOURCE_SURFACE)) {
        return reject(result, GpuFailed, pass, "a present pass samples a texture into a surface");
    }
    /* Sampling a subresource the pass writes as an attachment. */
    for (uint32_t i = 0; i < io->count; i++) {
        const GpuAccess *read = &io->items[i];
        if (read->usage != GpuUseSampled) {
            continue;
        }
        for (uint32_t j = 0; j < io->count; j++) {
            const GpuAccess *attachment = &io->items[j];
            if ((attachment->usage == GpuUseColor || attachment->usage == GpuUseDepth) &&
                attachment->resource == read->resource) {
                return reject(result, GpuFailed, pass, "%s is sampled and attached in one pass",
                              name_of(read->resource));
            }
        }
    }
    if (desc->program) {
        const GpuResourceState *state = gpu_state(desc->program);
        if (!state || gpu_resource_kind(desc->program) != GPU_RESOURCE_PROGRAM) {
            return reject(result, GpuFailed, pass, "program is not a GpuProgram");
        }
        if (state->status != GpuReady) {
            return reject(result, state->status == GpuFailed ? GpuFailed : GpuWaiting, pass,
                          "waiting for program %s", name_of(desc->program));
        }
    }
    return true;
}

/* Writers of a subresource (attachment layer and mip; the whole resource
 * otherwise) since its last full write. */
typedef struct {
    ecs_entity_t resource;
    uint16_t layer;
    uint8_t mip;
    bool whole;
    GPU_VEC(uint32_t) writers;
    bool discarded;
} resource_track_t;

static bool is_attachment(const GpuAccess *access) {
    return access->usage == GpuUseColor || access->usage == GpuUseDepth;
}

/* A read of the whole resource depends on every subresource; an attachment
 * load only on its own. */
static bool track_matches(const resource_track_t *track, const GpuAccess *access) {
    if (track->resource != access->resource) {
        return false;
    }
    if (!is_attachment(access) || track->whole) {
        return true;
    }
    return track->layer == access->layer && track->mip == access->mip;
}

/* The accesses of a pass with the resolve targets of its color attachments
 * as writes of their own. */
/* Textures the graph writes that materials sample: geometry passes read
 * them implicitly (RFC section 8.1). Filled per compilation. */
static GPU_VEC(ecs_entity_t) implicit_reads;

static uint32_t expand_pass(const gpu_plan_pass_t *pass, GpuAccess *out) {
    const GpuPassIO *io = &pass->io;
    uint32_t count = 0;
    for (uint32_t i = 0; i < io->count; i++) {
        out[count++] = io->items[i];
        if (io->items[i].usage == GpuUseColor && io->items[i].aux) {
            out[count++] = (GpuAccess){ .resource = io->items[i].aux, .usage = GpuUseColor,
                                        .load = GpuLoadDontCare, .store = GpuStoreStore };
        }
    }
    if (pass->desc.kind == GpuPassRender && pass->desc.phase) {
        for (uint32_t i = 0; i < implicit_reads.count && count < 3 * GPU_PASS_MAX_ACCESSES; i++) {
            out[count++] = (GpuAccess){ .resource = implicit_reads.items[i],
                                        .usage = GpuUseSampled };
        }
    }
    return count;
}

/* The graph-written textures that a texture set in use samples. */
static void collect_implicit_reads(const gpu_graph_t *graph, uint32_t count) {
    implicit_reads.count = 0;
    for (uint32_t t = 1; t < gpu_session.texsets.count; t++) {
        const gpu_texset_t *set = &gpu_session.texsets.items[t];
        for (uint32_t b = 0; set->users && b < set->count; b++) {
            const ecs_entity_t texture = set->bindings[b].texture;
            bool written = false, known = false;
            for (uint32_t p = 0; p < count && !written; p++) {
                const GpuPassIO *io = &graph->plan.items[p].io;
                for (uint32_t a = 0; a < io->count; a++) {
                    written = written || (io->items[a].resource == texture && is_write(&io->items[a]));
                }
            }
            for (uint32_t i = 0; i < implicit_reads.count; i++) {
                known = known || implicit_reads.items[i] == texture;
            }
            if (written && !known && implicit_reads.count < GPU_PASS_MAX_ACCESSES) {
                gpu_vec_push(implicit_reads, texture);
            }
        }
    }
}

static bool exported(const GpuRenderGraph *graph, ecs_entity_t resource) {
    for (uint32_t i = 0; i < GPU_GRAPH_EXPORTS; i++) {
        if (graph->exports[i] == resource) {
            return true;
        }
    }
    return false;
}

static void graph_publish(gpu_graph_t *graph, const compile_result_t *result, uint16_t passes,
                          uint16_t culled) {
    GpuGraphState *state = ecs_try_get(graph->entity, GpuGraphState);
    if (!state) {
        return;
    }
    const bool changed = state->status != result->status ||
                         strcmp(state->message, result->message) != 0 ||
                         state->passes != passes || state->culled != culled;
    state->status = result->status;
    state->error = result->error;
    state->pass = result->pass;
    state->passes = passes;
    state->culled = culled;
    state->revision++;
    memcpy(state->message, result->message, sizeof(state->message));
    if (result->status == GpuFailed && changed) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: graph %s: %s", ecs_entity_name(graph->entity),
                     result->message);
    }
    ecs_modified_cid(graph->entity, ecs_id(GpuGraphState));
}

static void compile(gpu_graph_t *graph) {
    graph->dirty = false;
    graph->valid = false;
    graph->plan.count = 0;
    graph->root_count = 0;
    gpu_stats()->graph_compiles++;
    compile_result_t result = { .status = GpuReady };
    const GpuRenderGraph *desc = ecs_get(graph->entity, GpuRenderGraph);
    graph->order = desc->order;

    GPU_VEC(pass_sort_t) passes = { 0 };
    const ecs_relation_sources_t children = ecs_relation_sources(graph->entity, ecs_rid(ChildOf));
    for (uint32_t i = 0; i < children.count; i++) {
        const ecs_entity_t pass = children.entities[i];
        const GpuPass *pass_desc = ecs_try_get(pass, GpuPass);
        if (pass_desc && !pass_desc->disabled && !ecs_has(pass, Disabled)) {
            gpu_vec_push(passes, ((pass_sort_t){ pass, pass_seq(pass), pass_desc->order }));
        }
    }
    if (passes.count) {
        qsort(passes.items, passes.count, sizeof(pass_sort_t), pass_compare);
    }

    gpu_vec_grow(graph->plan, passes.count);
    for (uint32_t i = 0; i < passes.count; i++) {
        const ecs_entity_t pass = passes.items[i].entity;
        gpu_plan_pass_t *planned = &graph->plan.items[i];
        *planned = (gpu_plan_pass_t){ .entity = pass, .desc = *ecs_get(pass, GpuPass),
                                      .surface = GPU_NONE };
        const GpuPassIO *io = ecs_try_get(pass, GpuPassIO);
        planned->io = io ? *io : (GpuPassIO){ 0 };
        if (!check_pass(&result, pass, &planned->desc, &planned->io)) {
            break;
        }
    }
    const uint32_t count = passes.count;
    gpu_vec_free(passes);
    if (result.status != GpuReady) {
        graph_publish(graph, &result, 0, 0);
        return;
    }

    /* Forward walk: each read depends on the writers since the last full
     * write of the resource (RFC section 8.2). deps holds (reader, writer)
     * pairs. */
    GPU_VEC(resource_track_t) tracks = { 0 };
    GPU_VEC(uint32_t) deps = { 0 };
    bool ok = true;
    GpuAccess accesses[3 * GPU_PASS_MAX_ACCESSES];
    collect_implicit_reads(graph, count);
    for (uint32_t i = 0; i < count && ok; i++) {
        const gpu_plan_pass_t *pass = &graph->plan.items[i];
        const uint32_t access_count = expand_pass(pass, accesses);
        for (uint32_t a = 0; a < access_count && ok; a++) {
            const GpuAccess *access = &accesses[a];
            for (uint32_t t = 0; t < tracks.count && is_read(access); t++) {
                resource_track_t *track = &tracks.items[t];
                if (!track_matches(track, access)) {
                    continue;
                }
                if (track->discarded) {
                    ok = reject(&result, GpuFailed, pass->entity, "reads discarded content of %s",
                                name_of(access->resource));
                    break;
                }
                for (uint32_t w = 0; w < track->writers.count; w++) {
                    gpu_vec_push(deps, i);
                    gpu_vec_push(deps, track->writers.items[w]);
                }
            }
        }
        for (uint32_t a = 0; a < access_count; a++) {
            const GpuAccess *access = &accesses[a];
            if (!is_write(access)) {
                continue;
            }
            const bool whole = !is_attachment(access);
            resource_track_t *track = NULL;
            for (uint32_t t = 0; t < tracks.count; t++) {
                resource_track_t *candidate = &tracks.items[t];
                if (candidate->resource == access->resource && candidate->whole == whole &&
                    (whole || (candidate->layer == access->layer && candidate->mip == access->mip))) {
                    track = candidate;
                }
            }
            if (!track) {
                gpu_vec_push(tracks, ((resource_track_t){ .resource = access->resource,
                                                          .layer = access->layer,
                                                          .mip = access->mip,
                                                          .whole = whole }));
                track = &tracks.items[tracks.count - 1];
            } else if (is_full_write(access)) {
                track->writers.count = 0;
            }
            if (!track->writers.count || track->writers.items[track->writers.count - 1] != i) {
                gpu_vec_push(track->writers, i);
            }
            track->discarded = (access->usage == GpuUseColor || access->usage == GpuUseDepth) &&
                               access->store == GpuStoreDontCare;
        }
    }
    /* A graph-written resource read before any write reads undefined content. */
    for (uint32_t i = 0; i < count && ok; i++) {
        const gpu_plan_pass_t *pass = &graph->plan.items[i];
        for (uint32_t a = 0; a < pass->io.count && ok; a++) {
            const GpuAccess *access = &pass->io.items[a];
            if (!is_read(access)) {
                continue;
            }
            uint32_t first_write = GPU_NONE;
            GpuAccess others[3 * GPU_PASS_MAX_ACCESSES];
            for (uint32_t j = 0; j < count && first_write == GPU_NONE; j++) {
                const uint32_t other_count = expand_pass(&graph->plan.items[j], others);
                for (uint32_t b = 0; b < other_count; b++) {
                    if (others[b].resource == access->resource && is_write(&others[b])) {
                        first_write = j;
                        break;
                    }
                }
            }
            if (first_write != GPU_NONE && first_write >= i &&
                gpu_resource_kind(access->resource) != GPU_RESOURCE_BUFFER) {
                ok = reject(&result, GpuFailed, pass->entity,
                            "reads %s before any pass writes it", name_of(access->resource));
            }
        }
    }
    if (ok) {
        /* Roots: the last writers of each surface (one bit per surface) and
         * of each export, and side effects (always active). */
        for (uint32_t i = 0; i < count; i++) {
            if (graph->plan.items[i].desc.side_effect) {
                graph->plan.items[i].roots |= 1ull << 63;
            }
        }
        for (uint32_t t = 0; t < tracks.count; t++) {
            const resource_track_t *track = &tracks.items[t];
            uint64_t bits = 0;
            uint32_t surface = GPU_NONE;
            if (gpu_resource_kind(track->resource) == GPU_RESOURCE_SURFACE) {
                for (uint32_t r = 0; r < graph->root_count; r++) {
                    if (graph->roots[r] == track->resource) {
                        surface = r;
                    }
                }
                if (surface == GPU_NONE && graph->root_count < 63) {
                    surface = graph->root_count;
                    graph->roots[graph->root_count++] = track->resource;
                }
                bits |= surface != GPU_NONE ? 1ull << surface : 0;
            }
            if (exported(desc, track->resource)) {
                bits |= 1ull << 63;
            }
            for (uint32_t w = 0; bits && w < track->writers.count; w++) {
                gpu_plan_pass_t *writer = &graph->plan.items[track->writers.items[w]];
                writer->roots |= bits;
                if (surface != GPU_NONE) {
                    writer->surface = surface;
                }
            }
        }
        /* Backward: a pass is needed by the roots of the passes reading it. */
        for (uint32_t i = count; i-- > 0;) {
            for (uint32_t d = 0; d + 1 < deps.count; d += 2) {
                if (deps.items[d] == i) {
                    graph->plan.items[deps.items[d + 1]].roots |= graph->plan.items[i].roots;
                }
            }
        }
        uint32_t kept = 0;
        for (uint32_t i = 0; i < count; i++) {
            if (graph->plan.items[i].roots) {
                graph->plan.items[kept++] = graph->plan.items[i];
            }
        }
        const uint32_t culled = count - kept;
        graph->plan.count = kept;
        graph->valid = true;
        gpu_stats()->passes_culled += culled;
        graph_publish(graph, &result, (uint16_t)kept, (uint16_t)culled);
    } else {
        graph_publish(graph, &result, 0, 0);
    }
    for (uint32_t t = 0; t < tracks.count; t++) {
        gpu_vec_free(tracks.items[t].writers);
    }
    gpu_vec_free(tracks);
    gpu_vec_free(deps);
}

static uint64_t mix(uint64_t hash, uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    return hash;
}

static uint64_t resource_signature(uint64_t hash, ecs_entity_t resource) {
    const GpuResourceState *state = resource ? gpu_state(resource) : NULL;
    hash = mix(hash, resource);
    return state ? mix(mix(hash, state->requested), state->status) : hash;
}

/* What the compiled plan depends on besides topology: descriptors and
 * readiness of the resources the passes name. Sizes are not part of it. */
static uint64_t graph_signature(ecs_entity_t graph_entity) {
    uint64_t hash = mix(0, gpu_session.texset_revision);
    const ecs_relation_sources_t children = ecs_relation_sources(graph_entity, ecs_rid(ChildOf));
    for (uint32_t i = 0; i < children.count; i++) {
        const GpuPass *pass = ecs_try_get(children.entities[i], GpuPass);
        const GpuPassIO *io = pass ? ecs_try_get(children.entities[i], GpuPassIO) : NULL;
        if (!pass) {
            continue;
        }
        hash = resource_signature(hash, pass->program);
        for (uint32_t a = 0; io && a < io->count && a < GPU_PASS_MAX_ACCESSES; a++) {
            hash = resource_signature(hash, io->items[a].resource);
            if (io->items[a].usage == GpuUseColor) {
                hash = resource_signature(hash, io->items[a].aux);
            }
        }
    }
    return hash;
}

static int graph_compare(const void *a, const void *b) {
    const gpu_graph_t *left = a, *right = b;
    if (left->order != right->order) {
        return left->order < right->order ? -1 : 1;
    }
    return left->entity < right->entity ? -1 : left->entity > right->entity;
}

static void graph_free(gpu_graph_t *graph) { gpu_vec_free(graph->plan); }

/* Finds new graphs and forgets the ones that went away. */
static void graphs_discover(void) {
    for (uint32_t i = 0; i < gpu_session.graphs.count; i++) {
        gpu_session.graphs.items[i].seen = false;
    }
    ecs_query_each(it, i, ecs_in(GpuRenderGraph)) {
        const GpuRenderGraph *desc = ecs_field(&it, 0);
        if (desc[i].disabled) {
            continue;
        }
        gpu_graph_t *graph = graph_find(it.entities[i]);
        if (!graph) {
            gpu_vec_push(gpu_session.graphs, ((gpu_graph_t){ .entity = it.entities[i],
                                                              .dirty = true }));
            graph = &gpu_session.graphs.items[gpu_session.graphs.count - 1];
        }
        graph->seen = true;
        graph->order = desc[i].order;
    }
    uint32_t kept = 0;
    for (uint32_t i = 0; i < gpu_session.graphs.count; i++) {
        if (gpu_session.graphs.items[i].seen) {
            gpu_session.graphs.items[kept++] = gpu_session.graphs.items[i];
        } else {
            graph_free(&gpu_session.graphs.items[i]);
        }
    }
    gpu_session.graphs.count = kept;
    if (gpu_session.graphs.count) {
        qsort(gpu_session.graphs.items, gpu_session.graphs.count, sizeof(gpu_graph_t),
              graph_compare);
    }
}

void gpu_graphs_prepare(void) {
    if (gpu_session.graphs_dirty) {
        gpu_session.graphs_dirty = false;
        graphs_discover();
        /* Removed passes may have left dangling seq entries. */
        uint32_t kept = 0;
        for (uint32_t i = 0; i < gpu_session.pass_seq.count; i++) {
            const ecs_entity_t pass = gpu_session.pass_seq.items[i];
            if (ecs_is_alive(pass) && ecs_has(pass, GpuPass)) {
                gpu_session.pass_seq.items[kept++] = pass;
            }
        }
        gpu_session.pass_seq.count = kept;
    }
    for (uint32_t g = 0; g < gpu_session.graphs.count; g++) {
        gpu_graph_t *graph = &gpu_session.graphs.items[g];
        if (!ecs_is_alive(graph->entity) || !ecs_has(graph->entity, GpuRenderGraph)) {
            graph->valid = false;
            gpu_session.graphs_dirty = true;
            continue;
        }
        const uint64_t signature = graph_signature(graph->entity);
        if (graph->dirty || signature != graph->signature) {
            graph->signature = signature;
            compile(graph);
        }
    }
}

void gpu_graphs_fini(void) {
    for (uint32_t i = 0; i < gpu_session.graphs.count; i++) {
        graph_free(&gpu_session.graphs.items[i]);
    }
    gpu_vec_free(gpu_session.graphs);
    gpu_vec_free(gpu_session.pass_seq);
    gpu_session.graphs_dirty = false;
    /* Every callback is destroyed exactly once, here at the latest. */
    while (gpu_session.callbacks.count) {
        gpu_callbacks_drop(gpu_session.callbacks.items[0].pass);
    }
    gpu_vec_free(gpu_session.callbacks);
    gpu_vec_free(implicit_reads);
}
