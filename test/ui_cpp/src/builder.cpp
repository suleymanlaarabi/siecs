#include <siui/cpp.hpp>
#include <bake_test.h>
#include <cmath>
#include <string>
static ecs_entity_t seen_entity;
static int node_sets,paint_sets,text_sets;
static void count_sets(ecs_observer_event_t *event) {
    if(event->component!=ecs_id(UiNode) && event->component!=ecs_id(UiPaint) &&
       event->component!=ecs_id(UiText)) return;
    if(!seen_entity) seen_entity=event->entity;
    if(event->entity!=seen_entity) return;
    if(event->component==ecs_id(UiNode)) {
        const UiNode *node=(const UiNode *)event->trigger_data;
        test_true(node!=nullptr);test_true(node->width.value==40);node_sets++;
    } else if(event->component==ecs_id(UiPaint)) {
        const UiPaint *paint=(const UiPaint *)event->trigger_data;
        test_true(paint!=nullptr);test_int(1,paint->background.r);paint_sets++;
    } else {
        const UiText *text=(const UiText *)event->trigger_data;
        test_true(text!=nullptr);test_true(text->text!=0);text_sets++;
    }
}
static void start() {ecs_init();ecs::import<siui>();}
static void done() {ecs_fini();}
void builder_card(void) {
    start();
    auto card=ui::node().max_width(420).padding(16).direction(ui::column).gap(8)
        .background({20,20,20,255}).border(1,{80,80,80,255}).text("Hello world").spawn();
    siui_layout_update(800,600);
    test_true(siui_layout(card)->width>34);
    test_true(ecs_has(card,UiNode));
    test_true(ecs_has(card,UiPaint));
    test_true(ecs_has(card,UiText));
    done();
}
void builder_child_of(void) {
    start();auto parent=ui::node().width(200).height(100).spawn();
    auto child=ui::node().width(ui::percent(100)).height(ui::auto_size).child_of(parent).spawn();
    siui_layout_update(800,600);
    test_true(std::fabs(siui_layout(child)->width-200)<0.01f);
    done();
}
void builder_string_view_copy(void) {
    start();std::string value="First";auto builder=ui::node().text(std::string_view(value));
    value="Second";auto entity=builder.spawn();
    test_str("First",siui_text_string(ecs_get(entity,UiText)->text));
    done();
}
void builder_single_mutations(void) {
    start();seen_entity=0;node_sets=paint_sets=text_sets=0;
    ecs_observer_desc_t observer{};observer.on=EcsOnSet;observer.callback=count_sets;
    ecs_observer_init(&observer);
    auto entity=ui::node().width(40).background({1,2,3,255}).text("X").spawn();
    test_true(ecs_has(entity,UiNode));test_true(ecs_has(entity,UiText));
    test_true(seen_entity==entity);
    test_int(1,node_sets);test_int(1,paint_sets);test_int(1,text_sets);
    ui::node().width(20).spawn();
    test_int(1,paint_sets);test_int(1,text_sets);
    done();
}
