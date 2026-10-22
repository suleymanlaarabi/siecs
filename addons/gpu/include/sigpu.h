#ifndef SIECS_SIGPU_H
#define SIECS_SIGPU_H
#include <siecs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * sigpu: ECS-native rendering on SDL3 GPU (addons/gpu/RFC.md). No SDL type
 * appears in this header.
 *
 * Resources. A GPU resource is an entity with one descriptor: GpuDevice,
 * GpuBuffer, GpuTexture, GpuSampler, GpuShader, GpuProgram, GpuGeometry,
 * GpuSurface, or the parameter component of a program (a material). Adding
 * a descriptor adds GpuResourceState. Setting a descriptor queues the entity
 * once per change; GpuSync creates the SDL GPU objects, within a per-frame
 * budget, once the device is open. Observers never call SDL.
 *
 * Render graphs. A GpuRenderGraph entity owns its passes (ChildOf); each
 * pass declares what it reads and writes in GpuPassIO. The order is
 * (GpuPass.order, creation), dependencies follow from conflicting accesses,
 * passes nobody needs are culled. Every surface gets a graph that presents
 * its color target (GpuSurfaceTargets).
 *
 * Draw features. A feature turns the rows of a query into instances kept in
 * persistent per-table ranges; geometry passes cull, batch and draw them.
 *
 * Every function of this header runs on the main thread, outside parallel
 * systems, except sigpu_instances, sigpu_bounds and sigpu_order_keys, which
 * extraction callbacks call.
 */
ECS_MODULE_DECLARE(sigpu, {
    /* Queued resources processed per frame; 0 selects 64. */
    uint32_t budget;
    /* Graphics pipelines created per frame before batches wait; 0 selects 16. */
    uint32_t pipeline_budget;
});

/* Frame phases (RFC section 5), registered by sigpu_import. */
SIECS_PUBLIC_API extern ecs_phase_t GpuSync;
SIECS_PUBLIC_API extern ecs_phase_t GpuExtract;
SIECS_PUBLIC_API extern ecs_phase_t GpuPrepare;
SIECS_PUBLIC_API extern ecs_phase_t GpuRecord;
SIECS_PUBLIC_API extern ecs_phase_t GpuSubmit;

#define GPU_MAX_FRAMES_IN_FLIGHT 3

/*
 * One SDL GPU device per process. Without an initialized SDL video
 * subsystem, sigpu initializes it with the offscreen driver, so an
 * application that opens windows imports siwindow first. Changing the
 * descriptor reopens the device; removing it releases every GPU object and
 * puts the resources back to Pending until a device comes back.
 */
ECS_COMPONENT_DECLARE(GpuDevice, {
    bool debug;
    /* Submissions the CPU may run ahead of the GPU; 0 selects 2. */
    uint8_t frames_in_flight;
});

/*
 * Pending -> Waiting(dependency) -> Ready, or Failed(error, dependency,
 * message). A change of the descriptor puts the resource back to Pending.
 * Retiring: the descriptor was removed and its GPU objects released (SDL
 * keeps them alive until the submissions that use them completed).
 *
 * Waiting and Failed resources are only processed again when their own
 * descriptor or a dependency changes. sigpu calls ecs_modified on every
 * status change it makes during GpuSync, so an OnSet observer on
 * GpuResourceState sees them.
 */
typedef enum {
    GpuPending = 0,
    GpuWaiting = 1,
    GpuReady = 2,
    GpuFailed = 3,
    GpuRetiring = 4
} GpuStatus;

typedef enum {
    GpuErrorNone = 0,
    /* The descriptor is rejected; the message names the field. */
    GpuErrorInvalid = 1,
    /* A referenced entity is dead or is not the expected kind of resource. */
    GpuErrorDependency = 2,
    /* GpuTexture.size_from loops back to the texture. */
    GpuErrorCycle = 3,
    /* Shader pack missing, malformed or with an unexpected fingerprint. */
    GpuErrorPack = 4,
    /* Program entry point missing from the pack or of the wrong stage. */
    GpuErrorEntry = 5,
    /* SDL refused to create the object. */
    GpuErrorSdl = 6,
    /* Second device, device creation failure or device lost. */
    GpuErrorDevice = 7,
    /* A render graph or a material does not match what it refers to. */
    GpuErrorInterface = 8
} GpuError;

#define GPU_STATE_MESSAGE_SIZE 96

/*
 * Written by sigpu only. requested counts descriptor changes, applied is the
 * requested value the current GPU objects were built from, allocation grows
 * each time new GPU objects replace the previous ones. queued and record are
 * internal.
 */
ECS_COMPONENT_DECLARE(GpuResourceState, {
    uint8_t status;
    uint8_t error;
    bool queued;
    uint32_t requested;
    uint32_t applied;
    uint32_t allocation;
    ecs_entity_t dependency;
    uint32_t record;
    char message[96];
});

typedef enum {
    GpuBufferVertex = 1 << 0,
    GpuBufferIndex = 1 << 1,
    GpuBufferIndirect = 1 << 2,
    /* Storage buffer read by the vertex or fragment stage. */
    GpuBufferGraphicsRead = 1 << 3,
    GpuBufferComputeRead = 1 << 4,
    GpuBufferComputeWrite = 1 << 5
} GpuBufferUsage;

typedef enum {
    /* Uploads write ranges in place; the GPU finishes reading the previous
     * contents first. */
    GpuUploadInPlace = 0,
    /* Every upload replaces the whole buffer and SDL cycles it, so frames in
     * flight keep reading the previous contents without a stall. */
    GpuUploadStream = 1
} GpuUploadPolicy;

/* size in bytes, multiple of 4. usage combines GpuBufferUsage flags. */
ECS_COMPONENT_DECLARE(GpuBuffer, {
    uint32_t size;
    uint32_t usage;
    uint32_t policy;
});

typedef enum {
    GpuFormatInvalid = 0,
    GpuFormatRgba8,
    GpuFormatRgba8Srgb,
    GpuFormatBgra8,
    GpuFormatBgra8Srgb,
    GpuFormatR8,
    GpuFormatRg8,
    GpuFormatR16f,
    GpuFormatRg16f,
    GpuFormatRgba16f,
    GpuFormatR32f,
    GpuFormatRg32f,
    GpuFormatRgba32f,
    GpuFormatR32u,
    GpuFormatRg11b10f,
    GpuFormatD16,
    GpuFormatD24S8,
    GpuFormatD32f,
    GpuFormatD32fS8,
    GpuFormatCount
} GpuFormat;

typedef enum {
    GpuTexture2d = 0,
    GpuTexture2dArray = 1,
    GpuTexture3d = 2,
    GpuTextureCube = 3
} GpuTextureDimension;

typedef enum {
    GpuTextureSampled = 1 << 0,
    GpuTextureColorTarget = 1 << 1,
    GpuTextureDepthTarget = 1 << 2,
    /* Storage texture read by the vertex or fragment stage. */
    GpuTextureGraphicsRead = 1 << 3,
    GpuTextureComputeRead = 1 << 4,
    GpuTextureComputeWrite = 1 << 5
} GpuTextureUsage;

/*
 * With size_from set, width and height follow that texture's or surface's
 * size times scale (0 selects 1), rounded to the nearest integer and at
 * least 1; the texture waits for its source and is rebuilt when the source
 * is reallocated or resized. depth is the layer count of arrays and the
 * depth of 3D textures (0 selects 1); cubes have 6 layers. mips and
 * samples: 0 selects 1.
 */
ECS_COMPONENT_DECLARE(GpuTexture, {
    uint8_t dimension;
    uint8_t format;
    uint8_t mips;
    uint8_t samples;
    uint32_t usage;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    ecs_entity_t size_from;
    float scale;
});

typedef enum { GpuFilterNearest = 0, GpuFilterLinear = 1 } GpuFilter;
typedef enum { GpuAddressRepeat = 0, GpuAddressMirror = 1, GpuAddressClamp = 2 } GpuAddress;
typedef enum {
    GpuCompareNone = 0,
    GpuCompareNever,
    GpuCompareLess,
    GpuCompareEqual,
    GpuCompareLessEqual,
    GpuCompareGreater,
    GpuCompareNotEqual,
    GpuCompareGreaterEqual,
    GpuCompareAlways
} GpuCompare;

/* max_anisotropy: 0 or 1 disables anisotropic filtering. */
ECS_COMPONENT_DECLARE(GpuSampler, {
    uint8_t min_filter;
    uint8_t mag_filter;
    uint8_t mip_filter;
    uint8_t address_u;
    uint8_t address_v;
    uint8_t address_w;
    uint8_t compare;
    uint8_t max_anisotropy;
});

#define GPU_PATH_SIZE 192

/*
 * A .gpushader pack built by tools/gpushader. path names an asset registered
 * with sigpu_asset_register, or else a file. fingerprint is the FNV-1a
 * 64-bit hash of the pack; 0 accepts any pack.
 */
ECS_COMPONENT_DECLARE(GpuShader, {
    char path[192];
    uint64_t fingerprint;
});

#define GPU_ENTRY_SIZE 64
#define GPU_PROGRAM_VARIANTS 3

typedef enum { GpuFaceNone = 0, GpuFaceBack = 1, GpuFaceFront = 2 } GpuFace;
typedef enum {
    GpuBlendNone = 0,
    /* src * a + dst * (1 - a) */
    GpuBlendAlpha = 1,
    GpuBlendAdditive = 2,
    GpuBlendPremultiplied = 3
} GpuBlend;
typedef enum {
    GpuTopologyTriangles = 0,
    GpuTopologyTriangleStrip = 1,
    GpuTopologyLines = 2,
    GpuTopologyLineStrip = 3,
    GpuTopologyPoints = 4
} GpuTopology;

/*
 * Fixed-function state of a graphics program. The zero value draws
 * triangles without culling, depth test or blending. depth_compare is a
 * GpuCompare (None disables the test); front_clockwise selects the front
 * face winding; depth_bias and slope_bias offset the depth written (shadow
 * maps).
 */
SIREFLECT_STRUCT(GpuRaster, {
    uint8_t cull;
    uint8_t front_clockwise;
    uint8_t depth_compare;
    uint8_t depth_write;
    uint8_t blend;
    uint8_t topology;
    uint8_t reserved[2];
    float depth_bias;
    float slope_bias;
});

/* Entries and state used when the program draws in phase. An empty
 * fragment entry draws depth only. */
SIREFLECT_STRUCT(GpuProgramVariant, {
    ecs_entity_t phase;
    char vertex[64];
    char fragment[64];
    GpuRaster raster;
});

/*
 * Entry points of a shader pack: vertex and fragment for a graphics
 * program, or compute alone. A graphics program draws in its own phase (0:
 * custom passes only) with vertex, fragment and raster, and in the phase of
 * each variant with the variant's entries and raster. Graphics phases are
 * plain identity entities (opaque, shadow...), named by GpuPass.phase.
 * Pipelines are derived per pass formats and vertex layout and cached.
 */
ECS_COMPONENT_DECLARE(GpuProgram, {
    ecs_entity_t shader;
    char vertex[64];
    char fragment[64];
    char compute[64];
    ecs_entity_t phase;
    GpuRaster raster;
    GpuProgramVariant variants[3];
});

typedef enum {
    GpuVertexInvalid = 0,
    GpuVertexFloat,
    GpuVertexFloat2,
    GpuVertexFloat3,
    GpuVertexFloat4,
    GpuVertexUint,
    GpuVertexUint2,
    GpuVertexUint4,
    GpuVertexInt,
    GpuVertexUbyte4,
    GpuVertexUbyte4Norm,
    GpuVertexByte4Norm,
    GpuVertexShort2Norm,
    GpuVertexShort4Norm,
    GpuVertexHalf2,
    GpuVertexHalf4,
    GpuVertexFormatCount
} GpuVertexFormat;

/* A vertex attribute, matched by name with the vertex inputs of a program
 * (the name of the entry point parameter or struct field). */
SIREFLECT_STRUCT(GpuVertexAttribute, {
    char name[24];
    uint8_t format;
    uint8_t buffer;
    uint16_t offset;
});

/* One level of detail: a range of the index buffer (of the vertex buffer
 * without indices), drawn while the projected radius stays under
 * max_pixels; the last level has max_pixels 0 and no limit. */
SIREFLECT_STRUCT(GpuGeometryLod, {
    uint32_t first_index;
    uint32_t index_count;
    int32_t vertex_offset;
    float max_pixels;
});

#define GPU_GEOMETRY_ATTRIBUTES 8
#define GPU_GEOMETRY_LODS 4

/*
 * Vertices and indices drawn by a feature. vertices and indices are
 * GpuBuffer entities (indices may be 0); index_size is 2 or 4. The
 * attributes read buffer 0. bounds is the local bounding sphere (center,
 * radius), scaled by the instance bounds the feature writes. Ready once its
 * buffers are.
 */
ECS_COMPONENT_DECLARE(GpuGeometry, {
    ecs_entity_t vertices;
    ecs_entity_t indices;
    uint32_t stride;
    uint8_t index_size;
    uint8_t topology;
    uint8_t lod_count;
    uint8_t attribute_count;
    GpuVertexAttribute attributes[8];
    GpuGeometryLod lods[4];
    float bounds[4];
});

typedef enum {
    GpuPresentVsync = 0,
    GpuPresentImmediate = 1,
    GpuPresentMailbox = 2
} GpuPresentMode;

/*
 * The swapchain of the siwindow window whose Window.slot is window_slot.
 * Waiting until that window exists. Its size is the window size in pixels;
 * textures follow it with size_from. Every surface gets a render graph and
 * a color target (GpuSurfaceTargets) cleared to clear, then presented with
 * the encoding the swapchain needs.
 */
ECS_COMPONENT_DECLARE(GpuSurface, {
    char window_slot[32];
    uint8_t present_mode;
    float clear[4];
});

/* Written by sigpu on surface entities: the surface's graph and its color
 * target, an Rgba8Srgb texture of the surface size, sampled by the present
 * pass. Passes that compose the image write color between GPU_ORDER_CLEAR
 * and GPU_ORDER_PRESENT. */
ECS_COMPONENT_DECLARE(GpuSurfaceTargets, {
    ecs_entity_t graph;
    ecs_entity_t color;
    ecs_entity_t clear_pass;
    ecs_entity_t present_pass;
});

/* ----------------------------------------------------------------------
 * Render graphs (RFC section 8)
 * ---------------------------------------------------------------------- */

/* Order bands shared by the passes of a graph. */
#define GPU_ORDER_CLEAR 0
#define GPU_ORDER_SHADOW 1000
#define GPU_ORDER_SCENE 2000
#define GPU_ORDER_POST 3000
#define GPU_ORDER_UI 4000
#define GPU_ORDER_PRESENT 5000

#define GPU_PASS_MAX_ACCESSES 16
#define GPU_GRAPH_EXPORTS 8

/*
 * Graphs record in increasing order. exports lists resources whose content
 * is read after the graph (readbacks, another graph): the passes writing
 * them are kept.
 */
ECS_COMPONENT_DECLARE(GpuRenderGraph, {
    int32_t order;
    bool disabled;
    ecs_entity_t exports[8];
});

/*
 * Written by sigpu. Ready with the compiled plan, Failed with the reason and
 * the offending pass (the graph then records nothing), Waiting while a
 * resource it uses is not Ready. revision counts compilations.
 */
ECS_COMPONENT_DECLARE(GpuGraphState, {
    uint8_t status;
    uint8_t error;
    uint16_t passes;
    uint16_t culled;
    uint32_t revision;
    ecs_entity_t pass;
    char message[96];
});

typedef enum {
    /* Color and depth attachments; draws features of phase for view, then
     * calls the execute callback. */
    GpuPassRender = 0,
    /* Storage writes are bound at the start of the pass. */
    GpuPassCompute = 1,
    GpuPassCopy = 2,
    /* Samples items[0] (a texture) into items[1] (a surface). */
    GpuPassPresent = 3
} GpuPassKind;

/*
 * A pass of a graph, ChildOf the graph. side_effect keeps a pass that
 * writes nothing the graph reads. program is the pass's own program, bound
 * before the callback. phase and view select the feature draws of a render
 * pass. viewport is normalized (x, y, width, height); zero covers the
 * attachments.
 */
ECS_COMPONENT_DECLARE(GpuPass, {
    uint8_t kind;
    bool disabled;
    bool side_effect;
    int32_t order;
    ecs_entity_t program;
    ecs_entity_t phase;
    ecs_entity_t view;
    float viewport[4];
});

typedef enum {
    GpuUseNone = 0,
    GpuUseColor,
    GpuUseDepth,
    GpuUseSampled,
    GpuUseStorageRead,
    GpuUseStorageWrite,
    GpuUseStorageReadWrite,
    GpuUseVertex,
    GpuUseIndex,
    GpuUseIndirect,
    GpuUseCopySrc,
    GpuUseCopyDst,
    GpuUsageCount
} GpuUsage;

typedef enum { GpuLoadClear = 0, GpuLoadLoad = 1, GpuLoadDontCare = 2 } GpuLoad;
typedef enum { GpuStoreStore = 0, GpuStoreDontCare = 1 } GpuStore;

/*
 * One resource a pass reads or writes. aux is the resolve texture of a
 * multisampled color attachment, or the sampler of a sampled texture.
 * clear is the clear color, clear[0] the clear depth. layer and mip select
 * the attachment subresource; offset and size a buffer range (size 0: to
 * the end). binding is the shader resource name the pass binds it to.
 */
SIREFLECT_STRUCT(GpuAccess, {
    ecs_entity_t resource;
    ecs_entity_t aux;
    float clear[4];
    uint32_t offset;
    uint32_t size;
    uint16_t layer;
    uint8_t mip;
    uint8_t usage;
    uint8_t load;
    uint8_t store;
    uint8_t reserved[2];
    char binding[32];
});

ECS_COMPONENT_DECLARE(GpuPassIO, {
    uint32_t count;
    GpuAccess items[16];
});

/* A view culled and drawn by the geometry passes that name it. lod_scale
 * converts a world radius at distance 1 (perspective) or a world radius
 * (orthographic) to pixels. cache lets a pass keep its attachments from the
 * previous frame when neither the view nor its content changed (shadow
 * cascades). Written by the renderer every frame the view moves. */
ECS_COMPONENT_DECLARE(GpuView, {
    float view_projection[16];
    float position[3];
    float forward[3];
    float lod_scale;
    bool orthographic;
    bool cache;
});

/* ----------------------------------------------------------------------
 * Draw features (RFC section 6)
 * ---------------------------------------------------------------------- */

typedef enum { GpuCullNone = 0, GpuCullSphere = 1 } GpuCulling;
typedef enum {
    /* Opaque and alpha-tested: grouped by pipeline, geometry, LOD, textures. */
    GpuOrderBatched = 0,
    /* Transparent: back to front per view; adjacent compatible items merge. */
    GpuOrderSorted = 1,
    /* UI: by the key sigpu_order_keys gives, across the layered features of
     * a pass, then insertion; adjacent compatible items merge. */
    GpuOrderLayered = 2
} GpuOrdering;

typedef struct {
    float x;
    float y;
    float z;
    float radius;
} GpuSphere;

/* Counters of the last frame for a view (GpuView), written by sigpu. */
ECS_COMPONENT_DECLARE(GpuViewStats, {
    uint32_t candidates;
    uint32_t visible;
    uint32_t cells_tested;
    uint32_t batches;
});

/* Inspectable description of a feature, set by sigpu_feature. */
ECS_COMPONENT_DECLARE(GpuFeature, {
    uint32_t instance_size;
    uint8_t culling;
    uint8_t ordering;
    ecs_entity_t geometry;
    ecs_entity_t material;
    ecs_entity_t program;
    float cell_size;
});

/* Geometry and material of a drawable (RFC section 4.2). */
ECS_RELATION_DECLARE(DrawGeometry);
ECS_RELATION_DECLARE(DrawMaterial);

typedef struct {
    const char *name;
    /* Component written per row by extract, read by the shader's instance
     * storage buffer (scope "instance") with the same layout. */
    ecs_component_t instance;
    /* Rows to draw. Read terms should be ecs_in_changed so unchanged tables
     * are skipped; DrawGeometry and DrawMaterial are added as optional
     * relations when missing. */
    ecs_query_desc_t query;
    /* Called per portion of a changed table, possibly concurrently. */
    void (*extract)(ecs_iter_t *it);
    /* Used when a row has no DrawGeometry / DrawMaterial. */
    ecs_entity_t geometry;
    ecs_entity_t material;
    /* Program of materials without a parameter component. */
    ecs_entity_t program;
    uint8_t culling;
    uint8_t ordering;
    /* Edge of the cells of stable ranges; 0 selects 32. */
    float cell_size;
    /* Rows per extraction portion; 0 selects 1024. */
    uint32_t chunk_size;
    /* Read by extract with sigpu_feature_data; user_data_dtor runs once
     * when the world ends. */
    void *user_data;
    void (*user_data_dtor)(void *user_data);
} GpuFeatureDesc;

/* ----------------------------------------------------------------------
 * Statistics and events
 * ---------------------------------------------------------------------- */

/* The open device, if any. driver is the SDL GPU driver ("vulkan"...). */
ECS_RESOURCE_DECLARE(GpuSession, {
    ecs_entity_t device;
    bool ready;
    bool lost;
    uint8_t frames_in_flight;
    char driver[32];
    uint64_t submissions;
});

/*
 * Counters for diagnostics and invariant tests (RFC section 18). objects is
 * the number of live SDL GPU objects sigpu owns, memory_reserved the bytes
 * of its internal buffers; the others count the current frame and are reset
 * at the start of every GpuSync.
 */
ECS_RESOURCE_DECLARE(GpuStats, {
    uint32_t processed;
    uint32_t deferred;
    uint32_t created;
    uint32_t create_failures;
    uint32_t released;
    uint32_t objects;
    uint32_t uploads;
    uint64_t upload_bytes;
    uint32_t submissions;
    uint32_t frame_waits;
    uint32_t graph_compiles;
    uint32_t passes;
    uint32_t passes_culled;
    uint32_t passes_suspended;
    uint32_t passes_reused;
    uint32_t tables_extracted;
    uint32_t tables_skipped;
    uint32_t instance_uploads;
    uint64_t instance_upload_bytes;
    uint32_t range_growths;
    uint32_t partitions_built;
    uint32_t candidates;
    uint32_t visible;
    uint32_t cells_tested;
    uint32_t batches;
    /* Rows or batches not drawn: material or geometry not Ready, pipeline
     * not created yet (budget) or failed, instance layout not matching. */
    uint32_t batches_skipped;
    uint32_t skipped_material;
    uint32_t skipped_geometry;
    uint32_t skipped_pipeline;
    uint32_t skipped_layout;
    uint32_t draws;
    uint32_t pipeline_binds;
    uint32_t pipelines_created;
    uint32_t pipeline_hits;
    uint32_t pipeline_misses;
    uint32_t materials_prepared;
    uint64_t memory_reserved;
    uint64_t memory_used;
    uint64_t sync_ns;
    uint64_t extract_ns;
    uint64_t prepare_ns;
    uint64_t record_ns;
});

/* Events emitted on the device entity. */
enum { GpuDeviceLost = 0, GpuEventCount };

typedef struct {
    uint32_t mip;
    uint32_t layer;
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} GpuTextureRegion;

/* ----------------------------------------------------------------------
 * Pass callbacks (RFC section 8.4)
 * ---------------------------------------------------------------------- */

/* Valid during the callback only. */
typedef struct gpu_pass_ctx_s gpu_pass_ctx_t;
typedef void (*gpu_pass_fn)(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data);
typedef void (*gpu_dtor_fn)(void *user_data);

typedef struct {
    const char *name;
    uint8_t kind;
    int32_t order;
    bool side_effect;
    ecs_entity_t program;
    ecs_entity_t phase;
    ecs_entity_t view;
    gpu_pass_fn execute;
    void *user_data;
    gpu_dtor_fn user_data_dtor;
} GpuPassDesc;

/* Layout of the vertex buffers a custom pass binds, matched by name with
 * the program's vertex inputs. */
#define GPU_LAYOUT_ATTRIBUTES 16
typedef struct {
    uint32_t strides[2];
    bool instanced[2];
    uint32_t attribute_count;
    GpuVertexAttribute attributes[16];
} GpuVertexLayout;

#ifdef __cplusplus
extern "C" {
#endif
/* Event id for one of the Gpu* event kinds. */
SIECS_PUBLIC_API ecs_event_t sigpu_event_id(uint32_t kind);

/* True when the entity's resource is Ready. */
SIECS_PUBLIC_API bool sigpu_ready(ecs_entity_t resource);

/* Bytes per texel of a color format, or per depth/stencil sample. */
SIECS_PUBLIC_API uint32_t sigpu_format_size(uint8_t format);

/* The largest sample count up to wanted that the open device supports for
 * a render target of format; 1 without a device. */
SIECS_PUBLIC_API uint32_t sigpu_max_samples(uint8_t format, uint32_t wanted);

/*
 * Copies data under a name that GpuShader.path can use. Registering a name
 * again replaces the data and queues the shaders that use it. False when the
 * name is empty or too long, or on allocation failure.
 */
SIECS_PUBLIC_API bool sigpu_asset_register(const char *name, const void *data, size_t size);

/*
 * Copies data and writes it at offset into the buffer during the next
 * GpuRecord after the buffer is Ready. A GpuUploadStream buffer only takes
 * whole-buffer uploads. The write applies to the allocation
 * current at that time; it is dropped if the buffer fails, is removed, or
 * no longer contains the range. False when the entity has no GpuBuffer or
 * the range is outside its current size.
 */
SIECS_PUBLIC_API bool sigpu_buffer_upload(
    ecs_entity_t buffer,
    uint32_t offset,
    const void *data,
    uint32_t size
);

/*
 * Same for a texture region. pitch is the byte distance between two rows
 * of data, 0 for tightly packed rows; it must be a multiple of the texel
 * size. size must cover the region.
 */
SIECS_PUBLIC_API bool sigpu_texture_upload(
    ecs_entity_t texture,
    const GpuTextureRegion *region,
    const void *data,
    size_t size,
    uint32_t pitch
);

/*
 * Diagnostics: waits for the GPU, then saves an RGBA8 texture (a surface's
 * color target for a surface) as a BMP file. False when the texture is not
 * Ready or not 8-bit RGBA. With SIGPU_CAPTURE=<path> in the environment,
 * sigpu saves every surface after frame SIGPU_CAPTURE_FRAME (default 60),
 * then stops the world when SIGPU_CAPTURE_QUIT is set.
 */
SIECS_PUBLIC_API bool sigpu_capture_bmp(ecs_entity_t resource, const char *path);

/*
 * Lifts the per-frame budgets for the next frame: every queued resource is
 * built in GpuSync and every pipeline the batches need is created in
 * GpuPrepare, so the frames after it run without creation (RFC 9.1).
 */
SIECS_PUBLIC_API void sigpu_prewarm(void);

/* Size in pixels of a Ready surface or texture; false otherwise. */
SIECS_PUBLIC_API bool sigpu_extent(ecs_entity_t resource, uint32_t *width, uint32_t *height);

/* A graph entity with GpuRenderGraph, named under parent (0: root). */
SIECS_PUBLIC_API ecs_entity_t sigpu_graph(ecs_entity_t parent, const char *name, int32_t order);

/* A pass of graph built from desc, with an empty GpuPassIO. */
SIECS_PUBLIC_API ecs_entity_t sigpu_pass(ecs_entity_t graph, const GpuPassDesc *desc);

/*
 * Sets the callback a pass calls while it records. user_data_dtor runs
 * exactly once: when the callback is replaced, the pass loses GpuPass or
 * dies, or the world ends.
 */
SIECS_PUBLIC_API void sigpu_pass_execute(
    ecs_entity_t pass,
    gpu_pass_fn execute,
    void *user_data,
    gpu_dtor_fn user_data_dtor
);

/* Appends an access to the pass's GpuPassIO; false when it is full. */
SIECS_PUBLIC_API bool sigpu_pass_access(ecs_entity_t pass, const GpuAccess *access);
SIECS_PUBLIC_API bool sigpu_pass_color(
    ecs_entity_t pass,
    ecs_entity_t texture,
    uint8_t load,
    uint8_t store,
    const float clear[4],
    ecs_entity_t resolve
);
SIECS_PUBLIC_API bool sigpu_pass_depth(
    ecs_entity_t pass,
    ecs_entity_t texture,
    uint8_t load,
    uint8_t store,
    float clear_depth,
    uint16_t layer
);
SIECS_PUBLIC_API bool sigpu_pass_sample(
    ecs_entity_t pass,
    const char *binding,
    ecs_entity_t texture,
    ecs_entity_t sampler
);
SIECS_PUBLIC_API bool sigpu_pass_storage_read(
    ecs_entity_t pass,
    const char *binding,
    ecs_entity_t resource
);
SIECS_PUBLIC_API bool sigpu_pass_storage_write(
    ecs_entity_t pass,
    const char *binding,
    ecs_entity_t resource
);
/* Keeps the passes that write resource; false when exports is full. */
SIECS_PUBLIC_API bool sigpu_graph_export(ecs_entity_t graph, ecs_entity_t resource);

/* Commands of a pass callback. Names are shader resource names; a name
 * the bound program does not use is ignored. */
SIECS_PUBLIC_API void gpu_pass_extent(const gpu_pass_ctx_t *ctx, uint32_t *width, uint32_t *height);
SIECS_PUBLIC_API uint64_t gpu_pass_frame(const gpu_pass_ctx_t *ctx);
/* Binds a program (pipeline for the pass's targets), then the accesses of
 * the pass that name a binding and the uniforms and textures set so far. */
SIECS_PUBLIC_API bool gpu_cmd_program(
    gpu_pass_ctx_t *ctx,
    ecs_entity_t program,
    const GpuVertexLayout *layout
);
/* Uniform block by name. The data is kept for the rest of the pass and
 * rebound with every later program, feature draws included. */
SIECS_PUBLIC_API void gpu_cmd_uniform(
    gpu_pass_ctx_t *ctx,
    const char *name,
    const void *data,
    uint32_t size
);
SIECS_PUBLIC_API void gpu_cmd_texture(
    gpu_pass_ctx_t *ctx,
    const char *name,
    ecs_entity_t texture,
    ecs_entity_t sampler
);
SIECS_PUBLIC_API void gpu_cmd_storage(gpu_pass_ctx_t *ctx, const char *name, ecs_entity_t resource);
SIECS_PUBLIC_API void gpu_cmd_vertex_buffer(
    gpu_pass_ctx_t *ctx,
    uint32_t slot,
    ecs_entity_t buffer,
    uint32_t offset
);
SIECS_PUBLIC_API void gpu_cmd_index_buffer(
    gpu_pass_ctx_t *ctx,
    ecs_entity_t buffer,
    uint32_t offset,
    uint8_t index_size
);
SIECS_PUBLIC_API void gpu_cmd_viewport(gpu_pass_ctx_t *ctx, float x, float y, float w, float h);
SIECS_PUBLIC_API void gpu_cmd_scissor(gpu_pass_ctx_t *ctx, int32_t x, int32_t y, int32_t w, int32_t h);
SIECS_PUBLIC_API void gpu_cmd_draw(
    gpu_pass_ctx_t *ctx,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance
);
SIECS_PUBLIC_API void gpu_cmd_draw_indexed(
    gpu_pass_ctx_t *ctx,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance
);
SIECS_PUBLIC_API void gpu_cmd_dispatch(gpu_pass_ctx_t *ctx, uint32_t x, uint32_t y, uint32_t z);
/* Enough groups of the bound compute program to cover width x height. */
SIECS_PUBLIC_API void gpu_cmd_dispatch_cover(gpu_pass_ctx_t *ctx, uint32_t width, uint32_t height);
SIECS_PUBLIC_API void gpu_cmd_copy_buffer(
    gpu_pass_ctx_t *ctx,
    ecs_entity_t source,
    uint32_t source_offset,
    ecs_entity_t destination,
    uint32_t destination_offset,
    uint32_t size
);
/* Copies mip 0, layer 0 of source into destination, of the same size. */
SIECS_PUBLIC_API void gpu_cmd_copy_texture(
    gpu_pass_ctx_t *ctx,
    ecs_entity_t source,
    ecs_entity_t destination
);

/* A feature: its GpuFeature entity, named desc->name, owning its systems. */
SIECS_PUBLIC_API ecs_entity_t sigpu_feature(const GpuFeatureDesc *desc);
/* Rows of the current portion in the feature's storage: instances, bounds
 * (NULL without culling) and order keys (NULL unless layered). */
SIECS_PUBLIC_API void *sigpu_instances(ecs_iter_t *it);
SIECS_PUBLIC_API GpuSphere *sigpu_bounds(ecs_iter_t *it);
SIECS_PUBLIC_API uint32_t *sigpu_order_keys(ecs_iter_t *it);
/* GpuFeatureDesc.user_data of the feature being extracted. */
SIECS_PUBLIC_API void *sigpu_feature_data(ecs_iter_t *it);

/* Exact test of a candidate; sets *distance and returns true on a hit. */
typedef bool (*gpu_pick_fn)(ecs_entity_t entity, float *distance, void *user_data);
/*
 * Nearest entity of the feature whose bounds the ray crosses and exact
 * accepts, from the bounds extracted last frame; 0 when none. direction is
 * normalized. Equal distances keep the smallest entity.
 */
SIECS_PUBLIC_API ecs_entity_t sigpu_feature_raycast(
    ecs_entity_t feature,
    const float origin[3],
    const float direction[3],
    gpu_pick_fn exact,
    void *user_data,
    float *distance
);

/*
 * Declares params as the parameter component of program (RFC section 7.1):
 * an entity with params is a material drawn with program. The component is
 * checked against the material blocks of the program (uniforms of the
 * storage buffer of scope "material", and its textures): fields are matched
 * by name; a texture field is an ecs_entity_t (0: a white texture), its
 * sampler the field <name>_sampler when present (0: linear, clamped). A
 * dead or unready texture keeps the material Waiting. Materials made before
 * the call are picked up. A field that cannot be packed fails the materials
 * with GpuErrorInterface and the field's name.
 */
SIECS_PUBLIC_API bool sigpu_program_params(ecs_entity_t program, ecs_component_t params);
#ifdef __cplusplus
}
#endif
#endif
