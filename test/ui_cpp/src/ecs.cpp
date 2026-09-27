#include <siui/cpp.hpp>
#include <bake_test.h>
#include <cmath>

static void start() { ecs::init(); ecs::import<siui>(); }
static void done() { ecs::fini(); }

void ecs_components(void) {
    start();
    auto panel = ecs::entity::create()
        .set(ui::vstack().wpx(200).padding_px(8).gap_px(6))
        .set(UiPaint{ .background = {20, 20, 20, 255} })
        .set(UiText{ .text = "Hello" });
    siui_layout_update(800, 600);
    test_true(ecs_has(panel.id(), UiNode));
    test_true(ecs_has(panel.id(), UiPaint));
    test_true(ecs_has(panel.id(), UiText));
    test_true(siui_layout(panel.id())->width >= 200);
    test_true(panel.get<UiText>().color.r == 255 && panel.get<UiText>().color.a == 255);
    done();
}

void ecs_child_of(void) {
    start();
    auto parent = ecs::entity::create().set(ui::hstack().wpx(200).hpx(100));
    auto child = ecs::entity::create()
        .set(UiNode{ .width = siui_percent(100), .height = siui_px(20) }.column().row())
        .relate<ecs::ChildOf>(parent);
    siui_layout_update(800, 600);
    test_true(std::fabs(siui_layout(child.id())->width - 200) < 0.01f);
    done();
}

void ecs_text_component(void) {
    start();
    auto label = ecs::entity::create()
        .set(UiNode{})
        .set(UiText{ .text = "First" });
    siui_layout_update(800, 600);
    float first = siui_layout(label.id())->width;
    label.set(UiText{ .text = "A longer string" });
    siui_layout_update(800, 600);
    test_true(siui_layout(label.id())->width > first);
    done();
}

void ecs_component_updates(void) {
    start();
    auto node = ecs::entity::create().set(UiNode{ .width = siui_px(40) });
    siui_layout_update(800, 600);
    test_true(std::fabs(siui_layout(node.id())->width - 40) < 0.01f);
    node.set(UiNode{ .width = siui_px(20) });
    siui_layout_update(800, 600);
    test_true(std::fabs(siui_layout(node.id())->width - 20) < 0.01f);
    done();
}
