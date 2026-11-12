#include <stdio.h>
#include <stdlib.h>
#include <test.h>

static int run(const char *code) {
    ecs_script_eval_result_t result = { 0 };
    int status = ecs_script_run("language.si", code, &result);
    if (result.error)
        fprintf(stderr, "%s\n", result.error);
    free(result.error);
    return status;
}

void Language_typed_functions_and_captures(void) {
    ecs_init();
    test_int(
        run("struct Value(x: f32)\nconst multiplier = 3\n"
            "fn scale(value: f32) -> f32 { $value * $multiplier }\n"
            "const multiplier = 100\nOutput { Value: {scale(4)} }"),
        0
    );
    const float *value = ecs_get_cid(ecs_lookup("Output"), ecs_component_lookup("Value"));
    test_flt(value[0], 12);
    test_int(run("fn wrong(x: f32) -> f32 { $x }\nconst value = wrong()"), -1);
    test_int(run("fn recursive(x: f32) -> f32 { recursive($x) }\nconst value = recursive(1)"), -1);
    ecs_fini();
}

void Language_mutable_function_locals(void) {
    ecs_init();
    test_int(
        run("struct Value(x: i32)\n"
            "fn sum(n: i32) -> i32 {\nmut i: i32 = 0\nmut total: i32 = 0\n"
            "while $i < $n { total = $total + $i\ni = $i + 1 }\n$total\n}\n"
            "Output { Value: {sum(5)} }"),
        0
    );
    const int32_t *value = ecs_get_cid(ecs_lookup("Output"), ecs_component_lookup("Value"));
    test_int(value[0], 10);
    test_int(
        run("fn immutable(n: i32) -> i32 { const x = 0\nx = 1\n$x }\nconst result = immutable(1)"),
        -1
    );
    ecs_fini();
}

void Language_collection_iteration_and_indexing(void) {
    ecs_init();
    test_int(
        run("struct Value(x: i32)\nconst values = [3, 5, 7]\n"
            "for (index, value) in $values { \"Item_$index\" { Value: {$value} } }\n"
            "Output { Value: {$values[1]} }"),
        0
    );
    ecs_component_t component = ecs_component_lookup("Value");
    test_int(*(const int32_t *)ecs_get_cid(ecs_lookup("Output"), component), 5);
    test_int(*(const int32_t *)ecs_get_cid(ecs_lookup("Item_2"), component), 7);
    test_int(run("const values = [1]\nconst invalid = $values[2]"), -1);
    ecs_fini();
}

void Language_iteration_boundaries(void) {
    const char *source =
        "struct Value(x: i64)\n"
        "for i in 2147483647..2147483649 { \"High_$i\" { Value: {$i} } }\n"
        "for i in -2147483650..-2147483648 { \"Low_$i\" { Value: {$i} } }\n"
        "for i in 9223372036854775806..9223372036854775807 { Last { Value: {$i} } }\n"
        "for i in 5..2 { Unexpected {} }\n"
        "for (index, item) in [] { Unexpected {} }\n"
        "for (index, item) in [10, 20] { if $index == 0 { continue }\n"
        "\"Item_$index\" { Value: {$item} } }";
    for (int mode = 0; mode < 2; mode++) {
        ecs_init();
        ecs_script_eval_desc_t desc = { .ir = mode != 0 };
        ecs_script_eval_result_t result = { 0 };
        test_int(ecs_script_run_w_desc("iteration.si", source, &desc, &result), 0);
        test_true(result.error == NULL);
        ecs_component_t component = ecs_component_lookup("Value");
        test_int(
            *(const int64_t *)ecs_get_cid(ecs_lookup("High_2147483648"), component),
            2147483648LL
        );
        test_int(
            *(const int64_t *)ecs_get_cid(ecs_lookup("Low_-2147483650"), component),
            -2147483650LL
        );
        test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Last"), component), INT64_MAX - 1);
        test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Item_1"), component), 20);
        test_uint(ecs_lookup("Unexpected"), 0);
        test_uint(ecs_lookup("Item_0"), 0);
        test_int(run("for (index, item) in 0..2 { Invalid {} }"), -1);
        test_int(run("for item in 42 { Invalid {} }"), -1);
        test_uint(ecs_lookup("Invalid"), 0);
        ecs_fini();
    }
}

void Language_includes_keep_lexical_declarations(void) {
    const char *path = "native-script-include.si";
    FILE *file = fopen(path, "wb");
    test_true(file != NULL);
    fputs("const factor = 4\nfn scale(x: f32) -> f32 { $x * $factor }\n", file);
    test_int(fclose(file), 0);
    ecs_init();
    test_int(
        run("include native-script-include.si\nstruct Value(x: f32)\n"
            "Output { Value: {scale(3)} }"),
        0
    );
    test_flt(*(const float *)ecs_get_cid(ecs_lookup("Output"), ecs_component_lookup("Value")), 12);
    ecs_fini();
    file = fopen(path, "wb");
    test_true(file != NULL);
    fputs("include native-script-include.si\n", file);
    fclose(file);
    ecs_script_eval_result_t result = { 0 };
    test_true(ecs_script_parse_file(path, NULL, &result) == NULL);
    test_true(result.error != NULL);
    free(result.error);
    test_int(remove(path), 0);
}

void Language_owning_strings_and_template_properties(void) {
    ecs_init();
    test_int(
        run("struct Label(text: string)\n"
            "fn label(value: string) -> string { $value }\n"
            "template Named { prop text: string = \"default\"\nLabel: {$text} }\n"
            "Named Root(label(\"original\"))\n"
            "const value = Root[Label]\nCopy { Label: $value }"),
        0
    );
    ecs_component_t component = ecs_component_lookup("Label");
    const char *const *root = ecs_get_cid(ecs_lookup("Root"), component);
    const char *const *copy = ecs_get_cid(ecs_lookup("Copy"), component);
    test_str(*root, "original");
    test_str(*copy, "original");
    test_true(*root != *copy);
    test_int(run("Root { Label: {\"replaced\"} }"), 0);
    test_str(*(const char *const *)ecs_get_cid(ecs_lookup("Copy"), component), "original");
    ecs_fini();
}

void Language_try_catch_rolls_back_scope(void) {
    ecs_init();
    test_int(
        run("struct Value(x: i32)\nOriginal { Value: {10} }\n"
            "try { Original { Value: {99} }\nDiscarded { Missing } } catch(RuntimeError) { "
            "Recovered {} }"),
        0
    );
    test_int(
        *(const int32_t *)ecs_get_cid(ecs_lookup("Original"), ecs_component_lookup("Value")),
        10
    );
    test_uint(ecs_lookup("Discarded"), 0);
    test_true(ecs_lookup("Recovered") != 0);
    test_int(run("try { Abandoned {}\nBad { Missing } } catch(OtherError) { Wrong {} }"), -1);
    test_uint(ecs_lookup("Abandoned"), 0);
    test_uint(ecs_lookup("Wrong"), 0);
    test_int(
        run("fn guarded(n: i32) -> i32 { mut x: i32 = 4\n"
            "try { x = 9\nconst failed = 1 / 0 } catch { x = $x + $n }\n$x }\n"
            "Final { Value: {guarded(3)} }"),
        0
    );
    test_int(*(const int32_t *)ecs_get_cid(ecs_lookup("Final"), ecs_component_lookup("Value")), 7);
    ecs_fini();
}

void Language_native_expression_execution(void) {
    ecs_init();
    test_int(
        run("struct Value(x: i64)\nconst values = (4..9)\n"
            "const vector = [1, 2, 3] * 2 + 1\n"
            "const child = new Created { Value: {$values[2]} }\n"
            "Output { Value: {match $vector.zyx[0] { 7 => 42, _ => 0 }} }"),
        0
    );
    ecs_component_t cid = ecs_component_lookup("Value");
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Output"), cid), 42);
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Created"), cid), 6);
    test_int(
        run("const a = 18446744073709551615u\n"
            "if $a > 18446744073709551614u { Exact {} }"),
        0
    );
    test_true(ecs_lookup("Exact") != 0);
    test_int(run("const overflow = 18446744073709551615u + 1u"), -1);
    test_int(run("const underflow = -9223372036854775807 - 2"), -1);
    test_int(run("while true {}"), -1);
    test_int(run("for i in 0..2147483647 {}"), -1);
    ecs_fini();
}

void Language_methods_scripts_arrays_and_typed_collections(void) {
    ecs_init();
    ecs_component_t numbers = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Numbers", .fields = "{ int64_t values[3]; }" }
    );
    test_true(numbers != 0);
    ecs_entity_t source = ecs_new_named(0, "Source");
    int64_t input[] = { 2, 4, 6 };
    ecs_set_cid(source, numbers, input);
    test_int(
        run("struct Value(x: i64)\nstruct Text(value: string)\n"
            "fn scale(value: i64, factor: i64) -> i64 { $value * $factor }\n"
            "const code = script { Nested {} }\nSourceText { Text: {$code} }\n"
            "mut values: i32[] = [1, 2]\nvalues = [3, 4, 5]\n"
            "Output { Value: {(7).scale(6) + Source[Numbers].values[1] + $values[2]} }\n"
            "if +18446744073709551615u == 18446744073709551615u { Exact {} }"),
        0
    );
    ecs_component_t value = ecs_component_lookup("Value");
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Output"), value), 51);
    test_true(ecs_lookup("Exact") != 0);
    test_uint(ecs_lookup("Nested"), 0);
    const char *code =
        *(const char *const *)ecs_get_cid(ecs_lookup("SourceText"), ecs_component_lookup("Text"));
    test_int(run(code), 0);
    test_true(ecs_lookup("Nested") != 0);
    test_int(run("const invalid = Source[Numbers].values[3]"), -1);
    test_int(run("const invalid = -18446744073709551615u"), -1);
    test_int(run("mut values: i32[] = [1]\nvalues = [\"invalid\"]"), -1);
    ecs_fini();
}

void Language_interpolation_formats(void) {
    ecs_init();
    test_int(
        run("struct Text(value: string)\n"
            "const maximum = 18446744073709551615u\n"
            "const word = \"abcdef\"\n"
            "Output { Text: {\"{42:08x}|{3.125:.2f}|{7:+04d}|{true:6s}|"
            "{$word:-8.3s}|{$maximum}|{$maximum:X}|{-9223372036854775808u}\"} }"),
        0
    );
    ecs_component_t text = ecs_component_lookup("Text");
    const char *message = *(const char *const *)ecs_get_cid(ecs_lookup("Output"), text);
    test_str(
        message,
        "0000002a|3.12|+007|  true|abc     "
        "|18446744073709551615|FFFFFFFFFFFFFFFF|-9223372036854775808"
    );
    test_int(run("Copy { Text: {\"{Output[Text].value:s}\"} }"), 0);
    test_str(*(const char *const *)ecs_get_cid(ecs_lookup("Copy"), text), message);
    const char *invalid[] = { "%n",  "1025d", ".1025f",
                              "2xx", "*d",    "000000000000000000000000000000d",
                              "+s" };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        char source[256];
        snprintf(source, sizeof source, "Discarded {}\nconst value = \"{42:%s}\"", invalid[i]);
        ecs_script_eval_result_t result = { 0 };
        test_int(ecs_script_run("format.si", source, &result), -1);
        test_true(result.error != NULL);
        test_int(result.line, 2);
        free(result.error);
        test_uint(ecs_lookup("Discarded"), 0);
    }
    const char *incompatible[] = { "Discarded {}\nconst value = \"{3.5:x}\"",
                                   "Discarded {}\nconst value = \"{$unknown}\"" };
    ecs_script_instance_t *instance = ecs_script_instance_run("format.si", "", NULL, NULL);
    test_true(instance != NULL);
    for (size_t i = 0; i < sizeof incompatible / sizeof incompatible[0]; i++) {
        ecs_script_eval_result_t result = { 0 };
        ecs_script_t *program = ecs_script_parse("format.si", incompatible[i], NULL, &result);
        test_true(program != NULL);
        test_int(ecs_script_instance_reload(instance, program, &result), -1);
        test_true(result.error != NULL);
        test_int(result.line, 2);
        free(result.error);
        ecs_script_free(program);
        test_uint(ecs_lookup("Discarded"), 0);
    }
    ecs_script_instance_free(instance);
    test_int(run("const value = \"{-1:u}\""), -1);
    test_int(run("const value = \"{18446744073709551615u:d}\""), -1);
    ecs_fini();
}

void Language_struct_bodies_and_arrays(void) {
    ecs_init();
    test_int(
        run("struct Point {\nx: f32\ny: f32\n}\n"
            "struct Samples {\npoints: Point[2]\nlabel: string\n}\n"
            "struct Values(items: i32[3])\n"
            "struct Labels { names: string[2] }\n"
            "Source { Samples: {points: [{1, 2}, {3, 4}], label: \"owned\"} }\n"
            "Copy { Samples: Source[Samples] }\n"
            "Words { Labels: {[\"first\", \"second\"]} }\n"
            "WordCopy { Labels: Words[Labels] }\n"
            "Output { Values: {[5, 6, 7]} }"),
        0
    );
    ecs_component_t samples = ecs_component_lookup("Samples");
    const ecs_component_info_t *info = ecs_component_info(samples);
    const sireflect_fields_t *fields = sireflect_type_fields(info->type);
    test_uint(fields->field_count, 2);
    const sireflect_type_info_t *array = sireflect_type_info(fields->fields[0].type);
    test_uint(array->element_count, 2);
    const float *points = ecs_get_cid(ecs_lookup("Source"), samples);
    test_flt(points[0], 1);
    test_flt(points[3], 4);
    const char *const *source =
        (const char *const *)((const char *)points + fields->fields[1].offset);
    const char *const *copy =
        (const char *const *)((const char *)ecs_get_cid(ecs_lookup("Copy"), samples) +
                              fields->fields[1].offset);
    test_str(*source, "owned");
    test_str(*copy, "owned");
    test_true(*source != *copy);
    const int32_t *values = ecs_get_cid(ecs_lookup("Output"), ecs_component_lookup("Values"));
    test_int(values[0], 5);
    test_int(values[2], 7);
    ecs_component_t labels = ecs_component_lookup("Labels");
    const char *const *words = ecs_get_cid(ecs_lookup("Words"), labels);
    const char *const *word_copy = ecs_get_cid(ecs_lookup("WordCopy"), labels);
    test_str(word_copy[0], "first");
    test_str(word_copy[1], "second");
    test_true(words[0] != word_copy[0]);
    test_true(words[1] != word_copy[1]);
    test_true(ecs_lookup("Point::x") != 0);
    test_true(ecs_lookup("Samples::points") != 0);
    ecs_script_instance_t *instance = ecs_script_instance_run("schema.si", "", NULL, NULL);
    test_true(instance != NULL);
    const char *invalid[] = {
        "struct Invalid { value: Missing }",          "struct Invalid { value: i32[0] }",
        "struct Invalid { value: i32[-1] }",          "struct Invalid { value: i32[1.5] }",
        "struct Invalid { value: i32[4294967296u] }", "struct Invalid { if true {} }"
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        ecs_script_eval_result_t result = { 0 };
        ecs_script_t *candidate = ecs_script_parse("schema.si", invalid[i], NULL, &result);
        test_true(candidate != NULL);
        test_int(ecs_script_instance_reload(instance, candidate, &result), -1);
        test_true(result.error != NULL);
        free(result.error);
        ecs_script_free(candidate);
        test_uint(ecs_component_lookup("Invalid"), 0);
        test_uint(ecs_lookup("Invalid"), 0);
    }
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Language_lexical_namespace_imports(void) {
    ecs_init();
    test_int(
        run("struct Value(x: i64)\n"
            "Space { struct Item(x: i32)\nSource { Item: {5} } }\n"
            "Other { struct Item(x: i64)\nSource { Item: {50} } }\n"
            "using Space\n"
            "fn read() -> i32 { Source[Item].x }\n"
            "fn identity(value: Item) -> Item { $value }\n"
            "template Model { prop p: Item = {7}\nItem: {$p.x} }\n"
            "with Item: {8} { Model First() }\n"
            "using Other\n"
            "Output { Value: {read()} }\n"
            "Second { Item: {identity({9}).x} }\n"
            "Third { using Space\nChild { Item: {11} } }\n"
            "Last { Item: {12} }\n"
            "Qualified { Value: {Space.Source[\"Space.Item\"].x} }"),
        0
    );
    ecs_component_t first = ecs_component_lookup("Space::Item");
    ecs_component_t second = ecs_component_lookup("Other::Item");
    test_true(first != 0 && second != 0 && first != second);
    test_int(*(const int32_t *)ecs_get_cid(ecs_lookup("First"), first), 7);
    test_false(ecs_has_cid(ecs_lookup("First"), second));
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Output"), ecs_component_lookup("Value")), 5);
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Second"), second), 9);
    test_int(*(const int32_t *)ecs_get_cid(ecs_lookup("Third::Child"), first), 11);
    test_int(*(const int64_t *)ecs_get_cid(ecs_lookup("Last"), second), 12);
    test_int(
        *(const int64_t *)ecs_get_cid(ecs_lookup("Qualified"), ecs_component_lookup("Value")),
        5
    );
    test_int(run("using Missing"), -1);
    ecs_fini();
}

void Language_native_annotations(void) {
    ecs_init();
    test_int(
        run("@brief \"two lines\\nwith text\"\n@color dark blue\n@flag\nOutput {}\n"
            "Plain {}\n@brief \"schema\"\nstruct Shape(x: i32)\n"
            "@brief \"definition\"\ntemplate Model { prop x: i32 = 1 }\n"
            "Model Instance()\n@brief old\n@brief new\nLast {}"),
        0
    );
    test_str(ecs_script_annotation(ecs_lookup("Output"), "brief"), "two lines\nwith text");
    test_str(ecs_script_annotation(ecs_lookup("Output"), "color"), "dark blue");
    test_str(ecs_script_annotation(ecs_lookup("Output"), "flag"), "");
    test_str(ecs_script_annotation(ecs_lookup("Shape"), "brief"), "schema");
    test_str(ecs_script_annotation(ecs_lookup("Model"), "brief"), "definition");
    test_str(ecs_script_annotation(ecs_lookup("Last"), "brief"), "new");
    test_true(ecs_script_annotation(ecs_lookup("Plain"), "brief") == NULL);
    test_true(ecs_script_annotation(ecs_lookup("Instance"), "brief") == NULL);
    test_true(ecs_script_annotation(0, "brief") == NULL);
    test_true(ecs_script_annotation(ecs_lookup("Output"), NULL) == NULL);
    test_int(run("@brief orphan"), -1);
    test_int(run("@brief misplaced\nconst x = 1"), -1);
    ecs_fini();
}

void Language_auto_override_and_staged_inheritance(void) {
    ecs_init();
    test_int(run("Invalid { auto_override | Name }"), -1);
    test_int(run("Invalid { auto_override | Abstract }"), -1);
    ecs_component_t shared = ecs_component_dynamic_init(&(
        ecs_dynamic_component_desc_t
    ){ .name = "SharedValue", .fields = "{ int32_t x; }", .inheritance = EcsInheritShared });
    test_true(shared != 0);
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "override.si",
        "struct OwnValue(x: i32)\n"
        "prefab Base { auto_override | SharedValue: {42}\nOwnValue: {19} }\n"
        "Child { (IsA, Base) }\nGrandchild { (IsA, Child) }\n"
        "prefab Other { SharedValue: {7} }\nOtherChild { (IsA, Other) }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_entity_t base = ecs_lookup("Base");
    ecs_entity_t child = ecs_lookup("Child");
    ecs_entity_t grandchild = ecs_lookup("Grandchild");
    test_true(ecs_has_cid_owned(child, shared));
    test_true(ecs_has_cid_owned(grandchild, shared));
    test_int(*(const int32_t *)ecs_get_cid(child, shared), 42);
    test_int(*(const int32_t *)ecs_get_cid(grandchild, shared), 42);
    test_int(*(const int32_t *)ecs_get_cid(child, ecs_component_lookup("OwnValue")), 19);
    test_false(ecs_has_cid_owned(ecs_lookup("OtherChild"), shared));
    test_int(*(const int32_t *)ecs_get_cid(ecs_lookup("OtherChild"), shared), 7);
    int32_t changed = 99;
    ecs_set_cid(base, shared, &changed);
    test_int(*(const int32_t *)ecs_get_cid(child, shared), 42);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Language_inheritance_publication_order(void) {
    ecs_init();
    ecs_script_instance_t *instance = ecs_script_instance_run(
        "bases.si",
        "struct Value(x: i32)\nprefab Root { Value: {42} }\n"
        "prefab Middle {}\nLeaf { (IsA, Middle) }\nMiddle { (IsA, Root) }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    ecs_component_t value = ecs_component_lookup("Value");
    ecs_entity_t leaf = ecs_lookup("Leaf");
    ecs_entity_t middle = ecs_lookup("Middle");
    test_true(ecs_has_cid_owned(middle, value));
    test_true(ecs_has_cid_owned(leaf, value));
    test_int(*(const int32_t *)ecs_get_cid(middle, value), 42);
    test_int(*(const int32_t *)ecs_get_cid(leaf, value), 42);
    ecs_script_t *candidate = ecs_script_parse(
        "bases.si",
        "struct Value(x: i32)\nprefab Root { Value: {12} }\n"
        "prefab Middle {}\nLeaf { (IsA, Middle) }\nMiddle { (IsA, Leaf) }",
        NULL,
        NULL
    );
    test_int(ecs_script_instance_reload(instance, candidate, NULL), -1);
    test_int(*(const int32_t *)ecs_get_cid(leaf, value), 42);
    test_uint(ecs_target_id(middle, ecs_rid(IsA)), ecs_lookup("Root"));
    ecs_script_free(candidate);
    ecs_script_instance_free(instance);
    ecs_fini();

    ecs_init();
    leaf = ecs_new_named(0, "Leaf");
    middle = ecs_new_named(0, "Middle");
    ecs_entity_t bridge = ecs_new_named(0, "Bridge");
    ecs_is_a(bridge, middle);
    instance = ecs_script_instance_run(
        "bridge.si",
        "struct Value(x: i32)\nprefab Root { Value: {31} }\n"
        "Leaf { (IsA, Bridge) }\nMiddle { (IsA, Root) }",
        NULL,
        NULL
    );
    test_true(instance != NULL);
    value = ecs_component_lookup("Value");
    test_true(ecs_has_cid_owned(leaf, value));
    test_int(*(const int32_t *)ecs_get_cid(leaf, value), 31);
    ecs_script_instance_free(instance);
    ecs_fini();
}

void Language_staged_inherited_presence(void) {
    ecs_init();
    test_true(ecs_tag_init("Marker") != 0);
    test_true(ecs_tag_init("Absent") != 0);
    test_int(
        run("struct Value(x: i32)\n"
            "prefab Base { Value: {42}\nMarker }\nChild { (IsA, Base) }\n"
            "Grandchild { (IsA, Child) }\n"
            "if Child?[Value] { Found {} } else { Wrong {} }\n"
            "if Grandchild?[Marker] { FoundTag {} } else { WrongTag {} }\n"
            "if Grandchild?[Absent] { WrongAbsent {} }"),
        0
    );
    test_true(ecs_has_cid(ecs_lookup("Child"), ecs_component_lookup("Value")));
    test_true(ecs_lookup("Found") != 0);
    test_true(ecs_lookup("FoundTag") != 0);
    test_uint(ecs_lookup("Wrong"), 0);
    test_uint(ecs_lookup("WrongTag"), 0);
    test_uint(ecs_lookup("WrongAbsent"), 0);
    ecs_fini();
}
