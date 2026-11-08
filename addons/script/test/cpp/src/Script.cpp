#include <test.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <thread>
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

void Script_source_documents(void) {
    auto document = ecs::script_document::create("Root {}", "document.si");
    test_true(bool(document));
    test_true(bool(document.edit(0, 4, "Leaf")));
    test_true(document.code() == "Leaf {}");
    test_true(document.undo());
    test_true(document.code() == "Root {}");
    test_true(document.redo());
    const auto failed = document.edit(6, 1, "");
    test_false(bool(failed));
    test_true(!failed.error().message.empty());
    test_true(document.code() == "Leaf {}");
    auto moved = std::move(document);
    test_false(bool(document));
    test_false(bool(document.edit(0, 0, "")));
    test_true(bool(moved));
    ecs::init();
    test_true(bool(ecs::script::run(moved.code(), moved.name(), { .ir = true })));
    test_true(bool(ecs::entity::lookup("Leaf")));
    ecs::fini();
    const char invalid[] = "Other\0ignored";
    test_false(bool(moved.edit(0, 4, std::string_view(invalid, sizeof(invalid) - 1))));
    const char *path = "cpp-script-document.si";
    test_true(bool(moved.save(path)));
    test_int(std::remove(path), 0);
    test_true(moved.undo());
    test_false(bool(ecs::script_document::create("Root {")));
}

void Script_persistent_instances(void) {
    ecs::init();
    {
        auto instance = ecs::script_instance::create("Root {}", "instance.si");
        test_true(bool(instance));
        test_int(instance.state(), EcsScriptActive);
        ecs_entity_t root = ecs_lookup("Root");
        test_true(root != 0);
        test_true(bool(instance.reload("Root { Child {} }")));
        test_uint(ecs_lookup("Root"), root);
        test_true(ecs_lookup("Root::Child") != 0);
        auto failed = instance.reload("Broken { Missing }");
        test_false(bool(failed));
        test_true(!failed.error().message.empty());
        test_uint(ecs_lookup("Broken"), 0);
        auto moved = std::move(instance);
        test_false(bool(instance));
        test_true(bool(moved));
        test_true(bool(ecs::script_instance::flush()));
    }
    ecs::fini();
}

void Script_document_applies_transactionally(void) {
    ecs::init();
    {
        auto document = ecs::script_document::create("Root {}", "document.si");
        auto instance = ecs::script_instance::create(document.code(), document.name());
        test_true(bool(instance));
        test_true(bool(document.edit(6, 0, " Child {} ")));
        test_true(bool(instance.apply(document)));
        test_true(ecs_lookup("Root::Child") != 0);
        test_true(document.undo());
        test_true(bool(instance.apply(document)));
        test_uint(ecs_lookup("Root::Child"), 0);
    }
    ecs::fini();
}

void Script_typed_worker_task(void) {
    ecs::init();
    ecs::component<ScriptPosition>();
    std::promise<void> release;
    auto gate = release.get_future().share();
    std::thread worker;
    std::atomic<bool> accepted{ false }, duplicate{ true };
    test_true((ecs::register_script_task<ScriptPosition, int64_t>(
        "fetch_position",
        [&](ecs::script_task<ScriptPosition> task, const int64_t &argument) {
            const int64_t input = argument;
            worker = std::thread([&, task, input] {
                gate.wait();
                const ScriptPosition result{ static_cast<float>(input), 12 };
                accepted = task.complete(result);
                duplicate = task.complete(result);
            });
        }
    )));
    {
        auto instance = ecs::script_instance::create(
            "async { const result = await fetch_position(42)\n"
            "Output { ScriptPosition: {$result.x, $result.y} } }",
            "worker.si"
        );
        test_true(bool(instance));
        test_uint(ecs_lookup("Output"), 0);
        release.set_value();
        worker.join();
        test_true(accepted.load());
        test_false(duplicate.load());
        test_uint(ecs_lookup("Output"), 0);
        test_true(bool(ecs::script_instance::flush()));
        const auto output = ecs::entity::lookup("Output").get<ScriptPosition>();
        test_flt(output.x, 42);
        test_flt(output.y, 12);
    }
    ecs::fini();
}

void Script_cancels_worker_task(void) {
    ecs::init();
    std::promise<void> release;
    auto gate = release.get_future().share();
    std::thread worker;
    std::atomic<bool> accepted{ true };
    unsigned cancellations = 0;
    test_true((ecs::register_script_task<int64_t>(
        "pending",
        [&](ecs::script_task<int64_t> task) {
            worker = std::thread([&, task] {
                gate.wait();
                accepted = task.complete(99);
            });
        },
        [&](ecs::script_task<int64_t>) {
            cancellations++;
            release.set_value();
            worker.join();
        }
    )));
    {
        auto instance = ecs::script_instance::create(
            "async { const result = await pending()\nUnexpected {} }",
            "cancel.si"
        );
        test_true(bool(instance));
        test_uint(ecs_lookup("Unexpected"), 0);
    }
    test_uint(cancellations, 1);
    test_false(accepted.load());
    test_true(bool(ecs::script_instance::flush()));
    test_uint(ecs_lookup("Unexpected"), 0);
    ecs::fini();
}

struct ScriptWorkerText {
    reflected(char *message;)
};

void Script_owned_worker_result(void) {
    ecs::init();
    std::promise<void> release;
    auto gate = release.get_future().share();
    std::thread worker;
    std::atomic<bool> accepted{ false };
    test_true((ecs::register_script_task<ScriptWorkerText>(
        "fetch_text",
        [&](ecs::script_task<ScriptWorkerText> task) {
            worker = std::thread([&, task] {
                gate.wait();
                char message[] = "worker value";
                const ScriptWorkerText result{ message };
                accepted = task.complete(result);
                std::memset(message, 'x', sizeof(message) - 1);
            });
        }
    )));
    {
        auto instance = ecs::script_instance::create(
            "struct Text(value: string)\n"
            "async { const result = await fetch_text()\nOutput { Text: {$result.message} } }",
            "owned-worker.si"
        );
        test_true(bool(instance));
        release.set_value();
        worker.join();
        test_true(accepted.load());
        test_true(bool(ecs::script_instance::flush()));
        const auto component = ecs_component_lookup("Text");
        const char *message =
            *static_cast<const char *const *>(ecs_get_cid(ecs_lookup("Output"), component));
        test_str(message, "worker value");
    }
    ecs::fini();
}

void Script_controlled_clock(void) {
    ecs::init();
    double now = 0;
    test_true(
        ecs::script_instance::set_clock(
            [](void *context) { return *static_cast<double *>(context); },
            &now
        )
    );
    {
        auto instance =
            ecs::script_instance::create("async { await sleep(3)\nDone {} }", "clock.si");
        test_true(bool(instance));
        test_uint(ecs_lookup("Done"), 0);
        now = 3;
        test_true(bool(ecs::script_instance::flush()));
        test_true(ecs_lookup("Done") != 0);
    }
    test_true(ecs::script_instance::set_clock(nullptr));
    ecs::fini();
}

void Script_scoped_budget(void) {
    ecs::init();
    auto parent = ecs::ensure("Group");
    test_true(
        bool(ecs::script::run(
            "Leaf {}",
            "parent.si",
            { .parent = parent.id(), .instruction_budget = 2 }
        ))
    );
    test_true(ecs_lookup("Group::Leaf") != 0);
    test_uint(ecs_lookup("Leaf"), 0);
    auto exhausted = ecs::script::run("while true {}", "budget.si", { .instruction_budget = 10 });
    test_false(bool(exhausted));
    test_true(exhausted.error().message.find("instruction budget exhausted") != std::string::npos);
    test_true(bool(ecs::script::run("Recovered {}")));
    test_true(ecs_lookup("Recovered") != 0);
    test_uint(ecs_lookup("Group::Recovered"), 0);
    ecs::fini();
}

void Script_pairs_and_global_rollback(void) {
    ecs::init();
    test_true(bool(ecs::script::run(
        "struct Payload(x: i64)\nTarget {}\nSource { (Payload, Target): {7} }\n"
        "Copy { Payload: {Source[Payload, Target].x} }")));
    const auto component = ecs_component_lookup("Payload");
    const auto source = ecs_lookup("Source");
    const ecs_pair_desc_t descriptor{
        .first = {component, EcsPairComponent},
        .second = {ecs_lookup("Target"), EcsPairEntity}, .payload = component};
    const auto pair = ecs_pair_find(&descriptor);
    test_true(pair != 0);
    test_int(*static_cast<const int64_t *>(ecs_pair_get(source, pair)), 7);
    const auto failed = ecs::script::run(
        "Source { (Payload, Target): {99} }\nDiscarded {}\nBad { Missing }");
    test_false(bool(failed));
    test_false(failed.error().message.empty());
    test_int(*static_cast<const int64_t *>(ecs_pair_get(source, pair)), 7);
    test_uint(ecs_lookup("Discarded"), 0);
    test_uint(ecs_lookup("Bad"), 0);
    ecs::fini();
}
