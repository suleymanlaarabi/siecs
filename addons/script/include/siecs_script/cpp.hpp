#pragma once

#include <siecs/cpp.hpp>
#include <siecs_script.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ecs {

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
        return { options.runtime ? options.runtime->get() : nullptr, options.skip_unknown, options.ir };
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
        const int status = ecs_script_document_edit(
            _document.get(), offset, length, text.c_str(), &result);
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

} // namespace ecs
