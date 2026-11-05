
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
void Eval_interpolated_ids(void);
void Eval_ir_execution(void);
void Eval_native_namespace(void);

// Testsuite 'File'
void File_run_and_errors(void);

// Testsuite 'Template'
void Template_defaults_and_arguments(void);
void Template_lexical_scope_and_nesting(void);
void Template_models_and_entity_props(void);
void Template_errors_and_recursion(void);
void Template_prefab_hierarchy(void);
void Template_anonymous_calls_in_scopes(void);
void Template_inheritance(void);
void Template_lexical_functions(void);

// Testsuite 'Document'
void Document_source_history(void);
void Document_failed_edit_preserves_history(void);
void Document_save_and_evaluate(void);
void Document_invalid_arguments(void);

// Testsuite 'Transaction'
void Transaction_failed_prepare_is_invisible(void);
void Transaction_commit_publishes_complete_state(void);
void Transaction_prepared_reads_and_native_types(void);

// Testsuite 'Instance'
void Instance_automatic_reactivity(void);
void Instance_unrelated_and_grouped_changes(void);
void Instance_failed_reload_keeps_active_graph(void);
void Instance_reload_replaces_dependencies(void);
void Instance_shared_inputs_and_conflicting_outputs(void);
void Instance_restore_and_preserve_host_changes(void);
void Instance_missing_reference_recovers(void);
void Instance_watched_files_and_includes(void);
void Instance_member_ownership_and_restore(void);
void Instance_member_dependencies(void);
void Instance_anonymous_identity(void);
void Instance_reuses_unchanged_regions(void);
void Instance_disjoint_self_dependency(void);
void Instance_rejects_incompatible_schema(void);
void Instance_inherited_member_dependencies(void);
void Instance_preserves_reentrant_invalidation(void);
void Instance_captured_regions(void);
void Instance_loop_regions(void);

// Testsuite 'Language'
void Language_typed_functions_and_captures(void);
void Language_mutable_function_locals(void);
void Language_collection_iteration_and_indexing(void);
void Language_includes_keep_lexical_declarations(void);
void Language_owning_strings_and_template_properties(void);
void Language_try_catch_rolls_back_scope(void);
void Language_native_expression_execution(void);
void Language_methods_scripts_arrays_and_typed_collections(void);
void Language_interpolation_formats(void);
void Language_struct_bodies_and_arrays(void);

// Testsuite 'Async'
void Async_wait_for_component_condition(void);
void Async_typed_host_task(void);
void Async_reload_keeps_identical_continuation(void);
void Async_cancellation_and_late_result(void);
void Async_loops_and_zero_delay(void);
void Async_controlled_clock(void);
void Async_clock_switch_preserves_remaining_delay(void);
void Async_inherited_templates(void);
void Async_environment_changes_restart_tasks(void);
void Async_definition_functions(void);
void Async_scoped_functions(void);
void Async_typed_errors_and_catch(void);
void Async_unhandled_error_is_inspectable(void);
void Async_scopes_and_while_continue(void);
void Async_repeated_world_lifetimes(void);
void Async_member_outputs_across_suspensions(void);
void Async_waits_on_inherited_value(void);
void Async_stabilizes_reactive_consumers(void);

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
    },
    {
        "interpolated_ids",
        Eval_interpolated_ids
    },
    {
        "ir_execution",
        Eval_ir_execution
    },
    {
        "native_namespace",
        Eval_native_namespace
    }
};

bake_test_case File_testcases[] = {
    {
        "run_and_errors",
        File_run_and_errors
    }
};

bake_test_case Template_testcases[] = {
    {
        "defaults_and_arguments",
        Template_defaults_and_arguments
    },
    {
        "lexical_scope_and_nesting",
        Template_lexical_scope_and_nesting
    },
    {
        "models_and_entity_props",
        Template_models_and_entity_props
    },
    {
        "errors_and_recursion",
        Template_errors_and_recursion
    },
    {
        "prefab_hierarchy",
        Template_prefab_hierarchy
    },
    {
        "anonymous_calls_in_scopes",
        Template_anonymous_calls_in_scopes
    },
    {
        "inheritance",
        Template_inheritance
    },
    {
        "lexical_functions",
        Template_lexical_functions
    }
};

bake_test_case Document_testcases[] = {
    {
        "source_history",
        Document_source_history
    },
    {
        "failed_edit_preserves_history",
        Document_failed_edit_preserves_history
    },
    {
        "save_and_evaluate",
        Document_save_and_evaluate
    },
    {
        "invalid_arguments",
        Document_invalid_arguments
    }
};

bake_test_case Transaction_testcases[] = {
    {
        "failed_prepare_is_invisible",
        Transaction_failed_prepare_is_invisible
    },
    {
        "commit_publishes_complete_state",
        Transaction_commit_publishes_complete_state
    },
    {
        "prepared_reads_and_native_types",
        Transaction_prepared_reads_and_native_types
    }
};

bake_test_case Instance_testcases[] = {
    {
        "automatic_reactivity",
        Instance_automatic_reactivity
    },
    {
        "unrelated_and_grouped_changes",
        Instance_unrelated_and_grouped_changes
    },
    {
        "failed_reload_keeps_active_graph",
        Instance_failed_reload_keeps_active_graph
    },
    {
        "reload_replaces_dependencies",
        Instance_reload_replaces_dependencies
    },
    {
        "shared_inputs_and_conflicting_outputs",
        Instance_shared_inputs_and_conflicting_outputs
    },
    {
        "restore_and_preserve_host_changes",
        Instance_restore_and_preserve_host_changes
    },
    {
        "missing_reference_recovers",
        Instance_missing_reference_recovers
    },
    {
        "watched_files_and_includes",
        Instance_watched_files_and_includes
    },
    {
        "member_ownership_and_restore",
        Instance_member_ownership_and_restore
    },
    {
        "member_dependencies",
        Instance_member_dependencies
    },
    {
        "anonymous_identity",
        Instance_anonymous_identity
    },
    {
        "reuses_unchanged_regions",
        Instance_reuses_unchanged_regions
    },
    {
        "disjoint_self_dependency",
        Instance_disjoint_self_dependency
    },
    {
        "rejects_incompatible_schema",
        Instance_rejects_incompatible_schema
    },
    {
        "inherited_member_dependencies",
        Instance_inherited_member_dependencies
    },
    {
        "preserves_reentrant_invalidation",
        Instance_preserves_reentrant_invalidation
    },
    {
        "captured_regions",
        Instance_captured_regions
    },
    {
        "loop_regions",
        Instance_loop_regions
    }
};

bake_test_case Language_testcases[] = {
    {
        "typed_functions_and_captures",
        Language_typed_functions_and_captures
    },
    {
        "mutable_function_locals",
        Language_mutable_function_locals
    },
    {
        "collection_iteration_and_indexing",
        Language_collection_iteration_and_indexing
    },
    {
        "includes_keep_lexical_declarations",
        Language_includes_keep_lexical_declarations
    },
    {
        "owning_strings_and_template_properties",
        Language_owning_strings_and_template_properties
    },
    {
        "try_catch_rolls_back_scope",
        Language_try_catch_rolls_back_scope
    },
    {
        "native_expression_execution",
        Language_native_expression_execution
    },
    {
        "methods_scripts_arrays_and_typed_collections",
        Language_methods_scripts_arrays_and_typed_collections
    },
    {
        "interpolation_formats",
        Language_interpolation_formats
    },
    {
        "struct_bodies_and_arrays",
        Language_struct_bodies_and_arrays
    }
};

bake_test_case Async_testcases[] = {
    {
        "wait_for_component_condition",
        Async_wait_for_component_condition
    },
    {
        "typed_host_task",
        Async_typed_host_task
    },
    {
        "reload_keeps_identical_continuation",
        Async_reload_keeps_identical_continuation
    },
    {
        "cancellation_and_late_result",
        Async_cancellation_and_late_result
    },
    {
        "loops_and_zero_delay",
        Async_loops_and_zero_delay
    },
    {
        "controlled_clock",
        Async_controlled_clock
    },
    {
        "clock_switch_preserves_remaining_delay",
        Async_clock_switch_preserves_remaining_delay
    },
    {
        "inherited_templates",
        Async_inherited_templates
    },
    {
        "environment_changes_restart_tasks",
        Async_environment_changes_restart_tasks
    },
    {
        "definition_functions",
        Async_definition_functions
    },
    {
        "scoped_functions",
        Async_scoped_functions
    },
    {
        "typed_errors_and_catch",
        Async_typed_errors_and_catch
    },
    {
        "unhandled_error_is_inspectable",
        Async_unhandled_error_is_inspectable
    },
    {
        "scopes_and_while_continue",
        Async_scopes_and_while_continue
    },
    {
        "repeated_world_lifetimes",
        Async_repeated_world_lifetimes
    },
    {
        "member_outputs_across_suspensions",
        Async_member_outputs_across_suspensions
    },
    {
        "waits_on_inherited_value",
        Async_waits_on_inherited_value
    },
    {
        "stabilizes_reactive_consumers",
        Async_stabilizes_reactive_consumers
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
        17,
        Eval_testcases
    },
    {
        "File",
        NULL,
        NULL,
        1,
        File_testcases
    },
    {
        "Template",
        NULL,
        NULL,
        8,
        Template_testcases
    },
    {
        "Document",
        NULL,
        NULL,
        4,
        Document_testcases
    },
    {
        "Transaction",
        NULL,
        NULL,
        3,
        Transaction_testcases
    },
    {
        "Instance",
        NULL,
        NULL,
        18,
        Instance_testcases
    },
    {
        "Language",
        NULL,
        NULL,
        10,
        Language_testcases
    },
    {
        "Async",
        NULL,
        NULL,
        18,
        Async_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs_script.test", argc, argv, suites, 9);
}
