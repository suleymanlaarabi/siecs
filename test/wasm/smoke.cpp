#include <siecs/cpp.hpp>

struct WasmCppPosition {
    int value;
};

extern "C" int siecs_wasm_cpp_smoke() {
    ecs::init(ecs_world_feat_desc_t{ .target_fps = 1 });

    auto entity = ecs::entity::create().set(WasmCppPosition{ .value = 9 });
    int calls = 0;
    ecs::system("WasmCppSmoke").each([&](WasmCppPosition &position) {
        position.value++;
        calls++;
    });

    bool progressed = ecs::progress();
    bool valid = progressed && calls == 1 && entity.get<WasmCppPosition>().value == 10;
    ecs::fini();
    return valid ? 0 : 1;
}
