#include <stdio.h>
#include <stdlib.h>
#include <test.h>

static int run(const char *source) {
    ecs_script_eval_result_t result = { 0 };
    int status = ecs_script_run("template-test", source, &result);
    if (result.error)
        fprintf(stderr, "%s\n", result.error);
    free(result.error);
    return status;
}

static ecs_component_t position(void) {
    return ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "Position",
        .fields = "{ float x; float y; }",
    });
}

void Template_defaults_and_arguments(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(
        run("template Point {\n prop x: f32 = 10\n prop y: f32 = 20\n Position: {$x, $y}\n}\n"
            "Point a()\nPoint b(30)\nPoint c(y: 40)\n"
            "template Empty {}\nEmpty empty()\n"),
        0
    );
    const float *a = ecs_get_cid(ecs_lookup("a"), component);
    const float *b = ecs_get_cid(ecs_lookup("b"), component);
    const float *c = ecs_get_cid(ecs_lookup("c"), component);
    test_flt(a[0], 10);
    test_flt(a[1], 20);
    test_flt(b[0], 30);
    test_flt(b[1], 20);
    test_flt(c[0], 10);
    test_flt(c[1], 40);
    ecs_component_t point = ecs_component_lookup("Point");
    const float *props = ecs_get_cid(ecs_lookup("c"), point);
    test_true(ecs_component_info(point)->type != 0);
    test_flt(props[0], 10);
    test_flt(props[1], 40);
    test_true(ecs_has_cid(ecs_lookup("empty"), ecs_component_lookup("Empty")));
    ecs_fini();
}

void Template_lexical_scope_and_nesting(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(
        run("const height = 2\n"
            "template Child {\n prop offset: f32 = 1\n Position: {$height, $offset}\n}\n"
            "template Parent {\n prop height: f32 = 9\n Child child($height)\n}\n"
            "Parent a(5)\nParent b(7)\n"),
        0
    );
    const float *a = ecs_get_cid(ecs_lookup("a::child"), component);
    const float *b = ecs_get_cid(ecs_lookup("b::child"), component);
    test_flt(a[0], 2);
    test_flt(a[1], 5);
    test_flt(b[0], 2);
    test_flt(b[1], 7);
    test_uint(ecs_target_id(ecs_lookup("a::child"), ecs_rid(ChildOf)), ecs_lookup("a"));
    ecs_fini();
}

void Template_models_and_entity_props(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(
        run("prefab Model { Position: {3, 4} }\n"
            "template Instance {\n prop model: entity = Model\n (IsA, $model)\n}\n"
            "Instance a()\nInstance b(model: \"Model\")\n"),
        0
    );
    test_uint(ecs_target_id(ecs_lookup("a"), ecs_rid(IsA)), ecs_lookup("Model"));
    test_uint(ecs_target_id(ecs_lookup("b"), ecs_rid(IsA)), ecs_lookup("Model"));
    const float *value = ecs_get_cid(ecs_lookup("a"), component);
    test_flt(value[0], 3);
    test_flt(value[1], 4);
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
    test_int(
        run("template Shape {\n prop x: f32 = 0\n Position: {$x, 4}\n Marker\n}\n"
            "prefab Tree { Shape branch(3) }\n"
            "a { (IsA, Tree) }\nb { (IsA, Tree) }\n"),
        0
    );
    ecs_entity_t base = ecs_lookup("Tree::branch");
    ecs_entity_t a = ecs_lookup("a::branch"), b = ecs_lookup("b::branch");
    test_true(base != 0);
    test_true(a != 0);
    test_true(b != 0);
    test_true(a != b);
    test_true(ecs_has(base, Abstract));
    test_false(ecs_has(a, Abstract));
    test_true(ecs_has_cid(a, marker));
    test_uint(ecs_target_id(a, ecs_rid(IsA)), base);
    const float *value = ecs_get_cid(a, component);
    test_flt(value[0], 3);
    test_flt(value[1], 4);
    ecs_fini();
}

void Template_anonymous_calls_in_scopes(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(
        run("template Point { prop x: f32 = 0\nPosition: {$x, 4} }\n"
            "Root { for i in 0..3 { Point($i) } }\n"
            "template Group { Point(10)\nPoint(20) }\nGroup Parent()"),
        0
    );
    ecs_entity_t root = ecs_lookup("Root"), parent = ecs_lookup("Parent");
    test_false(ecs_has_cid(root, component));
    test_false(ecs_has_cid(parent, component));
    ecs_relation_sources_t children = ecs_relation_sources(root, ecs_rid(ChildOf));
    test_uint(children.count, 3);
    unsigned seen = 0;
    for (uint32_t i = 0; i < children.count; i++) {
        const float *value = ecs_get_cid(children.entities[i], component);
        test_true(value != NULL);
        test_flt(value[1], 4);
        test_true(value[0] >= 0 && value[0] <= 2);
        seen |= 1u << (unsigned)value[0];
        test_uint(ecs_target_id(children.entities[i], ecs_rid(ChildOf)), root);
    }
    test_uint(seen, 7);
    children = ecs_relation_sources(parent, ecs_rid(ChildOf));
    test_uint(children.count, 2);
    seen = 0;
    for (uint32_t i = 0; i < children.count; i++) {
        const float *value = ecs_get_cid(children.entities[i], component);
        test_true(value != NULL);
        test_true(value[0] == 10 || value[0] == 20);
        seen |= value[0] == 10 ? 1u : 2u;
    }
    test_uint(seen, 3);
    ecs_fini();
}

void Template_inheritance(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(
        run("const global = 2\nconst extra = 100\n"
            "template Base { prop x: f32 = $global\nprop label: string = \"base\"\n"
            "Position: {$x, 0}\nBaseCapture { Position: {$global, $extra} } }\n"
            "const global = 9\n"
            "template Derived: Base { prop x: f32 = 9.0\nprop extra: f32 = 3\n"
            "Position: {$x + $extra, $global} }\n"
            "template Final: Derived { prop x = 20\n}\n"
            "Derived Root(label: \"custom\")\nFinal Default()"),
        0
    );
    ecs_entity_t root = ecs_lookup("Root"), other = ecs_lookup("Default");
    const float *value = ecs_get_cid(root, component);
    test_flt(value[0], 12);
    test_flt(value[1], 9);
    value = ecs_get_cid(ecs_lookup("Root::BaseCapture"), component);
    test_flt(value[0], 2);
    test_flt(value[1], 100);
    value = ecs_get_cid(other, component);
    test_flt(value[0], 23);
    ecs_component_t base = ecs_component_lookup("Base"), derived = ecs_component_lookup("Derived");
    const sireflect_fields_t *base_fields = sireflect_type_fields(ecs_component_info(base)->type);
    const sireflect_fields_t *derived_fields =
        sireflect_type_fields(ecs_component_info(derived)->type);
    test_uint(base_fields->field_count, 2);
    test_uint(derived_fields->field_count, 3);
    const char *const *base_text = (const char *const *)((const char *)ecs_get_cid(root, base) +
                                                         base_fields->fields[1].offset);
    const char *const *derived_text =
        (const char *const *)((const char *)ecs_get_cid(root, derived) +
                              derived_fields->fields[1].offset);
    test_str(*base_text, "custom");
    test_str(*derived_text, "custom");
    test_true(*base_text != *derived_text);
    test_flt(*(const float *)ecs_get_cid(root, base), 9);
    test_flt(*(const float *)ecs_get_cid(other, base), 20);
    test_true(ecs_has_cid(other, ecs_component_lookup("Final")));
    ecs_script_instance_t *instance = ecs_script_instance_run("inheritance.si", "", NULL, NULL);
    test_true(instance != NULL);
    const char *invalid[] = { "template Invalid: Missing {}",
                              "template LocalBase { prop x: f32 = 1 }\ntemplate Invalid: LocalBase "
                              "{ prop x: string = \"bad\" }",
                              "template Invalid { prop x: f32 = 1\nprop x: f32 = 2 }" };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        ecs_script_eval_result_t result = { 0 };
        ecs_script_t *candidate = ecs_script_parse("inheritance.si", invalid[i], NULL, &result);
        test_true(candidate != NULL);
        test_int(ecs_script_instance_reload(instance, candidate, &result), -1);
        test_true(result.error != NULL);
        free(result.error);
        ecs_script_free(candidate);
        test_uint(ecs_component_lookup("Invalid"), 0);
    }
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Template_lexical_functions(void) {
    ecs_init();
    ecs_component_t component = position();
    test_int(
        run("fn adjust(n: f32) -> f32 { $n + 1 }\n"
            "template Base { prop x: f32 = adjust(2)\nPosition: {adjust($x), 0} }\n"
            "fn adjust(n: f32) -> f32 { $n + 100 }\n"
            "template Derived: Base { Child { Position: {adjust(2), 0} } }\n"
            "Derived Root()\nAfter { Position: {adjust(2), 0} }"),
        0
    );
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Root"), component), 4);
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Root::Child"), component), 102);
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("After"), component), 102);
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Root"), ecs_component_lookup("Base")), 3);
    ecs_fini();
}
