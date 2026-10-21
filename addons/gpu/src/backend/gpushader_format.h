#ifndef SIGPU_GPUSHADER_FORMAT_H
#define SIGPU_GPUSHADER_FORMAT_H

/*
 * Layout of a .gpushader pack, shared by the pack tool and the runtime loader.
 * Every field is a little-endian uint32_t or a zero-padded char array, so the
 * structs have no padding and the file is the same on every host.
 *
 * File: header, entries, resources, fields, inputs, then the code blobs. An
 * entry's code_offset counts from the start of the file and is 4-aligned.
 */

#include <stdint.h>

#define GPU_PACK_MAGIC "SIGPUPAK"
#define GPU_PACK_VERSION 1u
#define GPU_PACK_NAME_SIZE 64
#define GPU_PACK_TOOLCHAIN_SIZE 32

enum { GpuPackVertex = 0, GpuPackFragment = 1, GpuPackCompute = 2 };
enum { GpuPackSpirv = 1 };

/* Resource kinds in SDL binding order. Graphics stages only use the first
 * four, read-only; compute adds the read-write ones. */
enum {
    GpuPackSampler = 0,
    GpuPackStorageTexture = 1,
    GpuPackStorageBuffer = 2,
    GpuPackUniformBuffer = 3,
    GpuPackRWStorageTexture = 4,
    GpuPackRWStorageBuffer = 5,
    GpuPackKindCount = 6
};

/* Set with the [Scope("...")] attribute on the global. */
enum {
    GpuPackScopeNone = 0,
    GpuPackScopeView = 1,
    GpuPackScopeMaterial = 2,
    GpuPackScopeInstance = 3,
    GpuPackScopePass = 4
};

enum {
    GpuPackScalarNone = 0,
    GpuPackScalarBool = 1,
    GpuPackScalarInt32 = 2,
    GpuPackScalarUInt32 = 3,
    GpuPackScalarFloat16 = 4,
    GpuPackScalarFloat32 = 5
};

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t entry_count;
    uint32_t resource_count;
    uint32_t field_count;
    uint32_t input_count;
    uint32_t file_size;
    char toolchain[GPU_PACK_TOOLCHAIN_SIZE];
} gpu_pack_header_t;

typedef struct {
    char name[GPU_PACK_NAME_SIZE];
    uint32_t stage;
    uint32_t target;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t first_resource;
    uint32_t resource_count;
    uint32_t first_input;
    uint32_t input_count;
    uint32_t threads[3];
    uint32_t counts[GpuPackKindCount];
} gpu_pack_entry_t;

/* slot is the index passed to SDL_BindGPU*; set and binding are the SPIR-V
 * decorations it was lowered to. stride is the element size of a storage
 * buffer, size the block size of a uniform buffer. */
typedef struct {
    char name[GPU_PACK_NAME_SIZE];
    uint32_t kind;
    uint32_t scope;
    uint32_t slot;
    uint32_t set;
    uint32_t binding;
    uint32_t stride;
    uint32_t size;
    uint32_t first_field;
    uint32_t field_count;
} gpu_pack_resource_t;

typedef struct {
    char name[GPU_PACK_NAME_SIZE];
    uint32_t offset;
    uint32_t size;
    uint32_t scalar;
    uint32_t rows;
    uint32_t columns;
} gpu_pack_field_t;

typedef struct {
    char name[GPU_PACK_NAME_SIZE];
    uint32_t location;
    uint32_t scalar;
    uint32_t components;
} gpu_pack_input_t;

#endif
