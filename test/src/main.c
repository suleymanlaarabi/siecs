
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'entity'
void entity_create(void);
void entity_with(void);
void entity_is_a_moves_entity_to_type_with_base(void);
void entity_is_a_keeps_local_component_data(void);
void entity_is_a_same_target_is_noop(void);
void entity_is_a_different_target_creates_different_table(void);
void entity_is_with_multiple_depth(void);

// Testsuite 'component'
void component_reflection(void);
void component_on_add(void);
void component_lifecycle_ops_are_used_for_storage_moves(void);
void component_deferred_set_overwrite_preserves_lifecycle(void);
void component_add_with_required_uses_current_table_edge(void);
void component_add_with_required_uses_cached_multi_add_edge(void);
void component_add_with_required_emits_each_on_add_once(void);
void component_add_with_required_accepts_sixteen_component_plan(void);
void component_add_zeroes_reused_component_slot(void);
void component_many_tags_preserve_data_on_migration(void);
void component_many_tags_swap_remove_preserves_moved_entity_data(void);
void component_same_local_type_with_different_base_creates_different_tables(void);
void component_type_add_remove_preserves_base(void);
void component_table_resolves_recursive_base_components(void);

// Testsuite 'rest'
void rest_schema_returns_editor_contract(void);
void rest_entity_detail_returns_editor_state(void);
void rest_entity_children_returns_direct_children(void);
void rest_set_component_value_updates_entity(void);
void rest_set_component_value_rejects_missing_field(void);
void rest_set_component_value_rejects_unknown_field(void);
void rest_set_component_value_rejects_wrong_type(void);
void rest_set_component_value_rejects_missing_component(void);
void rest_set_component_value_rejects_non_reflected_component(void);

// Testsuite 'multi_world'
void multi_world_same_component_registered_in_two_worlds(void);
void multi_world_worlds_keep_independent_component_storage(void);
void multi_world_queries_only_see_their_world_tables(void);
void multi_world_fini_one_world_keeps_other_world_valid(void);
void multi_world_relations_remain_world_local(void);
void multi_world_resource_ids_are_not_overwritten_by_other_world(void);
void multi_world_typed_resource_macros_use_their_world_records(void);

// Testsuite 'resource'
void resource_set_get(void);
void resource_try_get_missing(void);
void resource_remove(void);
void resource_replace(void);
void resource_lifecycle_ops_are_used_for_set_move_and_remove(void);
void resource_from_system_c(void);
void resource_hooks(void);

// Testsuite 'childof'
void childof_kill_parent(void);
void childof_relation_without_cascade_keeps_related_alive(void);
void childof_relation_remove_updates_source(void);

// Testsuite 'query'
void query_terms_field_order(void);
void query_out_term_matches_and_returns_field(void);
void query_not_excludes_tables(void);
void query_excludes_disabled_by_default(void);
void query_can_include_disabled_explicitly(void);
void query_excludes_abstract_by_default(void);
void query_can_include_abstract_explicitly(void);
void query_optional_field_present(void);
void query_optional_field_missing_keeps_field_order(void);
void query_inout_optional_mutates_when_present(void);
void query_inherited_field_is_shared(void);
void query_override_field_is_owned(void);
void query_inout_does_not_match_shared_inherited_field(void);
void query_inout_optional_ignores_shared_inherited_field(void);
void query_is_a_matches_direct_base(void);
void query_is_a_excludes_other_bases(void);
void query_is_a_matches_transitive_base(void);
void query_is_a_with_component_terms(void);
void query_ids_stay_valid_after_temporary_query_fini(void);

// Testsuite 'system'
void system_run(void);
void system_phase_order(void);
void system_start_phases_run_once(void);
void system_after_order(void);
void system_enable(void);
void system_without_query_runs_once(void);
void system_user_data_is_passed_and_destroyed(void);
void system_callback_can_advance_iterator(void);
void system_skips_disabled_by_default(void);
void system_can_run_on_disabled_when_requested(void);
void system_defers_structural_changes_until_iteration_end(void);
void system_flushes_between_ordered_systems(void);
void system_manual_defer_coalesces_to_final_state(void);
void system_deferred_many_sets_survive_arena_growth(void);
void system_deferred_set_overwrite_keeps_latest_value(void);
void system_deferred_set_adds_required_components(void);
void system_quit_makes_progress_return_false(void);

// Testsuite 'observer'
void observer_enable(void);
void observer_skips_disabled_by_default(void);
void observer_can_match_disabled_when_requested(void);
void observer_on_remove_runs_when_entity_is_killed(void);
void observer_event_register_reserves_static_ids(void);

// Testsuite 'module'
void module_import_registers_runtime(void);
void module_enable(void);
void module_disabled_import(void);
void module_double_import_is_noop(void);
void module_reimport_after_world_fini(void);

// Testsuite 'string'
void string_init(void);
void string_append(void);
void string_trim(void);
void string_starts_ends_with(void);

// Testsuite 'lexer'
void lexer_single_char_tokens(void);
void lexer_two_char_tokens(void);
void lexer_keywords_and_identifiers(void);
void lexer_numbers(void);
void lexer_strings(void);
void lexer_unknown(void);

// Testsuite 'vec'
void vec_u16_contains_and_remove(void);

bake_test_case entity_testcases[] = {
    {
        "create",
        entity_create
    },
    {
        "with",
        entity_with
    },
    {
        "is_a_moves_entity_to_type_with_base",
        entity_is_a_moves_entity_to_type_with_base
    },
    {
        "is_a_keeps_local_component_data",
        entity_is_a_keeps_local_component_data
    },
    {
        "is_a_same_target_is_noop",
        entity_is_a_same_target_is_noop
    },
    {
        "is_a_different_target_creates_different_table",
        entity_is_a_different_target_creates_different_table
    },
    {
        "is_with_multiple_depth",
        entity_is_with_multiple_depth
    }
};

bake_test_case component_testcases[] = {
    {
        "reflection",
        component_reflection
    },
    {
        "on_add",
        component_on_add
    },
    {
        "lifecycle_ops_are_used_for_storage_moves",
        component_lifecycle_ops_are_used_for_storage_moves
    },
    {
        "deferred_set_overwrite_preserves_lifecycle",
        component_deferred_set_overwrite_preserves_lifecycle
    },
    {
        "add_with_required_uses_current_table_edge",
        component_add_with_required_uses_current_table_edge
    },
    {
        "add_with_required_uses_cached_multi_add_edge",
        component_add_with_required_uses_cached_multi_add_edge
    },
    {
        "add_with_required_emits_each_on_add_once",
        component_add_with_required_emits_each_on_add_once
    },
    {
        "add_with_required_accepts_sixteen_component_plan",
        component_add_with_required_accepts_sixteen_component_plan
    },
    {
        "add_zeroes_reused_component_slot",
        component_add_zeroes_reused_component_slot
    },
    {
        "many_tags_preserve_data_on_migration",
        component_many_tags_preserve_data_on_migration
    },
    {
        "many_tags_swap_remove_preserves_moved_entity_data",
        component_many_tags_swap_remove_preserves_moved_entity_data
    },
    {
        "same_local_type_with_different_base_creates_different_tables",
        component_same_local_type_with_different_base_creates_different_tables
    },
    {
        "type_add_remove_preserves_base",
        component_type_add_remove_preserves_base
    },
    {
        "table_resolves_recursive_base_components",
        component_table_resolves_recursive_base_components
    }
};

bake_test_case rest_testcases[] = {
    {
        "schema_returns_editor_contract",
        rest_schema_returns_editor_contract
    },
    {
        "entity_detail_returns_editor_state",
        rest_entity_detail_returns_editor_state
    },
    {
        "entity_children_returns_direct_children",
        rest_entity_children_returns_direct_children
    },
    {
        "set_component_value_updates_entity",
        rest_set_component_value_updates_entity
    },
    {
        "set_component_value_rejects_missing_field",
        rest_set_component_value_rejects_missing_field
    },
    {
        "set_component_value_rejects_unknown_field",
        rest_set_component_value_rejects_unknown_field
    },
    {
        "set_component_value_rejects_wrong_type",
        rest_set_component_value_rejects_wrong_type
    },
    {
        "set_component_value_rejects_missing_component",
        rest_set_component_value_rejects_missing_component
    },
    {
        "set_component_value_rejects_non_reflected_component",
        rest_set_component_value_rejects_non_reflected_component
    }
};

bake_test_case multi_world_testcases[] = {
    {
        "same_component_registered_in_two_worlds",
        multi_world_same_component_registered_in_two_worlds
    },
    {
        "worlds_keep_independent_component_storage",
        multi_world_worlds_keep_independent_component_storage
    },
    {
        "queries_only_see_their_world_tables",
        multi_world_queries_only_see_their_world_tables
    },
    {
        "fini_one_world_keeps_other_world_valid",
        multi_world_fini_one_world_keeps_other_world_valid
    },
    {
        "relations_remain_world_local",
        multi_world_relations_remain_world_local
    },
    {
        "resource_ids_are_not_overwritten_by_other_world",
        multi_world_resource_ids_are_not_overwritten_by_other_world
    },
    {
        "typed_resource_macros_use_their_world_records",
        multi_world_typed_resource_macros_use_their_world_records
    }
};

bake_test_case resource_testcases[] = {
    {
        "set_get",
        resource_set_get
    },
    {
        "try_get_missing",
        resource_try_get_missing
    },
    {
        "remove",
        resource_remove
    },
    {
        "replace",
        resource_replace
    },
    {
        "lifecycle_ops_are_used_for_set_move_and_remove",
        resource_lifecycle_ops_are_used_for_set_move_and_remove
    },
    {
        "from_system_c",
        resource_from_system_c
    },
    {
        "hooks",
        resource_hooks
    }
};

bake_test_case childof_testcases[] = {
    {
        "kill_parent",
        childof_kill_parent
    },
    {
        "relation_without_cascade_keeps_related_alive",
        childof_relation_without_cascade_keeps_related_alive
    },
    {
        "relation_remove_updates_source",
        childof_relation_remove_updates_source
    }
};

bake_test_case query_testcases[] = {
    {
        "terms_field_order",
        query_terms_field_order
    },
    {
        "out_term_matches_and_returns_field",
        query_out_term_matches_and_returns_field
    },
    {
        "not_excludes_tables",
        query_not_excludes_tables
    },
    {
        "excludes_disabled_by_default",
        query_excludes_disabled_by_default
    },
    {
        "can_include_disabled_explicitly",
        query_can_include_disabled_explicitly
    },
    {
        "excludes_abstract_by_default",
        query_excludes_abstract_by_default
    },
    {
        "can_include_abstract_explicitly",
        query_can_include_abstract_explicitly
    },
    {
        "optional_field_present",
        query_optional_field_present
    },
    {
        "optional_field_missing_keeps_field_order",
        query_optional_field_missing_keeps_field_order
    },
    {
        "inout_optional_mutates_when_present",
        query_inout_optional_mutates_when_present
    },
    {
        "inherited_field_is_shared",
        query_inherited_field_is_shared
    },
    {
        "override_field_is_owned",
        query_override_field_is_owned
    },
    {
        "inout_does_not_match_shared_inherited_field",
        query_inout_does_not_match_shared_inherited_field
    },
    {
        "inout_optional_ignores_shared_inherited_field",
        query_inout_optional_ignores_shared_inherited_field
    },
    {
        "is_a_matches_direct_base",
        query_is_a_matches_direct_base
    },
    {
        "is_a_excludes_other_bases",
        query_is_a_excludes_other_bases
    },
    {
        "is_a_matches_transitive_base",
        query_is_a_matches_transitive_base
    },
    {
        "is_a_with_component_terms",
        query_is_a_with_component_terms
    },
    {
        "ids_stay_valid_after_temporary_query_fini",
        query_ids_stay_valid_after_temporary_query_fini
    }
};

bake_test_case system_testcases[] = {
    {
        "run",
        system_run
    },
    {
        "phase_order",
        system_phase_order
    },
    {
        "start_phases_run_once",
        system_start_phases_run_once
    },
    {
        "after_order",
        system_after_order
    },
    {
        "enable",
        system_enable
    },
    {
        "without_query_runs_once",
        system_without_query_runs_once
    },
    {
        "user_data_is_passed_and_destroyed",
        system_user_data_is_passed_and_destroyed
    },
    {
        "callback_can_advance_iterator",
        system_callback_can_advance_iterator
    },
    {
        "skips_disabled_by_default",
        system_skips_disabled_by_default
    },
    {
        "can_run_on_disabled_when_requested",
        system_can_run_on_disabled_when_requested
    },
    {
        "defers_structural_changes_until_iteration_end",
        system_defers_structural_changes_until_iteration_end
    },
    {
        "flushes_between_ordered_systems",
        system_flushes_between_ordered_systems
    },
    {
        "manual_defer_coalesces_to_final_state",
        system_manual_defer_coalesces_to_final_state
    },
    {
        "deferred_many_sets_survive_arena_growth",
        system_deferred_many_sets_survive_arena_growth
    },
    {
        "deferred_set_overwrite_keeps_latest_value",
        system_deferred_set_overwrite_keeps_latest_value
    },
    {
        "deferred_set_adds_required_components",
        system_deferred_set_adds_required_components
    },
    {
        "quit_makes_progress_return_false",
        system_quit_makes_progress_return_false
    }
};

bake_test_case observer_testcases[] = {
    {
        "enable",
        observer_enable
    },
    {
        "skips_disabled_by_default",
        observer_skips_disabled_by_default
    },
    {
        "can_match_disabled_when_requested",
        observer_can_match_disabled_when_requested
    },
    {
        "on_remove_runs_when_entity_is_killed",
        observer_on_remove_runs_when_entity_is_killed
    },
    {
        "event_register_reserves_static_ids",
        observer_event_register_reserves_static_ids
    }
};

bake_test_case module_testcases[] = {
    {
        "import_registers_runtime",
        module_import_registers_runtime
    },
    {
        "enable",
        module_enable
    },
    {
        "disabled_import",
        module_disabled_import
    },
    {
        "double_import_is_noop",
        module_double_import_is_noop
    },
    {
        "reimport_after_world_fini",
        module_reimport_after_world_fini
    }
};

bake_test_case string_testcases[] = {
    {
        "init",
        string_init
    },
    {
        "append",
        string_append
    },
    {
        "trim",
        string_trim
    },
    {
        "starts_ends_with",
        string_starts_ends_with
    }
};

bake_test_case lexer_testcases[] = {
    {
        "single_char_tokens",
        lexer_single_char_tokens
    },
    {
        "two_char_tokens",
        lexer_two_char_tokens
    },
    {
        "keywords_and_identifiers",
        lexer_keywords_and_identifiers
    },
    {
        "numbers",
        lexer_numbers
    },
    {
        "strings",
        lexer_strings
    },
    {
        "unknown",
        lexer_unknown
    }
};

bake_test_case vec_testcases[] = {
    {
        "u16_contains_and_remove",
        vec_u16_contains_and_remove
    }
};


static bake_test_suite suites[] = {
    {
        "entity",
        NULL,
        NULL,
        7,
        entity_testcases
    },
    {
        "component",
        NULL,
        NULL,
        14,
        component_testcases
    },
    {
        "rest",
        NULL,
        NULL,
        9,
        rest_testcases
    },
    {
        "multi_world",
        NULL,
        NULL,
        7,
        multi_world_testcases
    },
    {
        "resource",
        NULL,
        NULL,
        7,
        resource_testcases
    },
    {
        "childof",
        NULL,
        NULL,
        3,
        childof_testcases
    },
    {
        "query",
        NULL,
        NULL,
        19,
        query_testcases
    },
    {
        "system",
        NULL,
        NULL,
        17,
        system_testcases
    },
    {
        "observer",
        NULL,
        NULL,
        5,
        observer_testcases
    },
    {
        "module",
        NULL,
        NULL,
        5,
        module_testcases
    },
    {
        "string",
        NULL,
        NULL,
        4,
        string_testcases
    },
    {
        "lexer",
        NULL,
        NULL,
        6,
        lexer_testcases
    },
    {
        "vec",
        NULL,
        NULL,
        1,
        vec_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.test", argc, argv, suites, 13);
}
