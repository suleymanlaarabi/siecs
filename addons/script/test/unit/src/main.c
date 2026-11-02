
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'Parser'
void Parser_entity_scope(void);
void Parser_expression_precedence(void);
void Parser_template_and_async(void);
void Parser_diagnostics(void);
void Parser_depth_limit(void);
void Parser_repeated_parse(void);
void Parser_nested_script(void);

// Testsuite 'Eval'
void Eval_native_entities(void);
void Eval_native_component_values(void);
void Eval_script_struct(void);
void Eval_native_tags(void);
void Eval_conditions_loops(void);
void Eval_registered_relation(void);
void Eval_parse_error_does_not_mutate(void);
void Eval_unknown_component_is_error(void);
void Eval_interpolated_names(void);
void Eval_reusable_runtime(void);
void Eval_component_member_reads(void);
void Eval_native_lifecycle_and_observers(void);
void Eval_value_snapshot_and_runtime_error_recovery(void);
void Eval_owned_component_copy(void);

bake_test_case Parser_testcases[] = {
    {
        "entity_scope",
        Parser_entity_scope
    },
    {
        "expression_precedence",
        Parser_expression_precedence
    },
    {
        "template_and_async",
        Parser_template_and_async
    },
    {
        "diagnostics",
        Parser_diagnostics
    },
    {
        "depth_limit",
        Parser_depth_limit
    },
    {
        "repeated_parse",
        Parser_repeated_parse
    },
    {
        "nested_script",
        Parser_nested_script
    }
};

bake_test_case Eval_testcases[] = {
    {
        "native_entities",
        Eval_native_entities
    },
    {
        "native_component_values",
        Eval_native_component_values
    },
    {
        "script_struct",
        Eval_script_struct
    },
    {
        "native_tags",
        Eval_native_tags
    },
    {
        "conditions_loops",
        Eval_conditions_loops
    },
    {
        "registered_relation",
        Eval_registered_relation
    },
    {
        "parse_error_does_not_mutate",
        Eval_parse_error_does_not_mutate
    },
    {
        "unknown_component_is_error",
        Eval_unknown_component_is_error
    },
    {
        "interpolated_names",
        Eval_interpolated_names
    },
    {
        "reusable_runtime",
        Eval_reusable_runtime
    },
    {
        "component_member_reads",
        Eval_component_member_reads
    },
    {
        "native_lifecycle_and_observers",
        Eval_native_lifecycle_and_observers
    },
    {
        "value_snapshot_and_runtime_error_recovery",
        Eval_value_snapshot_and_runtime_error_recovery
    },
    {
        "owned_component_copy",
        Eval_owned_component_copy
    }
};


static bake_test_suite suites[] = {
    {
        "Parser",
        NULL,
        NULL,
        7,
        Parser_testcases
    },
    {
        "Eval",
        NULL,
        NULL,
        14,
        Eval_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs_script.test", argc, argv, suites, 2);
}
