#ifndef SIGPU_INTERNAL_H
#define SIGPU_INTERNAL_H

#include "pack/gpushader.h"
#include <SDL3/SDL.h>
#include <sigpu.h>
#include <stdatomic.h>
#include <stdlib.h>

typedef enum {
    GPU_RESOURCE_NONE,
    GPU_RESOURCE_DEVICE,
    GPU_RESOURCE_BUFFER,
    GPU_RESOURCE_TEXTURE,
    GPU_RESOURCE_SAMPLER,
    GPU_RESOURCE_SHADER,
    GPU_RESOURCE_PROGRAM,
    GPU_RESOURCE_GEOMETRY,
    GPU_RESOURCE_SURFACE,
    GPU_RESOURCE_MATERIAL,
    /* More than one descriptor on the entity. */
    GPU_RESOURCE_MANY
} gpu_resource_kind_t;

/* A texture descriptor with its size resolved. */
typedef struct {
    uint8_t dimension;
    uint8_t format;
    uint8_t mips;
    uint8_t samples;
    uint32_t usage;
    uint32_t width;
    uint32_t height;
    uint32_t depth; /* layers or depth (cube: 6) */
} gpu_texture_info_t;

_Static_assert(sizeof(gpu_texture_info_t) == 20, "gpu_texture_info_t compares with memcmp");

#define GPU_PROGRAM_STAGES (1 + GPU_PROGRAM_VARIANTS)
#define GPU_NONE UINT32_MAX

/* Graphics entries of a program for one phase: index 0 is the program's
 * own phase, the others its variants. */
typedef struct {
    bool used;
    ecs_entity_t phase;
    SDL_GPUShader *vertex;
    SDL_GPUShader *fragment;
    /* Entry indices in the shader's pack, GPU_NONE when absent. */
    uint32_t vertex_entry;
    uint32_t fragment_entry;
    GpuRaster raster;
} gpu_program_stage_t;

/* SDL objects of a Ready resource. GpuResourceState.record is its index + 1;
 * a free slot has kind GPU_RESOURCE_NONE. A resource holds SDL objects only
 * while it is Ready. */
typedef struct {
    ecs_entity_t entity;
    uint8_t kind;
    /* Bumped each time the slot is reused, so caches keyed by the record
     * can tell a new resource from the one they saw. */
    uint32_t generation;
    union {
        struct {
            SDL_GPUBuffer *buffer;
            uint32_t size;
            uint32_t policy;
        } buffer;
        struct {
            SDL_GPUTexture *texture;
            gpu_texture_info_t info;
        } texture;
        SDL_GPUSampler *sampler;
        gpu_pack_t pack;
        struct {
            ecs_entity_t shader;
            gpu_program_stage_t stages[GPU_PROGRAM_STAGES];
            SDL_GPUComputePipeline *compute;
            uint32_t compute_entry;
        } program;
        struct {
            ecs_entity_t vertices;
            ecs_entity_t indices;
        } geometry;
        struct {
            SDL_Window *window;
            ecs_entity_t window_entity;
            SDL_GPUTextureFormat format;
            /* The swapchain encodes to sRGB itself. */
            bool srgb;
            uint32_t width;
            uint32_t height;
        } surface;
        struct {
            ecs_entity_t program;
            uint32_t slot;
            uint32_t texset;
        } material;
    } as;
} gpu_record_t;

typedef struct {
    ecs_entity_t dependency;
    ecs_entity_t dependent;
} gpu_edge_t;

typedef struct {
    char name[GPU_PATH_SIZE];
    void *data;
    size_t size;
} gpu_asset_t;

typedef enum {
    GPU_UPLOAD_BUFFER,
    GPU_UPLOAD_TEXTURE,
    /* An internal SDL buffer; data is borrowed and stays valid until the
     * next GpuRecord. */
    GPU_UPLOAD_RAW
} gpu_upload_kind_t;

typedef struct {
    ecs_entity_t entity;
    uint8_t kind;
    bool owned;
    /* Whole-buffer upload of a GpuUploadStream buffer: SDL cycles it. */
    bool cycle;
    /* Target record, resolved once per GpuRecord for the ready uploads. */
    const gpu_record_t *record;
    SDL_GPUBuffer *raw;
    uint32_t offset;
    GpuTextureRegion region;
    uint32_t pitch;
    const void *data;
    uint32_t size;
} gpu_upload_t;

#define GPU_VEC(type)                                                                              \
    struct {                                                                                       \
        type *items;                                                                               \
        uint32_t count;                                                                            \
        uint32_t capacity;                                                                         \
    }

bool gpu_vec_reserve(void **items, uint32_t *capacity, uint32_t needed, size_t size);
#define gpu_vec_push(vec, value)                                                                   \
    (gpu_vec_reserve((void **)&(vec).items, &(vec).capacity, (vec).count + 1,                      \
                     sizeof(*(vec).items))                                                         \
         ? ((vec).items[(vec).count++] = (value), true)                                            \
         : false)
#define gpu_vec_grow(vec, needed)                                                                  \
    gpu_vec_reserve((void **)&(vec).items, &(vec).capacity, (needed), sizeof(*(vec).items))
#define gpu_vec_free(vec) (free((vec).items), (vec).items = NULL, (vec).count = 0, (vec).capacity = 0)

/* ----------------------------------------------------------------------
 * Pipelines (pipeline.c)
 * ---------------------------------------------------------------------- */

#define GPU_MAX_COLOR_TARGETS 4

/* Everything a graphics pipeline depends on besides the program. */
typedef struct {
    uint32_t color_count;
    SDL_GPUTextureFormat colors[GPU_MAX_COLOR_TARGETS];
    SDL_GPUTextureFormat depth;
    uint8_t samples;
} gpu_targets_t;

typedef struct {
    uint32_t record;
    uint32_t generation;
    uint32_t stage;
    gpu_targets_t targets;
    uint64_t layout;
} gpu_pipeline_key_t;

typedef struct {
    gpu_pipeline_key_t key;
    SDL_GPUGraphicsPipeline *pipeline;
    /* Failed creations are remembered so they are not retried each frame. */
    bool failed;
} gpu_pipeline_t;

/* A normalized vertex layout and its hash; slot 1 is per instance. */
uint64_t gpu_layout_hash(const GpuVertexLayout *layout);
/* Cached pipeline, created within the frame budget when allow_create;
 * NULL when it cannot or must not be created now. */
SDL_GPUGraphicsPipeline *gpu_pipeline_get(
    const gpu_record_t *program,
    uint32_t stage,
    const gpu_targets_t *targets,
    const GpuVertexLayout *layout,
    bool allow_create
);
void gpu_pipelines_forget(uint32_t record);
void gpu_pipelines_release(void);
uint32_t gpu_program_stage(const gpu_record_t *program, ecs_entity_t phase);

/* ----------------------------------------------------------------------
 * Render graphs (graph.c, record.c)
 * ---------------------------------------------------------------------- */

typedef struct {
    ecs_entity_t pass;
    gpu_pass_fn fn;
    void *user_data;
    gpu_dtor_fn dtor;
} gpu_callback_t;

/* A compiled pass. */
typedef struct {
    ecs_entity_t entity;
    GpuPass desc;
    GpuPassIO io;
    /* Roots (bits) that need this pass; it records when one is active. */
    uint64_t roots;
    /* Index in the surfaces the graph presents to, GPU_NONE when the pass
     * writes no surface. */
    uint32_t surface;
    /* Cached views: what the attachments hold since the last recording. */
    uint64_t cache_signature;
    bool cache_valid;
    /* Batches drawn this frame (geometry passes), filled in GpuPrepare. */
    uint32_t first_batch;
    uint32_t batch_count;
    bool skip;
} gpu_plan_pass_t;

typedef struct {
    ecs_entity_t entity;
    bool dirty;
    bool valid;
    int32_t order;
    uint64_t signature;
    GPU_VEC(gpu_plan_pass_t) plan;
    /* Root bit -> surface entity (0 for an always-active root). */
    ecs_entity_t roots[64];
    uint32_t root_count;
    bool seen;
} gpu_graph_t;

/* A resolved binding a draw provides on top of the pass's named ones. */
typedef struct {
    const char *name;
    uint8_t scope;
    SDL_GPUBuffer *buffer;
    SDL_GPUTexture *texture;
    SDL_GPUSampler *sampler;
} gpu_binding_t;

#define GPU_CTX_BINDINGS 16
#define GPU_CTX_UNIFORM_BYTES 4096

typedef struct {
    char name[GPU_PACK_NAME_SIZE];
    uint32_t offset;
    uint32_t size;
} gpu_ctx_uniform_t;

typedef struct {
    char name[GPU_PACK_NAME_SIZE];
    ecs_entity_t resource;
    ecs_entity_t sampler;
} gpu_ctx_resource_t;

struct gpu_pass_ctx_s {
    SDL_GPUCommandBuffer *cmd;
    SDL_GPURenderPass *render;
    SDL_GPUComputePass *compute;
    SDL_GPUCopyPass *copy;
    gpu_plan_pass_t *pass;
    gpu_targets_t targets;
    uint32_t width;
    uint32_t height;
    /* Bound program. */
    const gpu_record_t *program;
    uint32_t stage;
    const gpu_pack_t *pack;
    const gpu_pack_entry_t *entries[2];
    SDL_GPUGraphicsPipeline *pipeline;
    gpu_ctx_uniform_t uniforms[GPU_CTX_BINDINGS];
    uint32_t uniform_count;
    uint8_t uniform_data[GPU_CTX_UNIFORM_BYTES];
    uint32_t uniform_used;
    gpu_ctx_resource_t resources[GPU_CTX_BINDINGS];
    uint32_t resource_count;
};

/* Binds the program's resources: extra first, then the named uniforms and
 * resources of the callback, then the pass accesses with a binding. */
void gpu_ctx_bind(gpu_pass_ctx_t *ctx, const gpu_binding_t *extra, uint32_t extra_count);
bool gpu_ctx_pipeline(gpu_pass_ctx_t *ctx, const gpu_record_t *program, uint32_t stage,
                      SDL_GPUGraphicsPipeline *pipeline);
gpu_callback_t *gpu_callback_of(ecs_entity_t pass);
void gpu_graph_mark_dirty(ecs_entity_t graph);
void gpu_graphs_prepare(void);
void gpu_graphs_record(SDL_GPUCommandBuffer *cmd);
void gpu_graphs_fini(void);
void gpu_callbacks_drop(ecs_entity_t pass);

/* ----------------------------------------------------------------------
 * Draw features (feature.c, draw.c)
 * ---------------------------------------------------------------------- */

typedef struct {
    float center[3];
    float radius;
    uint32_t first;
    uint32_t count;
} gpu_cell_t;

typedef struct {
    uint16_t table;
    bool live;
    uint32_t offset;
    uint32_t capacity;
    uint32_t count;
    ecs_entity_t geometry;
    uint32_t empty_frames;
    uint32_t stable_frames;
    /* Rows written since the last upload, [lo, hi). */
    _Atomic uint32_t dirty_lo;
    _Atomic uint32_t dirty_hi;
    /* Cells of a stable range: rows in Morton order and their cells. */
    uint32_t *order;
    gpu_cell_t *cells;
    uint32_t cell_count;
    bool partitioned;
} gpu_range_t;

typedef struct gpu_feature_s {
    ecs_entity_t entity;
    char name[64];
    ecs_component_t instance;
    uint32_t stride;
    void (*extract)(ecs_iter_t *it);
    void *user_data;
    void (*user_data_dtor)(void *user_data);
    uint8_t culling;
    uint8_t ordering;
    float cell_size;
    uint32_t chunk_size;
    ecs_entity_t geometry;
    ecs_entity_t material;
    ecs_entity_t program;
    ecs_query_id_t sizing;
    ecs_system_id_t system;
    bool geometry_relation;
    bool material_relation;
    /* Mirror of the instance storage, indexed by offset + row. */
    uint8_t *instances;
    GpuSphere *bounds;
    ecs_entity_t *entities;
    ecs_entity_t *material_entities;
    uint32_t *materials;
    uint32_t *keys;
    uint32_t capacity;
    uint32_t used;
    GPU_VEC(gpu_range_t) ranges;
    /* Table id -> range index + 1. */
    uint32_t *range_of_table;
    uint32_t table_slots;
    /* Free blocks of the storage by size class (power of two). */
    GPU_VEC(uint32_t) free_blocks[32];
    SDL_GPUBuffer *buffer;
    uint32_t buffer_capacity;
    bool upload_all;
    uint32_t unresolved;
    /* Grows when any row changed; cached views compare it. */
    uint64_t revision;
} gpu_feature_t;

/* Material slot shared by every feature: what a row's material resolves to. */
typedef struct {
    ecs_entity_t entity;
    ecs_entity_t program;
    uint32_t slot;
    uint32_t texset;
    bool ready;
} gpu_material_t;

/* Program parameter component: the material block of the program. */
typedef struct {
    ecs_entity_t program;
    ecs_component_t params;
    /* Packing plan, built when the program is Ready. */
    bool planned;
    bool valid;
    uint32_t record_generation;
    uint32_t record;
    char message[GPU_STATE_MESSAGE_SIZE];
    uint32_t stride;
    char block[GPU_PACK_NAME_SIZE];
    struct gpu_copy_s *copies;
    uint32_t copy_count;
    /* Texture fields: shader sampler name, texture and sampler offsets. */
    struct gpu_texture_field_s *textures;
    uint32_t texture_count;
    SDL_GPUBuffer *buffer;
    uint32_t buffer_slots;
    uint8_t *data;
    uint32_t slots;
    GPU_VEC(uint32_t) free_slots;
    bool upload_all;
    uint32_t dirty_lo;
    uint32_t dirty_hi;
} gpu_params_t;

typedef struct {
    char name[GPU_PACK_NAME_SIZE];
    ecs_entity_t texture;
    ecs_entity_t sampler;
} gpu_texset_binding_t;

typedef struct {
    uint32_t count;
    gpu_texset_binding_t bindings[8];
    uint32_t users;
} gpu_texset_t;

void gpu_features_size(ecs_iter_t *it);
void gpu_features_prepare(void);
void gpu_features_release(void);
void gpu_features_fini(void);
uint32_t gpu_material_index(ecs_entity_t material);
gpu_feature_t *gpu_feature_of(ecs_entity_t entity);
gpu_params_t *gpu_params_of(ecs_component_t component);
gpu_params_t *gpu_params_for_program(ecs_entity_t program);
void gpu_material_build(ecs_entity_t entity, void *result);
void gpu_material_removed(ecs_entity_t entity);
void gpu_materials_prepare(void);
void gpu_materials_release(void);
void gpu_materials_fini(void);
const gpu_texset_t *gpu_texset(uint32_t id);
const gpu_material_t *gpu_material_at(uint32_t index);
/* Draws the batches of a geometry pass (draw.c). */
void gpu_draw_batches(gpu_pass_ctx_t *ctx, gpu_plan_pass_t *pass);
void gpu_batches_prepare_all(void);
bool gpu_pass_targets(const gpu_plan_pass_t *pass, gpu_targets_t *targets);
void gpu_batches_release(void);
void gpu_batches_fini(void);

/* ----------------------------------------------------------------------
 * Session
 * ---------------------------------------------------------------------- */

/* sigpu keeps one session per process, like the single device it owns. */
typedef struct {
    bool open;
    uint32_t budget;
    uint32_t pipeline_budget;
    /* sigpu_prewarm: the next frame has no creation budget. */
    bool prewarm;
    /* Grows when texture sets change: graphs revisit implicit reads. */
    uint32_t texset_revision;
    SDL_GPUDevice *gpu;
    bool video_owned;
    bool lost;
    bool lost_reported;
    /* A GpuDevice was added, set or removed: GpuSync reconsiders devices. */
    bool device_dirty;
    ecs_entity_t device;
    uint8_t frames_in_flight;
    uint64_t submissions;
    uint64_t frame;
    /* Fence of each submission still in flight, by frame slot. */
    SDL_GPUFence *fences[GPU_MAX_FRAMES_IN_FLIGHT];
    /* Staging of each frame slot, reused while large enough. */
    SDL_GPUTransferBuffer *staging[GPU_MAX_FRAMES_IN_FLIGHT];
    uint32_t staging_size[GPU_MAX_FRAMES_IN_FLIGHT];
    /* Command buffer recorded during GpuRecord, submitted in GpuSubmit. */
    SDL_GPUCommandBuffer *cmd;
    /* Resources to process, first in first out from queue_head. */
    GPU_VEC(ecs_entity_t) queue;
    uint32_t queue_head;
    /* Records whose owner changed or went away, released in GpuSync. */
    GPU_VEC(uint32_t) doomed;
    GPU_VEC(gpu_edge_t) edges;
    GPU_VEC(gpu_record_t) records;
    GPU_VEC(uint32_t) free_records;
    GPU_VEC(gpu_asset_t) assets;
    GPU_VEC(gpu_upload_t) uploads;
    ecs_event_t events[GpuEventCount];
    /* Pipelines. */
    GPU_VEC(gpu_pipeline_t) pipelines;
    uint32_t pipelines_created;
    /* Graphs and passes. */
    GPU_VEC(gpu_graph_t) graphs;
    GPU_VEC(gpu_callback_t) callbacks;
    GPU_VEC(ecs_entity_t) pass_seq;
    bool graphs_dirty;
    /* Surfaces that received a swapchain texture this frame. */
    GPU_VEC(ecs_entity_t) surfaces;
    bool window_hook;
    /* Internal resources, created with the first surface. */
    ecs_entity_t present_program;
    ecs_entity_t present_sampler;
    /* Linear clamp sampler of textures bound without one, created on use. */
    SDL_GPUSampler *default_sampler;
    /* White 1x1 texture of material textures left at 0, created on use. */
    SDL_GPUTexture *default_texture;
    /* Features and materials. */
    GPU_VEC(gpu_feature_t *) features;
    GPU_VEC(gpu_material_t) materials;
    GPU_VEC(gpu_params_t) params;
    GPU_VEC(gpu_texset_t) texsets;
    GPU_VEC(ecs_entity_t) retargeted;
    ecs_system_id_t sizing_system;
} gpu_session_t;

extern gpu_session_t gpu_session;

static inline GpuStats *gpu_stats(void) { return ecs_get_resource(GpuStats); }

/* Cold path, safe from observers: only sigpu's own queues change. touch
 * queues the entity once; with rebuild it also goes back to Pending and its
 * SDL objects are released in GpuSync. */
void gpu_touch(ecs_entity_t entity, bool rebuild);
void gpu_descriptor_changed(ecs_entity_t entity);
void gpu_descriptor_removed(ecs_entity_t entity);
void gpu_doom(uint32_t record);
gpu_resource_kind_t gpu_descriptor_kind(ecs_component_t component);
/* The entity's descriptor kind, GPU_RESOURCE_MANY if it holds several. */
gpu_resource_kind_t gpu_resource_kind(ecs_entity_t entity);
/* State of a live resource of the open session, else NULL. */
GpuResourceState *gpu_state(ecs_entity_t entity);
/* Queues every resource that refers to dependency. */
void gpu_wake(ecs_entity_t dependency);
void gpu_edge_add(ecs_entity_t dependency, ecs_entity_t dependent);

/* Frame. */
void gpu_sync(ecs_iter_t *it);
void gpu_prepare(ecs_iter_t *it);
void gpu_record(ecs_iter_t *it);
void gpu_submit(ecs_iter_t *it);
void gpu_session_shutdown(void *data);
/* SDL gave no command buffer or fence: the device is treated as lost. */
void gpu_mark_lost(const char *what);
void gpu_staging_release(void);
bool gpu_upload_raw(SDL_GPUBuffer *buffer, uint32_t offset, const void *data, uint32_t size);
void gpu_uploads_drop_raw(SDL_GPUBuffer *buffer);

const gpu_asset_t *gpu_asset_find(const char *name);

/* Result of building a resource; built holds the new SDL objects. */
typedef struct {
    uint8_t status;
    uint8_t error;
    ecs_entity_t dependency;
    char message[GPU_STATE_MESSAGE_SIZE];
    gpu_record_t built;
    /* The resource stays Ready with its current objects. */
    bool kept;
    /* Kept, but its size changed: dependents are woken. */
    bool resized;
} gpu_result_t;

void gpu_result_set(gpu_result_t *result, uint8_t status, uint8_t error,
                    ecs_entity_t dependency, const char *format, ...);
#define gpu_fail(result, error, dependency, ...)                                                   \
    gpu_result_set(result, GpuFailed, error, dependency, __VA_ARGS__)
const gpu_record_t *gpu_dependency_ready(
    gpu_result_t *result,
    ecs_entity_t dependency,
    gpu_resource_kind_t kind,
    const char *what
);
gpu_record_t *gpu_record_at(uint32_t id);

/* Surfaces (surface.c). */
void gpu_internal_assets(void);
void gpu_surface_build(ecs_entity_t entity, gpu_result_t *result, const gpu_record_t *current);
void gpu_surfaces_sync(void);
void gpu_surfaces_frame_end(void);
SDL_GPUTexture *gpu_surface_acquire(SDL_GPUCommandBuffer *cmd, ecs_entity_t surface,
                                    uint32_t *width, uint32_t *height);
void gpu_surface_release_window(gpu_record_t *record);
void gpu_surfaces_fini(void);
void gpu_geometry_build(ecs_entity_t entity, gpu_result_t *result);

/* SDL objects (objects.c). Each creation counts in GpuStats; NULL with a
 * log when SDL refuses. */
SDL_GPUBuffer *gpu_create_buffer(uint32_t size, uint32_t usage);
SDL_GPUBuffer *gpu_create_buffer_flags(uint32_t size, SDL_GPUBufferUsageFlags flags);
void gpu_release_buffer(SDL_GPUBuffer *buffer);
SDL_GPUTexture *gpu_create_texture(const gpu_texture_info_t *info);
SDL_GPUSampler *gpu_create_sampler(const GpuSampler *desc);
SDL_GPUShader *gpu_create_shader(const gpu_pack_t *pack, const gpu_pack_entry_t *entry);
SDL_GPUComputePipeline *gpu_create_compute_pipeline(
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry
);
void gpu_counted_create(const void *object, const char *what);
SDL_GPUSampler *gpu_default_sampler(void);
SDL_GPUTexture *gpu_default_texture(void);
void gpu_counted_release(const void *object);
void gpu_record_release(gpu_record_t *record);
bool gpu_open_device(bool debug);
void gpu_close_device(void);

/* Bytes per texel as SDL counts them, 0 for an invalid format. */
uint32_t gpu_format_size(uint8_t format);
bool gpu_format_depth(uint8_t format);
SDL_GPUSampleCount gpu_sample_count(uint32_t samples);

/* Pack of a Ready shader, and the stage entries of a Ready program. */
const gpu_pack_t *gpu_program_pack(const gpu_record_t *program);
const gpu_pack_entry_t *gpu_pack_entry_at(const gpu_pack_t *pack, uint32_t index);

/*
 * SDL objects of Ready resources, for sigpu's own passes, tests and the
 * examples that record commands themselves. NULL when the resource is not
 * Ready or not of that kind.
 */
SIECS_PUBLIC_API SDL_GPUDevice *gpu_session_device(void);
SIECS_PUBLIC_API SDL_GPUTextureFormat gpu_sdl_format(uint8_t format);
SIECS_PUBLIC_API const gpu_record_t *gpu_resource_record(ecs_entity_t entity, uint8_t kind);
SIECS_PUBLIC_API SDL_GPUBuffer *gpu_resource_buffer(ecs_entity_t buffer);
SIECS_PUBLIC_API SDL_GPUTexture *gpu_resource_texture(ecs_entity_t texture, gpu_texture_info_t *info);
/* The pack of a Ready program's shader and its entries (NULL if unused). */
SIECS_PUBLIC_API const gpu_pack_t *gpu_resource_program_pack(
    ecs_entity_t program,
    const gpu_pack_entry_t **vertex,
    const gpu_pack_entry_t **fragment,
    const gpu_pack_entry_t **compute
);

#endif
