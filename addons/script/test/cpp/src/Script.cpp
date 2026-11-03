#include <test.h>

#include <cstdio>
#include <type_traits>

struct ScriptPosition {
    reflected(float x; float y;)
};

static_assert(!std::is_copy_constructible_v<ecs::script>);
static_assert(!std::is_copy_assignable_v<ecs::script_runtime>);
static_assert(std::is_nothrow_move_constructible_v<ecs::script>);
static_assert(std::is_nothrow_move_assignable_v<ecs::script>);
static_assert(std::is_nothrow_move_constructible_v<ecs::script_runtime>);
static_assert(std::is_nothrow_move_assignable_v<ecs::script_runtime>);

void Script_parse_without_world(void) {
    auto program = [] {
        const std::string source = "Root { Child {} }";
        const std::string name = "scene.si";
        return ecs::script::parse(source, name);
    }();
    test_true(bool(program));
    test_true(program.name() == "scene.si");
    test_true(program.code() == "Root { Child {} }");
    test_true(program.error().message.empty());

    ecs::init();
    test_uint(ecs_lookup("Root"), 0);
    test_true(bool(program.eval()));
    test_true(bool(ecs::entity::lookup("Root::Child")));
    ecs::fini();
    // Program destruction also needs no active world.
}

void Script_typed_components_and_templates(void) {
    ecs::init();
    ecs::component<ScriptPosition>();
    const auto result = ecs::script::run(R"(
        prefab Base { ScriptPosition: {1, 2} }
        template Model {
            prop model: entity = Base
            prop offset: f32 = 3
            (IsA, $model)
            ScriptPosition: {$offset, 4}
        }
        Model instance(offset: 12)
    )");
    test_true(bool(result));
    const auto instance = ecs::entity::lookup("instance");
    test_true(bool(instance));
    test_flt(instance.get<ScriptPosition>().x, 12);
    test_flt(instance.get<ScriptPosition>().y, 4);
    test_uint(instance.target<ecs::IsA>().id(), ecs_lookup("Base"));
    ecs::fini();
}

void Script_reusable_runtime_and_moves(void) {
    ecs::init();
    ecs::component<ScriptPosition>();
    ecs::ensure("Input").set(ScriptPosition{ 2, 3 });
    auto original = ecs::script::parse("Output { ScriptPosition: {Input[ScriptPosition].x, 4} }");
    test_true(bool(original));
    auto program = std::move(original);
    test_false(bool(original));
    test_true(original.get() == nullptr);
    test_false(bool(original.eval()));
    ecs::script_runtime scratch;
    auto *native_runtime = scratch.get();
    ecs::script_runtime runtime(std::move(scratch));
    test_false(bool(scratch));
    test_true(runtime.get() == native_runtime);
    test_false(bool(program.eval({ .runtime = &scratch })));
    test_true(bool(program.eval({ .runtime = &runtime })));
    const auto output = ecs::entity::lookup("Output");
    test_flt(output.get<ScriptPosition>().x, 2);
    ecs::ensure("Input").set(ScriptPosition{ 7, 8 });
    test_true(bool(program.eval({ .runtime = &runtime })));
    test_uint(ecs_lookup("Output"), output.id());
    test_flt(output.get<ScriptPosition>().x, 7);

    auto replacement = ecs::script::parse("Unused {}");
    replacement = std::move(program);
    ecs::script_runtime replacement_runtime;
    replacement_runtime = std::move(runtime);
    test_false(bool(program));
    test_false(bool(runtime));
    test_true(replacement_runtime.get() == native_runtime);
    test_true(bool(replacement.eval({ .runtime = &replacement_runtime })));
    test_uint(ecs_lookup("Unused"), 0);
    test_uint(ecs_lookup("Output"), output.id());
    ecs::fini();
}

void Script_owned_diagnostics(void) {
    const auto result = [] {
        auto invalid = ecs::script::parse("Uncreated {}\nBroken {", "broken.si");
        test_false(bool(invalid));
        test_int(invalid.error().line, 2);
        test_true(invalid.error().column > 0);
        auto moved = std::move(invalid);
        test_true(moved.error().source == "broken.si");
        return moved.eval();
    }();
    test_false(bool(result));
    test_int(result.status(), -1);
    test_int(result.error().line, 2);
    test_true(result.error().source == "broken.si");
    test_true(!result.error().message.empty());
    const auto copy = result;
    test_true(copy.error().message == result.error().message);
    ecs::init();
    test_uint(ecs_lookup("Uncreated"), 0);
    ecs::fini();
}

void Script_options_and_recovery(void) {
    ecs::init();
    ecs::script_runtime runtime;
    const auto failed =
        ecs::script::run("Item { MissingComponent }", "unknown.si", { .runtime = &runtime });
    test_false(bool(failed));
    test_true(failed.error().source == "unknown.si");
    test_true(failed.error().message.find("MissingComponent") != std::string::npos);
    const auto skipped = ecs::script::run(
        "Item { MissingComponent }",
        "unknown.si",
        { .runtime = &runtime, .skip_unknown = true }
    );
    test_true(bool(skipped));
    test_true(skipped.error().message.empty());
    test_true(bool(ecs::script::run("Recovered {}", "ok.si", { .runtime = &runtime })));
    test_true(bool(ecs::entity::lookup("Recovered")));
    ecs::fini();
}

void Script_string_views_and_null_bytes(void) {
    ecs::init();
    const std::string source = "View {}\nTrailing {";
    const std::string name = "view.si.trailing";
    const auto result = ecs::script::run(
        std::string_view(source).substr(0, 7),
        std::string_view(name).substr(0, 7)
    );
    test_true(bool(result));
    test_true(bool(ecs::entity::lookup("View")));
    test_uint(ecs_lookup("Trailing"), 0);
    const char invalid[] = "Hidden {}\0Ignored {}";
    test_false(bool(ecs::script::run(std::string_view(invalid, sizeof(invalid) - 1))));
    test_uint(ecs_lookup("Hidden"), 0);
    test_false(bool(ecs::script::parse("", std::string_view(invalid, sizeof(invalid) - 1))));
    test_false(bool(ecs::script::run_file(std::string_view(invalid, sizeof(invalid) - 1))));
    test_true(bool(ecs::script::run({})));
    ecs::fini();
}

void Script_files(void) {
    ecs::init();
    const char *path = "cpp-script-test.si";
    FILE *file = std::fopen(path, "wb");
    test_true(file != nullptr);
    std::fputs("FromFile { Child {} }", file);
    std::fclose(file);
    ecs::script_runtime runtime;
    test_true(bool(ecs::script::run_file(path, { .runtime = &runtime })));
    const auto child = ecs::entity::lookup("FromFile::Child");
    test_true(bool(child));
    const std::string extended_path = std::string(path) + ".ignored";
    test_true(bool(ecs::script::run_file(std::string_view(extended_path).substr(0, 18))));
    test_uint(ecs_lookup("FromFile::Child"), child.id());
    test_int(std::remove(path), 0);
    const auto missing = ecs::script::run_file(path);
    test_false(bool(missing));
    test_true(missing.error().source == path);
    test_true(!missing.error().message.empty());
    ecs::fini();
}
