#pragma once

#include <siecs/cpp.hpp>
#include <siecs_script.h>

#include <array>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ecs {

/** Typed completion endpoint. Copies may be passed to worker threads; settlement
 * is accepted once, and cancellation makes retained endpoints stale. */
template <typename T> class script_task {
    ecs_script_task_t _task = 0;

  public:
    explicit script_task(ecs_script_task_t task) noexcept : _task(task) {}
    [[nodiscard]] ecs_script_task_t get() const noexcept { return _task; }
    [[nodiscard]] bool complete(const T &value) const {
        return ecs_script_task_complete(_task, &value);
    }
    [[nodiscard]] bool fail(std::string_view message, std::string_view type = "TaskError") const {
        const std::string owned_message(message), owned_type(type);
        return ecs_script_task_fail(_task, owned_type.c_str(), owned_message.c_str());
    }
};

namespace detail {
template <typename T> sireflect_handle_t script_host_type() {
    using type = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<type, bool>)
        return sireflect_type_by_name("bool");
    else if constexpr (std::is_same_v<type, char>)
        return sireflect_type_by_name("char");
    else if constexpr (std::is_floating_point_v<type>) {
        static_assert(std::is_same_v<type, float> || std::is_same_v<type, double>);
        return sireflect_type_by_name(std::is_same_v<type, float> ? "float" : "double");
    } else if constexpr (std::is_integral_v<type>) {
        constexpr const char *signed_names[] = { "int8_t", "int16_t", "int32_t", "int64_t" };
        constexpr const char *unsigned_names[] = { "uint8_t", "uint16_t", "uint32_t", "uint64_t" };
        constexpr size_t index = sizeof(type) == 1   ? 0
                                 : sizeof(type) == 2 ? 1
                                 : sizeof(type) == 4 ? 2
                                                     : 3;
        static_assert(sizeof(type) <= 8);
        return sireflect_type_by_name(
            std::is_signed_v<type> ? signed_names[index] : unsigned_names[index]
        );
    } else {
        static_assert(requires { type::fields; }, "script host values require native reflection");
        return ecs_component_info(ecs::component<type>())->type;
    }
}

template <typename R, typename... Args> struct script_task_binding {
    std::function<void(script_task<R>, const Args &...)> start;
    std::function<void(script_task<R>)> cancel;

    template <size_t... I>
    void
    invoke(ecs_script_task_t task, const ecs_script_value_t *arguments, std::index_sequence<I...>) {
        start(script_task<R>(task), *static_cast<const Args *>(arguments[I].data)...);
    }
};
} // namespace detail

/** Register on the world thread. Argument references last until settlement or
 * cancellation; copy arguments before retaining them in an independent worker.
 * Cancellation callbacks must stop using those views before returning. */
template <typename R, typename... Args>
[[nodiscard]] bool register_script_task(
    std::string_view name,
    std::type_identity_t<std::function<void(script_task<R>, const Args &...)>> start,
    std::type_identity_t<std::function<void(script_task<R>)>> cancel = {}
) {
    static_assert(std::is_default_constructible_v<R> && std::is_copy_constructible_v<R>);
    static_assert((!std::is_reference_v<Args> && ...));
    if (!start || name.empty() || name.find('\0') != std::string_view::npos)
        return false;
    using binding = detail::script_task_binding<R, Args...>;
    auto context = std::make_unique<binding>(binding{ std::move(start), std::move(cancel) });
    const std::string owned_name(name);
    const std::array<sireflect_handle_t, sizeof...(Args)> parameters{
        detail::script_host_type<Args>()...
    };
    ecs_script_function_desc_t desc{};
    desc.name = owned_name.c_str();
    desc.parameters = parameters.data();
    desc.parameter_count = sizeof...(Args);
    desc.result_type = detail::script_host_type<R>();
    desc.result_ops = detail::value_ops<R>();
    desc.context = context.get();
    desc.context_free = [](void *value) { delete static_cast<binding *>(value); };
    desc.start =
        [](ecs_script_task_t task, const ecs_script_value_t *arguments, uint32_t, void *value) {
            try {
                static_cast<binding *>(value)
                    ->invoke(task, arguments, std::index_sequence_for<Args...>{});
            } catch (const std::exception &error) {
                (void)ecs_script_task_fail(task, "HostError", error.what());
            } catch (...) {
                (void)ecs_script_task_fail(task, "HostError", "host task start failed");
            }
        };
    desc.cancel = [](ecs_script_task_t task, void *value) {
        auto &callback = static_cast<binding *>(value)->cancel;
        if (callback) {
            try {
                callback(script_task<R>(task));
            } catch (...) {
            }
        }
    };
    if (ecs_script_function_register(&desc))
        return false;
    context.release();
    return true;
}

struct script_diagnostic {
    std::string message;
    std::string source;
    int32_t line = 0;
    int32_t column = 0;
};

/** An owning result: diagnostics remain valid after the script is destroyed. */
class [[nodiscard]] script_result {
    int _status = 0;
    script_diagnostic _error;

    friend class script;
    friend class script_document;
    friend class script_instance;

    script_result(int status, ecs_script_eval_result_t result, std::string_view source)
        : _status(status) {
        const std::unique_ptr<char, decltype(&std::free)> message(result.error, &std::free);
        if (message)
            _error.message = message.get();
        if (status)
            _error.source = source;
        _error.line = result.line;
        _error.column = result.column;
    }

    explicit script_result(std::string message) : _status(-1), _error{ std::move(message) } {}

  public:
    script_result() = default;

    [[nodiscard]] explicit operator bool() const noexcept { return _status == 0; }
    [[nodiscard]] int status() const noexcept { return _status; }
    [[nodiscard]] const script_diagnostic &error() const noexcept { return _error; }
};

/** Reusable expression scratch. Use one runtime per concurrent execution thread. */
class script_runtime {
    struct deleter {
        void operator()(ecs_script_runtime_t *runtime) const noexcept {
            ecs_script_runtime_free(runtime);
        }
    };
    std::unique_ptr<ecs_script_runtime_t, deleter> _runtime{ ecs_script_runtime_new() };

  public:
    script_runtime() = default;
    script_runtime(const script_runtime &) = delete;
    script_runtime &operator=(const script_runtime &) = delete;
    script_runtime(script_runtime &&) noexcept = default;
    script_runtime &operator=(script_runtime &&) noexcept = default;

    [[nodiscard]] explicit operator bool() const noexcept { return bool(_runtime); }
    [[nodiscard]] ecs_script_runtime_t *get() const noexcept { return _runtime.get(); }
};

struct script_options {
    script_runtime *runtime = nullptr;
    bool skip_unknown = false;
    bool ir = false;
    ecs_entity_t parent = 0;
    uint32_t instruction_budget = 0;
};

/** Move-only parsed source. Parsing needs no world; evaluation uses the active world.
 * This owns the parsed program, not the entities produced by evaluation. */
class [[nodiscard]] script {
    struct deleter {
        void operator()(ecs_script_t *value) const noexcept { ecs_script_free(value); }
    };
    std::unique_ptr<ecs_script_t, deleter> _script;
    script_result _result;

    script() = default;

    static ecs_script_eval_desc_t descriptor(const script_options &options) noexcept {
        return { options.runtime ? options.runtime->get() : nullptr,
                 options.skip_unknown,
                 options.ir,
                 options.parent,
                 options.instruction_budget };
    }

    static bool contains_null(std::string_view text) noexcept {
        return text.find('\0') != std::string_view::npos;
    }

  public:
    script(const script &) = delete;
    script &operator=(const script &) = delete;
    script(script &&) noexcept = default;
    script &operator=(script &&) noexcept = default;

    /** Copy and parse the given source; inspect error() if the returned handle is empty. */
    [[nodiscard]] static script parse(std::string_view source, std::string_view name = "<script>") {
        script value;
        if (contains_null(source) || contains_null(name)) {
            value._result = script_result("script source and name must not contain null bytes");
            return value;
        }
        const std::string source_string(source), name_string(name);
        ecs_script_eval_result_t result{};
        value._script.reset(
            ecs_script_parse(name_string.c_str(), source_string.c_str(), nullptr, &result)
        );
        value._result = script_result(value._script ? 0 : -1, result, name);
        return value;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return bool(_script); }
    [[nodiscard]] const ecs_script_t *get() const noexcept { return _script.get(); }
    [[nodiscard]] const script_diagnostic &error() const noexcept { return _result.error(); }
    [[nodiscard]] std::string_view name() const noexcept { return _script ? _script->name : ""; }
    [[nodiscard]] std::string_view code() const noexcept { return _script ? _script->code : ""; }

    [[nodiscard]] script_result eval(const script_options &options = {}) const {
        if (!_script)
            return _result ? script_result("cannot evaluate an empty script") : _result;
        if (options.runtime && !*options.runtime)
            return script_result("cannot evaluate with an empty script runtime");
        const auto desc = descriptor(options);
        ecs_script_eval_result_t result{};
        const int status = ecs_script_eval(get(), &desc, &result);
        return script_result(status, result, name());
    }

    [[nodiscard]] static script_result
    run(std::string_view source,
        std::string_view name = "<script>",
        const script_options &options = {}) {
        return parse(source, name).eval(options);
    }

    [[nodiscard]] static script_result
    run_file(std::string_view filename, const script_options &options = {}) {
        if (contains_null(filename))
            return script_result("script filename must not contain null bytes");
        if (options.runtime && !*options.runtime)
            return script_result("cannot evaluate with an empty script runtime");
        const std::string path(filename);
        const auto desc = descriptor(options);
        ecs_script_eval_result_t result{};
        const int status = ecs_script_run_file_w_desc(path.c_str(), &desc, &result);
        return script_result(status, result, filename);
    }
};

/** Editable source with parsed, undoable revisions; changes do not evaluate. */
class [[nodiscard]] script_document {
    struct deleter {
        void operator()(ecs_script_document_t *value) const noexcept {
            ecs_script_document_free(value);
        }
    };
    std::unique_ptr<ecs_script_document_t, deleter> _document;
    script_result _result;

    script_document() = default;

    static bool contains_null(std::string_view text) noexcept {
        return text.find('\0') != std::string_view::npos;
    }

  public:
    script_document(const script_document &) = delete;
    script_document &operator=(const script_document &) = delete;
    script_document(script_document &&) noexcept = default;
    script_document &operator=(script_document &&) noexcept = default;

    [[nodiscard]] static script_document
    create(std::string_view source, std::string_view name = "<script>") {
        script_document value;
        if (contains_null(source) || contains_null(name)) {
            value._result = script_result("source and name must not contain null bytes");
            return value;
        }
        const std::string code(source), filename(name);
        ecs_script_eval_result_t result{};
        value._document.reset(ecs_script_document_new(filename.c_str(), code.c_str(), &result));
        value._result = script_result(value._document ? 0 : -1, result, name);
        return value;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return bool(_document); }
    [[nodiscard]] const script_diagnostic &error() const noexcept { return _result.error(); }
    [[nodiscard]] const ecs_script_t *get() const noexcept {
        return ecs_script_document_script(_document.get());
    }
    [[nodiscard]] std::string_view code() const noexcept { return get() ? get()->code : ""; }
    [[nodiscard]] std::string_view name() const noexcept { return get() ? get()->name : ""; }

    [[nodiscard]] script_result edit(size_t offset, size_t length, std::string_view replacement) {
        if (contains_null(replacement))
            return script_result("replacement must not contain null bytes");
        const std::string text(replacement), filename(name());
        ecs_script_eval_result_t result{};
        const int status =
            ecs_script_document_edit(_document.get(), offset, length, text.c_str(), &result);
        return script_result(status, result, filename);
    }

    [[nodiscard]] bool undo() noexcept { return ecs_script_document_undo(_document.get()); }
    [[nodiscard]] bool redo() noexcept { return ecs_script_document_redo(_document.get()); }

    [[nodiscard]] script_result save(std::string_view filename) const {
        if (contains_null(filename))
            return script_result("filename must not contain null bytes");
        const std::string path(filename);
        ecs_script_eval_result_t result{};
        const int status = ecs_script_document_save(_document.get(), path.c_str(), &result);
        return script_result(status, result, filename);
    }
};

struct script_instance_options {
    ecs_entity_t parent = 0;
    uint32_t instruction_budget = 0;
    bool watch = false;
    double watch_interval = 0.25;
};

/** Owns a persistent program instance and its native ECS contributions.
 * Keep its lifetime inside the active ECS world. */
class [[nodiscard]] script_instance {
    struct deleter {
        void operator()(ecs_script_instance_t *instance) const noexcept {
            ecs_script_instance_free(instance);
        }
    };
    std::unique_ptr<ecs_script_instance_t, deleter> _instance;
    script_result _result;
    std::string _name;

    script_instance() = default;
    static bool contains_null(std::string_view text) noexcept {
        return text.find('\0') != std::string_view::npos;
    }
    static ecs_script_instance_desc_t descriptor(const script_instance_options &options) noexcept {
        return { options.parent,
                 options.instruction_budget,
                 options.watch,
                 options.watch_interval };
    }

  public:
    script_instance(const script_instance &) = delete;
    script_instance &operator=(const script_instance &) = delete;
    script_instance(script_instance &&) noexcept = default;
    script_instance &operator=(script_instance &&) noexcept = default;

    [[nodiscard]] static script_instance create(
        std::string_view source,
        std::string_view name = "<script>",
        const script_instance_options &options = {}
    ) {
        script_instance value;
        if (contains_null(source) || contains_null(name)) {
            value._result = script_result("source and name must not contain null bytes");
            return value;
        }
        value._name = name;
        const std::string text(source);
        const auto desc = descriptor(options);
        ecs_script_eval_result_t result{};
        value._instance.reset(
            ecs_script_instance_run(value._name.c_str(), text.c_str(), &desc, &result)
        );
        value._result = script_result(value._instance ? 0 : -1, result, value._name);
        return value;
    }

    [[nodiscard]] static script_instance
    load(std::string_view filename, const script_instance_options &options = {}) {
        script_instance value;
        if (contains_null(filename)) {
            value._result = script_result("filename must not contain null bytes");
            return value;
        }
        value._name = filename;
        const auto desc = descriptor(options);
        ecs_script_eval_result_t result{};
        value._instance.reset(ecs_script_instance_load(value._name.c_str(), &desc, &result));
        value._result = script_result(value._instance ? 0 : -1, result, filename);
        return value;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return bool(_instance); }
    [[nodiscard]] ecs_script_instance_t *get() const noexcept { return _instance.get(); }
    [[nodiscard]] const script_diagnostic &error() const noexcept { return _result.error(); }
    [[nodiscard]] ecs_script_instance_state_t state() const noexcept {
        return ecs_script_instance_state(_instance.get());
    }
    [[nodiscard]] ecs_script_stats_t stats() const noexcept {
        return ecs_script_instance_stats(_instance.get());
    }
    [[nodiscard]] script_diagnostic diagnostic() const {
        const auto *result = ecs_script_instance_diagnostic(_instance.get());
        return result ? script_diagnostic{ result->error ? result->error : "",
                                           _name,
                                           result->line,
                                           result->column }
                      : script_diagnostic{};
    }
    [[nodiscard]] script_result reload(const ecs_script_t *program) {
        ecs_script_eval_result_t result{};
        int status = ecs_script_instance_reload(_instance.get(), program, &result);
        return script_result(status, result, program ? program->name : _name);
    }
    [[nodiscard]] script_result reload(std::string_view source) {
        if (contains_null(source))
            return script_result("source must not contain null bytes");
        const std::string text(source);
        ecs_script_eval_result_t result{};
        ecs_script_t *program = ecs_script_parse(_name.c_str(), text.c_str(), nullptr, &result);
        if (!program)
            return script_result(-1, result, _name);
        int status = ecs_script_instance_reload(_instance.get(), program, &result);
        ecs_script_free(program);
        return script_result(status, result, _name);
    }
    [[nodiscard]] script_result apply(const script_document &document) {
        return reload(document.get());
    }
    [[nodiscard]] static script_result flush() {
        ecs_script_eval_result_t result{};
        int status = ecs_script_flush(&result);
        return script_result(status, result, "<runtime>");
    }
    [[nodiscard]] static bool set_clock(ecs_script_clock_t clock, void *context = nullptr) {
        return ecs_script_clock_set(clock, context) == 0;
    }
};

} // namespace ecs
