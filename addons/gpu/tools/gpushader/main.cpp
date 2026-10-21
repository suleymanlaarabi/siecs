/*
 * gpushader: compiles a Slang file into a .gpushader pack (format in
 * src/backend/gpushader_format.h). Each entry point is compiled on its own,
 * its resources are lowered to the SDL GPU binding convention, and the
 * reflected interface is stored next to the code.
 *
 * Author convention, enforced here:
 * - resources are globals; loose uniforms go in a ConstantBuffer<T>;
 * - textures are combined samplers (Sampler2D...), separate SamplerState is
 *   not supported by SDL;
 * - vertex and fragment stages only read storage textures and buffers;
 * - [Scope("view" | "material" | "instance" | "pass")] tags a global.
 *
 * Usage: gpushader <input.slang> -o <output.gpushader>
 *        gpushader --dump <pack.gpushader>
 */

#include "backend/gpushader_format.h"

#include <slang-com-ptr.h>
#include <slang.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

static_assert(sizeof(gpu_pack_header_t) == 8 + 6 * 4 + GPU_PACK_TOOLCHAIN_SIZE);
static_assert(sizeof(gpu_pack_entry_t) == GPU_PACK_NAME_SIZE + 4 * (11 + GpuPackKindCount));

static const char *input_path = "";

[[noreturn]] static void fail(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::fprintf(stderr, "gpushader: %s: ", input_path);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
    va_end(args);
    std::exit(1);
}

static void report(slang::IBlob *diagnostics) {
    if (diagnostics && diagnostics->getBufferSize()) {
        std::fprintf(stderr, "%s", (const char *)diagnostics->getBufferPointer());
    }
}

static void copy_name(char (&out)[GPU_PACK_NAME_SIZE], const char *name) {
    if (!name) {
        name = "";
    }
    if (std::strlen(name) >= GPU_PACK_NAME_SIZE) {
        fail("name '%s' is longer than %d bytes", name, GPU_PACK_NAME_SIZE - 1);
    }
    std::memset(out, 0, sizeof(out));
    std::memcpy(out, name, std::strlen(name));
}

static uint32_t scalar_of(slang::TypeReflection::ScalarType scalar) {
    using S = slang::TypeReflection::ScalarType;
    switch (scalar) {
    case S::Bool:
        return GpuPackScalarBool;
    case S::Int32:
        return GpuPackScalarInt32;
    case S::UInt32:
        return GpuPackScalarUInt32;
    case S::Float16:
        return GpuPackScalarFloat16;
    case S::Float32:
        return GpuPackScalarFloat32;
    default:
        return GpuPackScalarNone;
    }
}

static const char *stage_name(uint32_t stage) {
    switch (stage) {
    case GpuPackVertex:
        return "vertex";
    case GpuPackFragment:
        return "fragment";
    default:
        return "compute";
    }
}

static uint32_t scope_of(slang::VariableReflection *variable) {
    for (unsigned i = 0; i < variable->getUserAttributeCount(); i++) {
        slang::Attribute *attribute = variable->getUserAttributeByIndex(i);
        if (std::strcmp(attribute->getName(), "Scope") != 0) {
            continue;
        }
        size_t size = 0;
        const char *value = attribute->getArgumentValueString(0, &size);
        std::string scope = value ? std::string(value, size) : std::string();
        if (scope == "view") {
            return GpuPackScopeView;
        }
        if (scope == "material") {
            return GpuPackScopeMaterial;
        }
        if (scope == "instance") {
            return GpuPackScopeInstance;
        }
        if (scope == "pass") {
            return GpuPackScopePass;
        }
        fail("'%s': unknown scope \"%s\"", variable->getName(), scope.c_str());
    }
    return GpuPackScopeNone;
}

/* Kind of a global resource, or fails with the reason it cannot be lowered. */
static uint32_t kind_of(slang::VariableLayoutReflection *param, uint32_t stage) {
    slang::TypeLayoutReflection *type = param->getTypeLayout();
    const char *name = param->getName();
    switch (type->getKind()) {
    case slang::TypeReflection::Kind::ConstantBuffer:
        return GpuPackUniformBuffer;
    case slang::TypeReflection::Kind::SamplerState:
        fail("'%s': separate samplers are not supported by SDL, use a combined Sampler2D", name);
    case slang::TypeReflection::Kind::Resource:
        break;
    default:
        fail("'%s': unsupported global, wrap uniforms in a ConstantBuffer<T>", name);
    }
    const SlangResourceShape shape = type->getResourceShape();
    const SlangResourceShape base = SlangResourceShape(shape & SLANG_RESOURCE_BASE_SHAPE_MASK);
    const SlangResourceAccess access = type->getResourceAccess();
    const bool writable = access != SLANG_RESOURCE_ACCESS_READ;
    if (writable && stage != GpuPackCompute) {
        fail("'%s': %s stages can only read storage resources", name, stage_name(stage));
    }
    if (base == SLANG_STRUCTURED_BUFFER || base == SLANG_BYTE_ADDRESS_BUFFER) {
        return writable ? GpuPackRWStorageBuffer : GpuPackStorageBuffer;
    }
    if (shape & SLANG_TEXTURE_COMBINED_FLAG) {
        return GpuPackSampler;
    }
    if (base >= SLANG_TEXTURE_1D && base <= SLANG_TEXTURE_CUBE) {
        if (!writable) {
            fail("'%s': sampled textures must be combined samplers (Sampler2D)", name);
        }
        return GpuPackRWStorageTexture;
    }
    fail("'%s': unsupported resource shape", name);
}

/* SDL descriptor set and first binding of each kind, for one stage. Kinds that
 * share a set follow each other in enum order. */
static uint32_t set_of(uint32_t stage, uint32_t kind) {
    if (stage == GpuPackCompute) {
        switch (kind) {
        case GpuPackSampler:
        case GpuPackStorageTexture:
        case GpuPackStorageBuffer:
            return 0;
        case GpuPackRWStorageTexture:
        case GpuPackRWStorageBuffer:
            return 1;
        default:
            return 2;
        }
    }
    const uint32_t base = stage == GpuPackVertex ? 0 : 2;
    return kind == GpuPackUniformBuffer ? base + 1 : base;
}

struct Pack {
    std::vector<gpu_pack_entry_t> entries;
    std::vector<gpu_pack_resource_t> resources;
    std::vector<gpu_pack_field_t> fields;
    std::vector<gpu_pack_input_t> inputs;
    std::vector<std::vector<uint32_t>> code;
};

static void add_fields(Pack &pack, gpu_pack_resource_t &resource,
                       slang::TypeLayoutReflection *element) {
    resource.first_field = (uint32_t)pack.fields.size();
    if (!element || element->getKind() != slang::TypeReflection::Kind::Struct) {
        return;
    }
    for (unsigned i = 0; i < element->getFieldCount(); i++) {
        slang::VariableLayoutReflection *field = element->getFieldByIndex(i);
        slang::TypeLayoutReflection *type = field->getTypeLayout();
        gpu_pack_field_t out = {};
        copy_name(out.name, field->getName());
        out.offset = (uint32_t)field->getOffset();
        out.size = (uint32_t)type->getSize();
        out.scalar = scalar_of(type->getScalarType());
        switch (type->getKind()) {
        case slang::TypeReflection::Kind::Scalar:
            out.rows = out.columns = 1;
            break;
        case slang::TypeReflection::Kind::Vector:
            out.rows = 1;
            out.columns = (uint32_t)type->getElementCount();
            break;
        case slang::TypeReflection::Kind::Matrix:
            out.rows = type->getRowCount();
            out.columns = type->getColumnCount();
            break;
        default:
            out.scalar = GpuPackScalarNone;
            break;
        }
        pack.fields.push_back(out);
    }
    resource.field_count = (uint32_t)pack.fields.size() - resource.first_field;
}

static void add_input(Pack &pack, slang::VariableLayoutReflection *param, uint32_t base) {
    slang::TypeLayoutReflection *type = param->getTypeLayout();
    const uint32_t location =
        base + (uint32_t)param->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_INPUT);
    if (type->getKind() == slang::TypeReflection::Kind::Struct) {
        for (unsigned i = 0; i < type->getFieldCount(); i++) {
            add_input(pack, type->getFieldByIndex(i), location);
        }
        return;
    }
    if (param->getCategory() != slang::ParameterCategory::VaryingInput) {
        return; /* system values such as SV_VertexID */
    }
    gpu_pack_input_t input = {};
    copy_name(input.name, param->getName());
    input.location = location;
    input.scalar = scalar_of(type->getScalarType());
    input.components = type->getKind() == slang::TypeReflection::Kind::Vector
                           ? (uint32_t)type->getElementCount()
                           : 1;
    pack.inputs.push_back(input);
}

/* Rewrites the DescriptorSet and Binding decorations of the variables that
 * Slang placed at (set, binding) to the SDL location of the resource. */
static void lower_bindings(std::vector<uint32_t> &spirv, const std::vector<uint32_t> &from_set,
                           const std::vector<uint32_t> &from_binding,
                           const std::vector<const gpu_pack_resource_t *> &to) {
    enum { OpDecorate = 71, Binding = 33, DescriptorSet = 34 };
    struct Location {
        uint32_t id, set_word, binding_word;
    };
    std::vector<Location> locations;
    auto location = [&](uint32_t id) -> Location & {
        for (Location &l : locations) {
            if (l.id == id) {
                return l;
            }
        }
        locations.push_back({ id, 0, 0 });
        return locations.back();
    };
    for (size_t word = 5; word < spirv.size();) {
        const uint32_t count = spirv[word] >> 16;
        const uint32_t opcode = spirv[word] & 0xffff;
        if (count == 0 || word + count > spirv.size()) {
            fail("malformed SPIR-V from Slang");
        }
        if (opcode == OpDecorate && count == 4) {
            if (spirv[word + 2] == DescriptorSet) {
                location(spirv[word + 1]).set_word = (uint32_t)word + 3;
            } else if (spirv[word + 2] == Binding) {
                location(spirv[word + 1]).binding_word = (uint32_t)word + 3;
            }
        }
        word += count;
    }
    std::vector<uint32_t> patched_set(spirv.size()), patched_binding(spirv.size());
    for (size_t r = 0; r < to.size(); r++) {
        bool found = false;
        for (const Location &l : locations) {
            if (!l.set_word || !l.binding_word || spirv[l.set_word] != from_set[r] ||
                spirv[l.binding_word] != from_binding[r]) {
                continue;
            }
            patched_set[l.set_word] = to[r]->set + 1;
            patched_binding[l.binding_word] = to[r]->binding + 1;
            found = true;
        }
        if (!found) {
            fail("'%s': binding not found in the SPIR-V", to[r]->name);
        }
    }
    /* Applied after matching so a new location never matches an old one. */
    for (size_t word = 0; word < spirv.size(); word++) {
        if (patched_set[word]) {
            spirv[word] = patched_set[word] - 1;
        }
        if (patched_binding[word]) {
            spirv[word] = patched_binding[word] - 1;
        }
    }
}

static void compile_entry(Pack &pack, slang::ISession *session, slang::IModule *module,
                          slang::IEntryPoint *entry_point) {
    slang::IComponentType *components[] = { module, entry_point };
    Slang::ComPtr<slang::IComponentType> composite, linked;
    Slang::ComPtr<slang::IBlob> diagnostics, code;
    if (SLANG_FAILED(session->createCompositeComponentType(components, 2, composite.writeRef(),
                                                           diagnostics.writeRef()))) {
        report(diagnostics);
        fail("cannot compose entry point");
    }
    if (SLANG_FAILED(composite->link(linked.writeRef(), diagnostics.writeRef()))) {
        report(diagnostics);
        fail("link failed");
    }
    if (SLANG_FAILED(linked->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef()))) {
        report(diagnostics);
        fail("code generation failed");
    }
    report(diagnostics);
    Slang::ComPtr<slang::IMetadata> metadata;
    if (SLANG_FAILED(linked->getEntryPointMetadata(0, 0, metadata.writeRef(), nullptr))) {
        fail("no metadata for the entry point");
    }

    slang::ProgramLayout *layout = linked->getLayout();
    slang::EntryPointReflection *reflection = layout->getEntryPointByIndex(0);

    gpu_pack_entry_t entry = {};
    copy_name(entry.name, reflection->getName());
    switch (reflection->getStage()) {
    case SLANG_STAGE_VERTEX:
        entry.stage = GpuPackVertex;
        break;
    case SLANG_STAGE_FRAGMENT:
        entry.stage = GpuPackFragment;
        break;
    case SLANG_STAGE_COMPUTE:
        entry.stage = GpuPackCompute;
        break;
    default:
        fail("'%s': only vertex, fragment and compute stages are supported", entry.name);
    }
    entry.target = GpuPackSpirv;
    if (entry.stage == GpuPackCompute) {
        SlangUInt threads[3] = { 0, 0, 0 };
        reflection->getComputeThreadGroupSize(3, threads);
        for (int i = 0; i < 3; i++) {
            entry.threads[i] = (uint32_t)threads[i];
        }
    }

    entry.first_input = (uint32_t)pack.inputs.size();
    for (unsigned i = 0; i < reflection->getParameterCount(); i++) {
        slang::VariableLayoutReflection *param = reflection->getParameterByIndex(i);
        const slang::ParameterCategory category = param->getCategory();
        if (category == slang::ParameterCategory::Uniform ||
            category == slang::ParameterCategory::PushConstantBuffer ||
            category == slang::ParameterCategory::DescriptorTableSlot) {
            fail("'%s': uniform entry point parameters are not supported, use a global "
                 "ConstantBuffer<T>",
                 param->getName());
        }
        if (entry.stage == GpuPackVertex) {
            add_input(pack, param, 0);
        }
    }

    /* Used globals, grouped by kind in declaration order. */
    std::vector<slang::VariableLayoutReflection *> used[GpuPackKindCount];
    for (unsigned i = 0; i < layout->getParameterCount(); i++) {
        slang::VariableLayoutReflection *param = layout->getParameterByIndex(i);
        if (param->getCategory() == slang::ParameterCategory::Uniform) {
            fail("'%s': loose uniforms are not supported, wrap them in a ConstantBuffer<T>",
                 param->getName());
        }
        if (param->getCategory() != slang::ParameterCategory::DescriptorTableSlot) {
            fail("'%s': unsupported parameter category", param->getName());
        }
        bool is_used = false;
        metadata->isParameterLocationUsed(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT,
                                          param->getBindingSpace(), param->getBindingIndex(),
                                          is_used);
        if (is_used) {
            used[kind_of(param, entry.stage)].push_back(param);
        }
    }

    entry.first_resource = (uint32_t)pack.resources.size();
    std::vector<uint32_t> from_set, from_binding;
    uint32_t next_binding[4] = { 0, 0, 0, 0 };
    for (uint32_t kind = 0; kind < GpuPackKindCount; kind++) {
        entry.counts[kind] = (uint32_t)used[kind].size();
        for (size_t slot = 0; slot < used[kind].size(); slot++) {
            slang::VariableLayoutReflection *param = used[kind][slot];
            slang::TypeLayoutReflection *type = param->getTypeLayout();
            gpu_pack_resource_t resource = {};
            copy_name(resource.name, param->getName());
            resource.kind = kind;
            resource.scope = scope_of(param->getVariable());
            resource.slot = (uint32_t)slot;
            resource.set = set_of(entry.stage, kind);
            resource.binding = next_binding[resource.set]++;
            slang::TypeLayoutReflection *element = type->getElementTypeLayout();
            if (kind == GpuPackUniformBuffer) {
                resource.size = element ? (uint32_t)element->getSize() : 0;
            } else if (kind == GpuPackStorageBuffer || kind == GpuPackRWStorageBuffer) {
                resource.stride = element ? (uint32_t)element->getStride() : 0;
            }
            add_fields(pack, resource, element);
            pack.resources.push_back(resource);
            from_set.push_back(param->getBindingSpace());
            from_binding.push_back(param->getBindingIndex());
        }
    }
    entry.resource_count = (uint32_t)pack.resources.size() - entry.first_resource;
    entry.input_count = (uint32_t)pack.inputs.size() - entry.first_input;

    const size_t size = code->getBufferSize();
    if (size % 4 != 0 || size < 20) {
        fail("'%s': SPIR-V size is not a multiple of 4", entry.name);
    }
    std::vector<uint32_t> spirv(size / 4);
    std::memcpy(spirv.data(), code->getBufferPointer(), size);
    std::vector<const gpu_pack_resource_t *> lowered;
    for (uint32_t i = entry.first_resource; i < pack.resources.size(); i++) {
        lowered.push_back(&pack.resources[i]);
    }
    lower_bindings(spirv, from_set, from_binding, lowered);
    entry.code_size = (uint32_t)size;
    pack.code.push_back(std::move(spirv));
    pack.entries.push_back(entry);
}

static const char *kind_names[GpuPackKindCount] = {
    "sampler", "storage_texture", "storage_buffer", "uniform_buffer", "rw_storage_texture",
    "rw_storage_buffer",
};
static const char *scope_names[] = { "-", "view", "material", "instance", "pass" };

/* Prints a pack in a stable text form, for review and diffs. */
static int dump(const char *path) {
    input_path = path;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        fail("cannot open");
    }
    std::vector<char> data((std::istreambuf_iterator<char>(file)), {});
    gpu_pack_header_t header;
    if (data.size() < sizeof(header)) {
        fail("truncated pack");
    }
    std::memcpy(&header, data.data(), sizeof(header));
    if (std::memcmp(header.magic, GPU_PACK_MAGIC, 8) != 0 || header.file_size != data.size()) {
        fail("not a valid pack");
    }
    std::printf("pack v%u, %s, %u bytes\n", header.version, header.toolchain, header.file_size);
    const char *cursor = data.data() + sizeof(header);
    std::vector<gpu_pack_entry_t> entries(header.entry_count);
    std::vector<gpu_pack_resource_t> resources(header.resource_count);
    std::vector<gpu_pack_field_t> fields(header.field_count);
    std::vector<gpu_pack_input_t> inputs(header.input_count);
    auto take = [&](void *out, size_t size) {
        std::memcpy(out, cursor, size);
        cursor += size;
    };
    take(entries.data(), entries.size() * sizeof(gpu_pack_entry_t));
    take(resources.data(), resources.size() * sizeof(gpu_pack_resource_t));
    take(fields.data(), fields.size() * sizeof(gpu_pack_field_t));
    take(inputs.data(), inputs.size() * sizeof(gpu_pack_input_t));
    for (const gpu_pack_entry_t &entry : entries) {
        std::printf("%s %s: %u bytes of SPIR-V", stage_name(entry.stage), entry.name,
                    entry.code_size);
        if (entry.stage == GpuPackCompute) {
            std::printf(", threads %ux%ux%u", entry.threads[0], entry.threads[1],
                        entry.threads[2]);
        }
        std::printf("\n");
        for (uint32_t i = 0; i < entry.input_count; i++) {
            const gpu_pack_input_t &input = inputs[entry.first_input + i];
            std::printf("  input %s: location %u, %u x scalar %u\n", input.name, input.location,
                        input.components, input.scalar);
        }
        for (uint32_t i = 0; i < entry.resource_count; i++) {
            const gpu_pack_resource_t &r = resources[entry.first_resource + i];
            std::printf("  %s %s: scope %s, slot %u, set %u binding %u", kind_names[r.kind],
                        r.name, scope_names[r.scope], r.slot, r.set, r.binding);
            if (r.stride) {
                std::printf(", stride %u", r.stride);
            }
            if (r.size) {
                std::printf(", size %u", r.size);
            }
            std::printf("\n");
            for (uint32_t f = 0; f < r.field_count; f++) {
                const gpu_pack_field_t &field = fields[r.first_field + f];
                std::printf("    %s: offset %u, size %u, scalar %u, %ux%u\n", field.name,
                            field.offset, field.size, field.scalar, field.rows, field.columns);
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && std::strcmp(argv[1], "--dump") == 0) {
        return dump(argv[2]);
    }
    const char *output_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (argv[i][0] != '-' && !*input_path) {
            input_path = argv[i];
        } else {
            std::fprintf(stderr, "usage: gpushader <input.slang> -o <output.gpushader>\n");
            return 1;
        }
    }
    if (!*input_path || !output_path) {
        std::fprintf(stderr, "usage: gpushader <input.slang> -o <output.gpushader>\n");
        return 1;
    }

    std::ifstream file(input_path, std::ios::binary);
    if (!file) {
        fail("cannot open");
    }
    std::stringstream source;
    source << file.rdbuf();

    Slang::ComPtr<slang::IGlobalSession> global;
    if (SLANG_FAILED(slang::createGlobalSession(global.writeRef()))) {
        fail("cannot create the Slang session");
    }

    slang::CompilerOptionEntry options[] = {
        { slang::CompilerOptionName::VulkanUseEntryPointName,
          { slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr } },
        /* SDL creates a Vulkan 1.0 instance, which only accepts SPIR-V 1.0;
         * Slang warns that this version is old, but its output validates. */
        { slang::CompilerOptionName::DisableWarnings,
          { slang::CompilerOptionValueKind::String, 0, 0, "50011", nullptr } },
        { slang::CompilerOptionName::DebugInformation,
          { slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_NONE, 0, nullptr,
            nullptr } },
    };
    slang::TargetDesc target = {};
    target.format = SLANG_SPIRV;
    target.profile = global->findProfile("spirv_1_0");
    slang::SessionDesc desc = {};
    desc.targets = &target;
    desc.targetCount = 1;
    desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
    desc.compilerOptionEntries = options;
    desc.compilerOptionEntryCount = sizeof(options) / sizeof(options[0]);
    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(global->createSession(desc, session.writeRef()))) {
        fail("cannot create the Slang compile session");
    }

    /* The module name and path are not written to the code: the pack only
     * depends on the source text. */
    Slang::ComPtr<slang::IBlob> diagnostics;
    const std::string text = source.str();
    slang::IModule *module = session->loadModuleFromSourceString("shader", "shader.slang",
                                                                 text.c_str(),
                                                                 diagnostics.writeRef());
    report(diagnostics);
    if (!module) {
        fail("compilation failed");
    }

    Pack pack;
    const SlangInt32 entry_count = module->getDefinedEntryPointCount();
    if (entry_count == 0) {
        fail("no entry point; mark them with [shader(\"vertex\" | \"fragment\" | \"compute\")]");
    }
    for (SlangInt32 i = 0; i < entry_count; i++) {
        Slang::ComPtr<slang::IEntryPoint> entry_point;
        module->getDefinedEntryPoint(i, entry_point.writeRef());
        compile_entry(pack, session, module, entry_point);
    }

    gpu_pack_header_t header = {};
    std::memcpy(header.magic, GPU_PACK_MAGIC, sizeof(header.magic));
    header.version = GPU_PACK_VERSION;
    header.entry_count = (uint32_t)pack.entries.size();
    header.resource_count = (uint32_t)pack.resources.size();
    header.field_count = (uint32_t)pack.fields.size();
    header.input_count = (uint32_t)pack.inputs.size();
    std::snprintf(header.toolchain, sizeof(header.toolchain), "slang %s",
                  global->getBuildTagString());

    uint32_t offset = (uint32_t)(sizeof(header) + pack.entries.size() * sizeof(gpu_pack_entry_t) +
                                 pack.resources.size() * sizeof(gpu_pack_resource_t) +
                                 pack.fields.size() * sizeof(gpu_pack_field_t) +
                                 pack.inputs.size() * sizeof(gpu_pack_input_t));
    for (gpu_pack_entry_t &entry : pack.entries) {
        entry.code_offset = offset;
        offset += entry.code_size;
    }
    header.file_size = offset;

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    auto write = [&](const void *data, size_t size) {
        out.write((const char *)data, (std::streamsize)size);
    };
    write(&header, sizeof(header));
    write(pack.entries.data(), pack.entries.size() * sizeof(gpu_pack_entry_t));
    write(pack.resources.data(), pack.resources.size() * sizeof(gpu_pack_resource_t));
    write(pack.fields.data(), pack.fields.size() * sizeof(gpu_pack_field_t));
    write(pack.inputs.data(), pack.inputs.size() * sizeof(gpu_pack_input_t));
    for (const std::vector<uint32_t> &code : pack.code) {
        write(code.data(), code.size() * 4);
    }
    if (!out) {
        fail("cannot write '%s'", output_path);
    }
    return 0;
}
