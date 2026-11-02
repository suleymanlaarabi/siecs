#include <test.h>
#include <stdio.h>
#include <stdlib.h>

static int run(const char *source) {
    ecs_script_eval_result_t result = {0};
    int status = ecs_script_run("template-test", source, &result);
    if (result.error) fprintf(stderr, "%s\n", result.error);
    free(result.error);
    return status;
}

static ecs_component_t position(void) {
    return ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "Position", .fields = "{ float x; float y; }",
    });
}

void Template_defaults_and_arguments(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(run(
        "template Point {\n prop x: f32 = 10\n prop y: f32 = 20\n Position: {$x, $y}\n}\n"
        "Point a()\nPoint b(30)\nPoint c(y: 40)\n"
        "template Empty {}\nEmpty empty()\n"), 0);
    const float *a = ecs_get_cid(ecs_lookup("a"), component);
    const float *b = ecs_get_cid(ecs_lookup("b"), component);
    const float *c = ecs_get_cid(ecs_lookup("c"), component);
    test_flt(a[0], 10); test_flt(a[1], 20);
    test_flt(b[0], 30); test_flt(b[1], 20);
    test_flt(c[0], 10); test_flt(c[1], 40);
    ecs_component_t point = ecs_component_lookup("Point");
    const float *props = ecs_get_cid(ecs_lookup("c"), point);
    test_true(ecs_component_info(point)->type != 0);
    test_flt(props[0], 10); test_flt(props[1], 40);
    test_true(ecs_has_cid(ecs_lookup("empty"), ecs_component_lookup("Empty")));
    ecs_fini();
}

void Template_lexical_scope_and_nesting(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(run(
        "const height = 2\n"
        "template Child {\n prop offset: f32 = 1\n Position: {$height, $offset}\n}\n"
        "template Parent {\n prop height: f32 = 9\n Child child($height)\n}\n"
        "Parent a(5)\nParent b(7)\n"), 0);
    const float *a = ecs_get_cid(ecs_lookup("a::child"), component);
    const float *b = ecs_get_cid(ecs_lookup("b::child"), component);
    test_flt(a[0], 2); test_flt(a[1], 5);
    test_flt(b[0], 2); test_flt(b[1], 7);
    test_uint(ecs_target_id(ecs_lookup("a::child"), ecs_rid(ChildOf)), ecs_lookup("a"));
    ecs_fini();
}

void Template_models_and_entity_props(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(run(
        "prefab Model { Position: {3, 4} }\n"
        "template Instance {\n prop model: entity = Model\n (IsA, $model)\n}\n"
        "Instance a()\nInstance b(model: \"Model\")\n"), 0);
    test_uint(ecs_target_id(ecs_lookup("a"), ecs_rid(IsA)), ecs_lookup("Model"));
    test_uint(ecs_target_id(ecs_lookup("b"), ecs_rid(IsA)), ecs_lookup("Model"));
    const float *value = ecs_get_cid(ecs_lookup("a"), component);
    test_flt(value[0], 3); test_flt(value[1], 4);
    ecs_fini();
}

void Template_errors_and_recursion(void) {
    ecs_init();
    position();
    test_int(run("template Point { prop x: f32 = 0 }\nPoint a(missing: 2)"), -1);
    test_int(run("template Point { prop x: f32 = 0 }\nPoint a(x: 2, x: 3)"), -1);
    test_int(run("template Recurse { Recurse() }\nRecurse()"), -1);
    test_int(run("template BreakLoop { continue }\nfor i in 0..1 { BreakLoop() }"), -1);
    test_int(run("prop x: f32 = 0"), -1);
    ecs_fini();
}

void Template_prefab_hierarchy(void) {
    ecs_init();
    ecs_component_t component = position();
    ecs_component_t marker = ecs_tag_init("Marker");
    test_int(run(
        "template Shape {\n prop x: f32 = 0\n Position: {$x, 4}\n Marker\n}\n"
        "prefab Tree { Shape branch(3) }\n"
        "a { (IsA, Tree) }\nb { (IsA, Tree) }\n"), 0);
    ecs_entity_t base = ecs_lookup("Tree::branch");
    ecs_entity_t a = ecs_lookup("a::branch"), b = ecs_lookup("b::branch");
    test_true(base != 0); test_true(a != 0); test_true(b != 0);
    test_true(a != b);
    test_true(ecs_has(base, Abstract));
    test_false(ecs_has(a, Abstract));
    test_true(ecs_has_cid(a, marker));
    test_uint(ecs_target_id(a, ecs_rid(IsA)), base);
    const float *value = ecs_get_cid(a, component);
    test_flt(value[0], 3); test_flt(value[1], 4);
    ecs_fini();
}
