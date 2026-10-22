#pragma once
#include <siecs/cpp.hpp>
#include <sigpu.h>

/*
 * C++ façade of the draw features (RFC section 6.1). The extract callback
 * takes the same parameters as ecs::system::each (components, ecs::changed,
 * ecs::optional, resources...) plus the outputs of the row:
 *
 *   gpu::feature<CubeInstance>("render3d::cubes")
 *       .with<Cuboid>()
 *       .geometry(cube)
 *       .program(program)
 *       .culling(GpuCullSphere)
 *       .extract([](ecs::changed<const GlobalPosition3d> p, const Cuboid &c,
 *                   gpu::out<CubeInstance> out, gpu::bounds bounds) {
 *           *out = pack_cube(*p, c);
 *           *bounds = cube_bounds(*p, c);
 *       });
 *
 * A plain `CubeInstance &` parameter would be read as a component of the
 * query: the outputs are their own parameter types.
 */
namespace gpu {

/* The instance of the current row in the feature's storage. */
template <typename T> class out {
    T *_base = nullptr;
    T *_ptr = nullptr;

  public:
    static out init() noexcept { return {}; }
    void table(ecs_iter_t *it) noexcept { _base = static_cast<T *>(sigpu_instances(it)); }
    void row(ecs_iter_t *, uint32_t row) noexcept { _ptr = _base ? _base + row : nullptr; }
    [[nodiscard]] T &operator*() const noexcept { return *_ptr; }
    [[nodiscard]] T *operator->() const noexcept { return _ptr; }
};

/* The bounding sphere of the current row; inert without culling. */
class bounds {
    GpuSphere *_base = nullptr;
    GpuSphere *_ptr = nullptr;
    GpuSphere _unused{};

  public:
    static bounds init() noexcept { return {}; }
    void table(ecs_iter_t *it) noexcept { _base = sigpu_bounds(it); }
    void row(ecs_iter_t *, uint32_t row) noexcept { _ptr = _base ? _base + row : &_unused; }
    [[nodiscard]] GpuSphere &operator*() const noexcept { return *_ptr; }
    [[nodiscard]] GpuSphere *operator->() const noexcept { return _ptr; }
};

/* The layer key of the current row of a layered feature. */
class order_key {
    uint32_t *_base = nullptr;
    uint32_t *_ptr = nullptr;
    uint32_t _unused = 0;

  public:
    static order_key init() noexcept { return {}; }
    void table(ecs_iter_t *it) noexcept { _base = sigpu_order_keys(it); }
    void row(ecs_iter_t *, uint32_t row) noexcept { _ptr = _base ? _base + row : &_unused; }
    [[nodiscard]] uint32_t &operator*() const noexcept { return *_ptr; }
};

namespace detail {
template <typename Callback, typename Args> void extract(ecs_iter_t *it) {
    Callback &callback = *static_cast<Callback *>(sigpu_feature_data(it));
    auto params = ecs::detail::init_callback_params<Args>();
    ecs::detail::run_batch<Args>(callback, it, params);
}

template <typename Callback> void destroy(void *callback) {
    delete static_cast<Callback *>(callback);
}
} // namespace detail

template <typename Instance> class feature : public ecs::query_filter<feature<Instance>> {
    GpuFeatureDesc _desc{};

  public:
    explicit feature(const char *name) {
        _desc.name = name;
        _desc.instance = ecs::detail::ecs_cpp_component_id<Instance>();
    }
    feature &geometry(ecs_entity_t geometry) {
        _desc.geometry = geometry;
        return *this;
    }
    feature &material(ecs_entity_t material) {
        _desc.material = material;
        return *this;
    }
    feature &program(ecs_entity_t program) {
        _desc.program = program;
        return *this;
    }
    feature &culling(uint8_t culling) {
        _desc.culling = culling;
        return *this;
    }
    feature &ordering(uint8_t ordering) {
        _desc.ordering = ordering;
        return *this;
    }
    feature &cell_size(float size) {
        _desc.cell_size = size;
        return *this;
    }
    feature &chunk_size(uint32_t rows) {
        _desc.chunk_size = rows;
        return *this;
    }

    /* Registers the feature; the world owns the callback. */
    template <typename F> ecs_entity_t extract(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using args = typename ecs::function_traits<callback>::args_tuple;
        ecs::detail::append_callback_terms<args>(
            this->desc, this->component_index, this->resource_index, this->relation_index
        );
        _desc.query = this->desc;
        _desc.extract = detail::extract<callback, args>;
        _desc.user_data = new callback(std::forward<F>(func));
        _desc.user_data_dtor = detail::destroy<callback>;
        const ecs_entity_t entity = sigpu_feature(&_desc);
        if (!entity) {
            detail::destroy<callback>(_desc.user_data);
        }
        return entity;
    }
};

} // namespace gpu
