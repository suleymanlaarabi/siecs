
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'public'
void public_metadata_and_entity_introspection(void);

// Testsuite 'entity'
void entity_create(void);
void entity_reuses_id_with_new_generation_after_kill(void);
void entity_new_no_reuse_uses_new_index(void);
void entity_new_no_reuse_ignores_multiple_free_indices(void);
void entity_create_has_default_name(void);
void entity_explicit_name_overrides_default(void);
void entity_with(void);
void entity_is_a_moves_entity_to_type_with_base(void);
void entity_is_a_materializes_owned_components(void);
void entity_is_a_keeps_local_component_data(void);
void entity_is_a_same_target_is_noop(void);
void entity_is_a_different_target_creates_different_table(void);
void entity_is_a_marks_base_abstract(void);
void entity_deferred_is_a_marks_base_abstract(void);
void entity_is_with_multiple_depth(void);

// Testsuite 'component'
void component_reflection(void);
void component_dynamic_component_layout_and_info(void);
void component_component_info_is_stable(void);
void component_name_returns_registered_name(void);
void component_on_add(void);
void component_lifecycle_ops_are_used_for_storage_moves(void);
void component_deferred_set_overwrite_preserves_lifecycle(void);
void component_add_with_required_uses_current_table_edge(void);
void component_with_accepts_multiple_requirements(void);
void component_add_with_required_uses_cached_multi_add_edge(void);
void component_add_with_required_emits_each_on_add_once(void);
void component_add_with_required_accepts_sixteen_component_plan(void);
void component_add_with_required_deduplicates_branches(void);
void component_add_zeroes_reused_component_slot(void);
void component_double_add_and_remove_are_noops(void);
void component_many_tags_preserve_data_on_migration(void);
void component_many_tags_swap_remove_preserves_moved_entity_data(void);
void component_same_local_type_with_different_base_creates_different_tables(void);
void component_type_add_remove_preserves_base(void);
void component_type_pairs_are_sorted_replaced_and_removed_atomically(void);
void component_table_index_indexes_generic_pairs(void);
void component_tag_components_have_no_storage(void);
void component_try_get_handles_missing_and_inherited(void);
void component_table_type_tracks_data_columns(void);
void component_table_index_resize_preserves_type_hashes(void);
void component_table_resolves_recursive_base_components(void);

// Testsuite 'resource'
void resource_set_get(void);
void resource_delta_time(void);
void resource_name_lookup(void);
void resource_try_get_missing(void);
void resource_remove(void);
void resource_replace(void);
void resource_lifecycle_ops_are_used_for_set_move_and_remove(void);
void resource_from_system_c(void);
void resource_hooks(void);

// Testsuite 'childof'
void childof_kill_parent(void);
void childof_dense_retarget_without_migration(void);
void childof_dense_retarget_keeps_reverse_sources(void);
void childof_dense_delete_policies(void);
void childof_bydepth_depth_and_cascade(void);
void childof_bydepth_targets_return_records(void);
void childof_bydepth_reparent_updates_subtree(void);
void childof_cascade_cache_accepts_new_depth(void);
void childof_bytarget_exact_query_and_retarget(void);
void childof_bytarget_exact_query_spans_tables(void);
void childof_bytarget_order_by_target(void);
void childof_bytarget_delete_policies(void);
void childof_bytarget_keeps_target_generation(void);
void childof_multiple_bytarget_relations_share_one_type(void);
void childof_relation_query_presence_optional_and_not(void);
void childof_up_finds_nearest_ancestor(void);
void childof_up_optional_returns_null(void);
void childof_deferred_relation_keeps_last_target(void);
void childof_relation_observer_events(void);
void childof_type_layout_stays_compact(void);
void childof_query_slot_reuses_component_and_relation_terms(void);
void childof_relation_only_system_and_observer(void);

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
void query_fields_refresh_after_table_growth(void);

// Testsuite 'system'
void system_run(void);
void system_name_returns_registered_name(void);
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
void system_deferred_changes_coalesce_by_component(void);
void system_quit_makes_progress_return_false(void);

// Testsuite 'observer'
void observer_enable(void);
void observer_skips_disabled_by_default(void);
void observer_can_match_disabled_when_requested(void);
void observer_on_remove_runs_when_entity_is_killed(void);

// Testsuite 'module'
void module_import_registers_runtime(void);
void module_name_returns_imported_name(void);
void module_enable(void);
void module_disabled_import(void);
void module_double_import_is_noop(void);

bake_test_case public_testcases[] = {
    {
        "metadata_and_entity_introspection",
        public_metadata_and_entity_introspection
    }
};

bake_test_case entity_testcases[] = {
    {
        "create",
        entity_create
    },
    {
        "reuses_id_with_new_generation_after_kill",
        entity_reuses_id_with_new_generation_after_kill
    },
    {
        "new_no_reuse_uses_new_index",
        entity_new_no_reuse_uses_new_index
    },
    {
        "new_no_reuse_ignores_multiple_free_indices",
        entity_new_no_reuse_ignores_multiple_free_indices
    },
    {
        "create_has_default_name",
        entity_create_has_default_name
    },
    {
        "explicit_name_overrides_default",
        entity_explicit_name_overrides_default
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
        "is_a_materializes_owned_components",
        entity_is_a_materializes_owned_components
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
        "is_a_marks_base_abstract",
        entity_is_a_marks_base_abstract
    },
    {
        "deferred_is_a_marks_base_abstract",
        entity_deferred_is_a_marks_base_abstract
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
        "dynamic_component_layout_and_info",
        component_dynamic_component_layout_and_info
    },
    {
        "component_info_is_stable",
        component_component_info_is_stable
    },
    {
        "name_returns_registered_name",
        component_name_returns_registered_name
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
        "with_accepts_multiple_requirements",
        component_with_accepts_multiple_requirements
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
        "add_with_required_deduplicates_branches",
        component_add_with_required_deduplicates_branches
    },
    {
        "add_zeroes_reused_component_slot",
        component_add_zeroes_reused_component_slot
    },
    {
        "double_add_and_remove_are_noops",
        component_double_add_and_remove_are_noops
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
        "type_pairs_are_sorted_replaced_and_removed_atomically",
        component_type_pairs_are_sorted_replaced_and_removed_atomically
    },
    {
        "table_index_indexes_generic_pairs",
        component_table_index_indexes_generic_pairs
    },
    {
        "tag_components_have_no_storage",
        component_tag_components_have_no_storage
    },
    {
        "try_get_handles_missing_and_inherited",
        component_try_get_handles_missing_and_inherited
    },
    {
        "table_type_tracks_data_columns",
        component_table_type_tracks_data_columns
    },
    {
        "table_index_resize_preserves_type_hashes",
        component_table_index_resize_preserves_type_hashes
    },
    {
        "table_resolves_recursive_base_components",
        component_table_resolves_recursive_base_components
    }
};

bake_test_case resource_testcases[] = {
    {
        "set_get",
        resource_set_get
    },
    {
        "delta_time",
        resource_delta_time
    },
    {
        "name_lookup",
        resource_name_lookup
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
        "dense_retarget_without_migration",
        childof_dense_retarget_without_migration
    },
    {
        "dense_retarget_keeps_reverse_sources",
        childof_dense_retarget_keeps_reverse_sources
    },
    {
        "dense_delete_policies",
        childof_dense_delete_policies
    },
    {
        "bydepth_depth_and_cascade",
        childof_bydepth_depth_and_cascade
    },
    {
        "bydepth_targets_return_records",
        childof_bydepth_targets_return_records
    },
    {
        "bydepth_reparent_updates_subtree",
        childof_bydepth_reparent_updates_subtree
    },
    {
        "cascade_cache_accepts_new_depth",
        childof_cascade_cache_accepts_new_depth
    },
    {
        "bytarget_exact_query_and_retarget",
        childof_bytarget_exact_query_and_retarget
    },
    {
        "bytarget_exact_query_spans_tables",
        childof_bytarget_exact_query_spans_tables
    },
    {
        "bytarget_order_by_target",
        childof_bytarget_order_by_target
    },
    {
        "bytarget_delete_policies",
        childof_bytarget_delete_policies
    },
    {
        "bytarget_keeps_target_generation",
        childof_bytarget_keeps_target_generation
    },
    {
        "multiple_bytarget_relations_share_one_type",
        childof_multiple_bytarget_relations_share_one_type
    },
    {
        "relation_query_presence_optional_and_not",
        childof_relation_query_presence_optional_and_not
    },
    {
        "up_finds_nearest_ancestor",
        childof_up_finds_nearest_ancestor
    },
    {
        "up_optional_returns_null",
        childof_up_optional_returns_null
    },
    {
        "deferred_relation_keeps_last_target",
        childof_deferred_relation_keeps_last_target
    },
    {
        "relation_observer_events",
        childof_relation_observer_events
    },
    {
        "type_layout_stays_compact",
        childof_type_layout_stays_compact
    },
    {
        "query_slot_reuses_component_and_relation_terms",
        childof_query_slot_reuses_component_and_relation_terms
    },
    {
        "relation_only_system_and_observer",
        childof_relation_only_system_and_observer
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
    },
    {
        "fields_refresh_after_table_growth",
        query_fields_refresh_after_table_growth
    }
};

bake_test_case system_testcases[] = {
    {
        "run",
        system_run
    },
    {
        "name_returns_registered_name",
        system_name_returns_registered_name
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
        "deferred_changes_coalesce_by_component",
        system_deferred_changes_coalesce_by_component
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
    }
};

bake_test_case module_testcases[] = {
    {
        "import_registers_runtime",
        module_import_registers_runtime
    },
    {
        "name_returns_imported_name",
        module_name_returns_imported_name
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
    }
};


static bake_test_suite suites[] = {
    {
        "public",
        NULL,
        NULL,
        1,
        public_testcases
    },
    {
        "entity",
        NULL,
        NULL,
        15,
        entity_testcases
    },
    {
        "component",
        NULL,
        NULL,
        26,
        component_testcases
    },
    {
        "resource",
        NULL,
        NULL,
        9,
        resource_testcases
    },
    {
        "childof",
        NULL,
        NULL,
        22,
        childof_testcases
    },
    {
        "query",
        NULL,
        NULL,
        20,
        query_testcases
    },
    {
        "system",
        NULL,
        NULL,
        19,
        system_testcases
    },
    {
        "observer",
        NULL,
        NULL,
        4,
        observer_testcases
    },
    {
        "module",
        NULL,
        NULL,
        5,
        module_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.test", argc, argv, suites, 9);
}
