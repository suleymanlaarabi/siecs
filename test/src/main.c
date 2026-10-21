
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'public'
void public_metadata_and_entity_introspection(void);
void public_scene_save_memory_roundtrip(void);
void public_scene_invalid_load_is_non_destructive(void);
void public_scene_failed_apply_rolls_back(void);
void public_scene_reflected_codec_roundtrip(void);
void public_scene_unsupported_pointer_is_consistent(void);
void public_scene_binary_fixture_and_truncation(void);
void public_scene_unaligned_component_ids(void);

// Testsuite 'registry'
void registry_component_names(void);
void registry_relation_names_and_namespaces(void);
void registry_growth_and_owned_names(void);
void registry_duplicate_names_keep_first(void);
void registry_world_restart(void);
void registry_type_slots_preserve_pointers(void);

// Testsuite 'naming'
void naming_paths_and_scopes(void);
void naming_rename_updates_descendant_paths(void);
void naming_set_component_copies_and_removes(void);
void naming_collision_deletes_previous_subtree(void);
void naming_reparent_and_root_collision(void);
void naming_protects_ancestors_and_cycles(void);
void naming_invalid_input_is_non_destructive(void);
void naming_anonymous_segments_and_generations(void);
void naming_deferred_final_name_and_parent(void);
void naming_deferred_creation_reuses_pending_paths(void);
void naming_deferred_remove_kill_and_recreate(void);
void naming_deferred_siblings_are_all_owned_by_parent(void);
void naming_set_event_observes_final_identity(void);
void naming_collision_callbacks_can_allocate(void);
void naming_prefab_instances_have_local_child_names(void);
void naming_scene_roundtrip_preserves_duplicate_local_names(void);
void naming_hash_growth_tombstones_and_restart(void);

// Testsuite 'entity'
void entity_create(void);
void entity_add_components_after_world_restart(void);
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
void entity_propagate_add_marks_childof_subtree(void);
void entity_is_a_marks_childof_subtree_abstract(void);
void entity_is_a_repeated_base_abstract_is_noop(void);
void entity_deferred_is_a_marks_childof_subtree_abstract(void);
void entity_default_query_excludes_abstract_childof_subtree(void);
void entity_is_with_multiple_depth(void);

// Testsuite 'component'
void component_reflection(void);
void component_dynamic_component_layout_and_info(void);
void component_component_info_is_stable(void);
void component_name_returns_registered_name(void);
void component_on_add(void);
void component_modified_does_not_call_on_set_hook(void);
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
void component_resolution_keeps_tags_and_local_names(void);
void component_set_hook_can_relocate_storage(void);
void component_owned_tag_copy_hooks_survive_inheritance(void);

// Testsuite 'resource'
void resource_set_get(void);
void resource_delta_time(void);
void resource_name_lookup(void);
void resource_try_get_missing(void);
void resource_zero_sized_presence_uses_storage_pointer(void);
void resource_remove(void);
void resource_replace(void);
void resource_lifecycle_ops_are_used_for_set_move_and_remove(void);
void resource_transfer_hook_combinations(void);
void resource_from_system_c(void);
void resource_hooks(void);
void resource_fini_runs_after_component_remove(void);
void resource_fini_uses_reverse_registration_order(void);
void resource_reflection_info_and_failure(void);

// Testsuite 'world'
void world_at_fini_runs_all_in_reverse_registration_order(void);
void world_at_fini_is_cleared_on_restart(void);

// Testsuite 'rest'
void rest_is_a_and_child_of_same_target_routes(void);
void rest_enum_schema_and_component_roundtrip(void);
void rest_in_process_body_limit(void);
void rest_invalid_route_parameters(void);

// Testsuite 'rest_reflection'
void rest_reflection_reflected_schema_graph_and_roles(void);
void rest_reflection_reflected_patch_and_resources(void);

// Testsuite 'childof'
void childof_relation_batch_views(void);
void childof_is_a_and_child_of_same_target_queries(void);
void childof_kill_parent(void);
void childof_with_relation_adds_default_relation(void);
void childof_with_relation_preserves_existing_relation(void);
void childof_with_relation_applies_through_component_requirement(void);
void childof_with_relation_deferred_add(void);
void childof_with_relation_deferred_explicit_relation_wins(void);
void childof_with_relation_double_add_does_not_restore_removed_relation(void);
void childof_dense_retarget_without_migration(void);
void childof_dense_retarget_keeps_reverse_sources(void);
void childof_dense_replacement_during_flush_keeps_reverse_source(void);
void childof_dense_unrelate_keeps_source_indices(void);
void childof_dense_unrelate_updates_swapped_reverse_index(void);
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
void childof_generic_isa_has_relation(void);
void childof_generic_isa_target(void);
void childof_generic_isa_retarget(void);
void childof_generic_isa_unrelate(void);
void childof_generic_isa_cycle(void);
void childof_acyclic_dense_cycle_aborts(void);
void childof_acyclic_by_target_cycle_aborts(void);
void childof_generic_isa_deferred(void);
void childof_isa_clones_child_tree(void);
void childof_isa_clone_snapshot_and_same_target_noop(void);
void childof_isa_clones_child_tree_deferred(void);
void childof_generic_isa_relation_event(void);
void childof_relation_observer_events(void);
void childof_relation_observer_reports_relation_without_component(void);
void childof_type_layout_stays_compact(void);
void childof_query_slot_reuses_component_and_relation_terms(void);
void childof_relation_only_system_and_observer(void);
void childof_new_child_of_abstract_parent_is_abstract(void);
void childof_new_child_of_normal_parent_is_not_abstract(void);
void childof_isa_instantiated_children_are_not_abstract(void);
void childof_propagate_add_rejects_cyclic_relation_in_debug(void);
void childof_propagate_add_rejects_by_target_relation(void);

// Testsuite 'query'
void query_tags_keep_presence_without_data(void);
void query_full_descriptor_and_empty_candidates(void);
void query_sorted_growth_preserves_fields_and_equal_order(void);
void query_terms_field_order(void);
void query_resources_do_not_affect_matching_or_fields(void);
void query_resource_only_matches_all_tables(void);
void query_count_matches_current_query_entities(void);
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
void query_inherited_fields_refresh_after_base_table_growth(void);

// Testsuite 'system'
void system_run(void);
void system_parallel_independent_callbacks(void);
void system_parallel_worker_context_is_deferred(void);
void system_parallel_query_table_conflicts(void);
void system_indexed_queries_detect_late_overlap(void);
void system_batches_invalidate_after_table_creation(void);
void system_main_thread_only(void);
void system_parallel_after_is_a_barrier(void);
void system_resource_access_conflicts(void);
void system_parallel_structural_changes_flush_at_barrier(void);
void system_manual_run_is_synchronous_with_workers(void);
void system_worker_auto_and_reinit(void);
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
void system_interval_throttles_progress(void);
void system_interval_manual_execution_is_forced(void);
void system_interval_resets_on_enable_transition(void);
void system_interval_does_not_gate_start_phase(void);
void system_custom_phase(void);
void system_default_phase_is_on_update(void);
void system_custom_phase_after_only(void);
void system_custom_phase_unconstrained(void);
void system_parallel_portions_cover_rows(void);
void system_parallel_portions_empty_and_manual(void);
void system_dependency_releases_without_group_barrier(void);
void system_parallel_creation_grows_stable_index(void);
void system_parallel_commands_have_logical_order(void);
void system_publication_rechecks_same_phase_conflicts(void);
void system_parallel_shared_and_up_fields(void);
void system_parallel_relation_views_follow_portions(void);
void system_graph_keeps_all_after_dependencies(void);
void system_parallel_rejects_tasks(void);
void system_parallel_rejects_resource_writes(void);
void system_parallel_rejects_main_thread(void);
void system_parallel_rejects_no_defer(void);
void system_parallel_rejects_ordered_queries(void);
void system_parallel_rejects_query_destruction(void);
void system_shared_reads_conflict_across_tables(void);
void system_serial_commands_keep_registration_order(void);
void system_empty_portion_releases_main_thread_successor(void);
void system_parallel_creation_crosses_page_boundary(void);
void system_parallel_returns_unused_recycled_ids(void);
void system_parallel_returns_unused_fresh_ids(void);
void system_parallel_reservations_suspend_for_exclusive_callbacks(void);
void system_parallel_batched_commands_keep_logical_order(void);

// Testsuite 'observer'
void observer_global_registration_does_not_duplicate_queries(void);
void observer_enable(void);
void observer_skips_disabled_by_default(void);
void observer_can_match_disabled_when_requested(void);
void observer_on_remove_runs_when_entity_is_killed(void);
void observer_modified_emits_current_component_value(void);
void observer_modified_supports_zero_sized_tags(void);
void observer_on_set_reports_component(void);
void observer_on_add_reports_component(void);
void observer_observers_see_set_after_edge_is_cached(void);
void observer_kill_runs_on_remove_hook_of_tag(void);
void observer_on_remove_reports_component(void);
void observer_custom_event_reports_zero_component(void);
void observer_deferred_on_set_reports_component_at_flush(void);
void observer_target_exact_entity(void);
void observer_target_no_query(void);
void observer_target_filtered(void);
void observer_target_is_a_instances_use_concrete_query(void);
void observer_target_destroy(void);
void observer_target_entity_kill(void);
void observer_target_index_reuse(void);
void observer_target_on_remove(void);
void observer_target_many(void);
void observer_user_data_lifetime(void);

// Testsuite 'module'
void module_import_registers_runtime(void);
void module_name_returns_imported_name(void);
void module_enable(void);
void module_disabled_import(void);
void module_double_import_is_noop(void);
void module_dynamic_load(void);
void module_dynamic_missing(void);
void module_owns_queries_created_during_import(void);
void module_forgets_manually_destroyed_query(void);
void module_query_id_reuse_does_not_keep_old_owner(void);
void module_queries_survive_disable_enable(void);

bake_test_case public_testcases[] = {
    {
        "metadata_and_entity_introspection",
        public_metadata_and_entity_introspection
    },
    {
        "scene_save_memory_roundtrip",
        public_scene_save_memory_roundtrip
    },
    {
        "scene_invalid_load_is_non_destructive",
        public_scene_invalid_load_is_non_destructive
    },
    {
        "scene_failed_apply_rolls_back",
        public_scene_failed_apply_rolls_back
    },
    {
        "scene_reflected_codec_roundtrip",
        public_scene_reflected_codec_roundtrip
    },
    {
        "scene_unsupported_pointer_is_consistent",
        public_scene_unsupported_pointer_is_consistent
    },
    {
        "scene_binary_fixture_and_truncation",
        public_scene_binary_fixture_and_truncation
    },
    {
        "scene_unaligned_component_ids",
        public_scene_unaligned_component_ids
    }
};

bake_test_case registry_testcases[] = {
    {
        "component_names",
        registry_component_names
    },
    {
        "relation_names_and_namespaces",
        registry_relation_names_and_namespaces
    },
    {
        "growth_and_owned_names",
        registry_growth_and_owned_names
    },
    {
        "duplicate_names_keep_first",
        registry_duplicate_names_keep_first
    },
    {
        "world_restart",
        registry_world_restart
    },
    {
        "type_slots_preserve_pointers",
        registry_type_slots_preserve_pointers
    }
};

bake_test_case naming_testcases[] = {
    {
        "paths_and_scopes",
        naming_paths_and_scopes
    },
    {
        "rename_updates_descendant_paths",
        naming_rename_updates_descendant_paths
    },
    {
        "set_component_copies_and_removes",
        naming_set_component_copies_and_removes
    },
    {
        "collision_deletes_previous_subtree",
        naming_collision_deletes_previous_subtree
    },
    {
        "reparent_and_root_collision",
        naming_reparent_and_root_collision
    },
    {
        "protects_ancestors_and_cycles",
        naming_protects_ancestors_and_cycles
    },
    {
        "invalid_input_is_non_destructive",
        naming_invalid_input_is_non_destructive
    },
    {
        "anonymous_segments_and_generations",
        naming_anonymous_segments_and_generations
    },
    {
        "deferred_final_name_and_parent",
        naming_deferred_final_name_and_parent
    },
    {
        "deferred_creation_reuses_pending_paths",
        naming_deferred_creation_reuses_pending_paths
    },
    {
        "deferred_remove_kill_and_recreate",
        naming_deferred_remove_kill_and_recreate
    },
    {
        "deferred_siblings_are_all_owned_by_parent",
        naming_deferred_siblings_are_all_owned_by_parent
    },
    {
        "set_event_observes_final_identity",
        naming_set_event_observes_final_identity
    },
    {
        "collision_callbacks_can_allocate",
        naming_collision_callbacks_can_allocate
    },
    {
        "prefab_instances_have_local_child_names",
        naming_prefab_instances_have_local_child_names
    },
    {
        "scene_roundtrip_preserves_duplicate_local_names",
        naming_scene_roundtrip_preserves_duplicate_local_names
    },
    {
        "hash_growth_tombstones_and_restart",
        naming_hash_growth_tombstones_and_restart
    }
};

bake_test_case entity_testcases[] = {
    {
        "create",
        entity_create
    },
    {
        "add_components_after_world_restart",
        entity_add_components_after_world_restart
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
        "propagate_add_marks_childof_subtree",
        entity_propagate_add_marks_childof_subtree
    },
    {
        "is_a_marks_childof_subtree_abstract",
        entity_is_a_marks_childof_subtree_abstract
    },
    {
        "is_a_repeated_base_abstract_is_noop",
        entity_is_a_repeated_base_abstract_is_noop
    },
    {
        "deferred_is_a_marks_childof_subtree_abstract",
        entity_deferred_is_a_marks_childof_subtree_abstract
    },
    {
        "default_query_excludes_abstract_childof_subtree",
        entity_default_query_excludes_abstract_childof_subtree
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
        "modified_does_not_call_on_set_hook",
        component_modified_does_not_call_on_set_hook
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
    },
    {
        "resolution_keeps_tags_and_local_names",
        component_resolution_keeps_tags_and_local_names
    },
    {
        "set_hook_can_relocate_storage",
        component_set_hook_can_relocate_storage
    },
    {
        "owned_tag_copy_hooks_survive_inheritance",
        component_owned_tag_copy_hooks_survive_inheritance
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
        "zero_sized_presence_uses_storage_pointer",
        resource_zero_sized_presence_uses_storage_pointer
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
        "transfer_hook_combinations",
        resource_transfer_hook_combinations
    },
    {
        "from_system_c",
        resource_from_system_c
    },
    {
        "hooks",
        resource_hooks
    },
    {
        "fini_runs_after_component_remove",
        resource_fini_runs_after_component_remove
    },
    {
        "fini_uses_reverse_registration_order",
        resource_fini_uses_reverse_registration_order
    },
    {
        "reflection_info_and_failure",
        resource_reflection_info_and_failure
    }
};

bake_test_case world_testcases[] = {
    {
        "at_fini_runs_all_in_reverse_registration_order",
        world_at_fini_runs_all_in_reverse_registration_order
    },
    {
        "at_fini_is_cleared_on_restart",
        world_at_fini_is_cleared_on_restart
    }
};

bake_test_case rest_testcases[] = {
    {
        "is_a_and_child_of_same_target_routes",
        rest_is_a_and_child_of_same_target_routes
    },
    {
        "enum_schema_and_component_roundtrip",
        rest_enum_schema_and_component_roundtrip
    },
    {
        "in_process_body_limit",
        rest_in_process_body_limit
    },
    {
        "invalid_route_parameters",
        rest_invalid_route_parameters
    }
};

bake_test_case rest_reflection_testcases[] = {
    {
        "reflected_schema_graph_and_roles",
        rest_reflection_reflected_schema_graph_and_roles
    },
    {
        "reflected_patch_and_resources",
        rest_reflection_reflected_patch_and_resources
    }
};

bake_test_case childof_testcases[] = {
    {
        "relation_batch_views",
        childof_relation_batch_views
    },
    {
        "is_a_and_child_of_same_target_queries",
        childof_is_a_and_child_of_same_target_queries
    },
    {
        "kill_parent",
        childof_kill_parent
    },
    {
        "with_relation_adds_default_relation",
        childof_with_relation_adds_default_relation
    },
    {
        "with_relation_preserves_existing_relation",
        childof_with_relation_preserves_existing_relation
    },
    {
        "with_relation_applies_through_component_requirement",
        childof_with_relation_applies_through_component_requirement
    },
    {
        "with_relation_deferred_add",
        childof_with_relation_deferred_add
    },
    {
        "with_relation_deferred_explicit_relation_wins",
        childof_with_relation_deferred_explicit_relation_wins
    },
    {
        "with_relation_double_add_does_not_restore_removed_relation",
        childof_with_relation_double_add_does_not_restore_removed_relation
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
        "dense_replacement_during_flush_keeps_reverse_source",
        childof_dense_replacement_during_flush_keeps_reverse_source
    },
    {
        "dense_unrelate_keeps_source_indices",
        childof_dense_unrelate_keeps_source_indices
    },
    {
        "dense_unrelate_updates_swapped_reverse_index",
        childof_dense_unrelate_updates_swapped_reverse_index
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
        "generic_isa_has_relation",
        childof_generic_isa_has_relation
    },
    {
        "generic_isa_target",
        childof_generic_isa_target
    },
    {
        "generic_isa_retarget",
        childof_generic_isa_retarget
    },
    {
        "generic_isa_unrelate",
        childof_generic_isa_unrelate
    },
    {
        "generic_isa_cycle",
        childof_generic_isa_cycle
    },
    {
        "acyclic_dense_cycle_aborts",
        childof_acyclic_dense_cycle_aborts
    },
    {
        "acyclic_by_target_cycle_aborts",
        childof_acyclic_by_target_cycle_aborts
    },
    {
        "generic_isa_deferred",
        childof_generic_isa_deferred
    },
    {
        "isa_clones_child_tree",
        childof_isa_clones_child_tree
    },
    {
        "isa_clone_snapshot_and_same_target_noop",
        childof_isa_clone_snapshot_and_same_target_noop
    },
    {
        "isa_clones_child_tree_deferred",
        childof_isa_clones_child_tree_deferred
    },
    {
        "generic_isa_relation_event",
        childof_generic_isa_relation_event
    },
    {
        "relation_observer_events",
        childof_relation_observer_events
    },
    {
        "relation_observer_reports_relation_without_component",
        childof_relation_observer_reports_relation_without_component
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
    },
    {
        "new_child_of_abstract_parent_is_abstract",
        childof_new_child_of_abstract_parent_is_abstract
    },
    {
        "new_child_of_normal_parent_is_not_abstract",
        childof_new_child_of_normal_parent_is_not_abstract
    },
    {
        "isa_instantiated_children_are_not_abstract",
        childof_isa_instantiated_children_are_not_abstract
    },
    {
        "propagate_add_rejects_cyclic_relation_in_debug",
        childof_propagate_add_rejects_cyclic_relation_in_debug
    },
    {
        "propagate_add_rejects_by_target_relation",
        childof_propagate_add_rejects_by_target_relation
    }
};

bake_test_case query_testcases[] = {
    {
        "tags_keep_presence_without_data",
        query_tags_keep_presence_without_data
    },
    {
        "full_descriptor_and_empty_candidates",
        query_full_descriptor_and_empty_candidates
    },
    {
        "sorted_growth_preserves_fields_and_equal_order",
        query_sorted_growth_preserves_fields_and_equal_order
    },
    {
        "terms_field_order",
        query_terms_field_order
    },
    {
        "resources_do_not_affect_matching_or_fields",
        query_resources_do_not_affect_matching_or_fields
    },
    {
        "resource_only_matches_all_tables",
        query_resource_only_matches_all_tables
    },
    {
        "count_matches_current_query_entities",
        query_count_matches_current_query_entities
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
    },
    {
        "inherited_fields_refresh_after_base_table_growth",
        query_inherited_fields_refresh_after_base_table_growth
    }
};

bake_test_case system_testcases[] = {
    {
        "run",
        system_run
    },
    {
        "parallel_independent_callbacks",
        system_parallel_independent_callbacks
    },
    {
        "parallel_worker_context_is_deferred",
        system_parallel_worker_context_is_deferred
    },
    {
        "parallel_query_table_conflicts",
        system_parallel_query_table_conflicts
    },
    {
        "indexed_queries_detect_late_overlap",
        system_indexed_queries_detect_late_overlap
    },
    {
        "batches_invalidate_after_table_creation",
        system_batches_invalidate_after_table_creation
    },
    {
        "main_thread_only",
        system_main_thread_only
    },
    {
        "parallel_after_is_a_barrier",
        system_parallel_after_is_a_barrier
    },
    {
        "resource_access_conflicts",
        system_resource_access_conflicts
    },
    {
        "parallel_structural_changes_flush_at_barrier",
        system_parallel_structural_changes_flush_at_barrier
    },
    {
        "manual_run_is_synchronous_with_workers",
        system_manual_run_is_synchronous_with_workers
    },
    {
        "worker_auto_and_reinit",
        system_worker_auto_and_reinit
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
    },
    {
        "interval_throttles_progress",
        system_interval_throttles_progress
    },
    {
        "interval_manual_execution_is_forced",
        system_interval_manual_execution_is_forced
    },
    {
        "interval_resets_on_enable_transition",
        system_interval_resets_on_enable_transition
    },
    {
        "interval_does_not_gate_start_phase",
        system_interval_does_not_gate_start_phase
    },
    {
        "custom_phase",
        system_custom_phase
    },
    {
        "default_phase_is_on_update",
        system_default_phase_is_on_update
    },
    {
        "custom_phase_after_only",
        system_custom_phase_after_only
    },
    {
        "custom_phase_unconstrained",
        system_custom_phase_unconstrained
    },
    {
        "parallel_portions_cover_rows",
        system_parallel_portions_cover_rows
    },
    {
        "parallel_portions_empty_and_manual",
        system_parallel_portions_empty_and_manual
    },
    {
        "dependency_releases_without_group_barrier",
        system_dependency_releases_without_group_barrier
    },
    {
        "parallel_creation_grows_stable_index",
        system_parallel_creation_grows_stable_index
    },
    {
        "parallel_commands_have_logical_order",
        system_parallel_commands_have_logical_order
    },
    {
        "publication_rechecks_same_phase_conflicts",
        system_publication_rechecks_same_phase_conflicts
    },
    {
        "parallel_shared_and_up_fields",
        system_parallel_shared_and_up_fields
    },
    {
        "parallel_relation_views_follow_portions",
        system_parallel_relation_views_follow_portions
    },
    {
        "graph_keeps_all_after_dependencies",
        system_graph_keeps_all_after_dependencies
    },
    {
        "parallel_rejects_tasks",
        system_parallel_rejects_tasks
    },
    {
        "parallel_rejects_resource_writes",
        system_parallel_rejects_resource_writes
    },
    {
        "parallel_rejects_main_thread",
        system_parallel_rejects_main_thread
    },
    {
        "parallel_rejects_no_defer",
        system_parallel_rejects_no_defer
    },
    {
        "parallel_rejects_ordered_queries",
        system_parallel_rejects_ordered_queries
    },
    {
        "parallel_rejects_query_destruction",
        system_parallel_rejects_query_destruction
    },
    {
        "shared_reads_conflict_across_tables",
        system_shared_reads_conflict_across_tables
    },
    {
        "serial_commands_keep_registration_order",
        system_serial_commands_keep_registration_order
    },
    {
        "empty_portion_releases_main_thread_successor",
        system_empty_portion_releases_main_thread_successor
    },
    {
        "parallel_creation_crosses_page_boundary",
        system_parallel_creation_crosses_page_boundary
    },
    {
        "parallel_returns_unused_recycled_ids",
        system_parallel_returns_unused_recycled_ids
    },
    {
        "parallel_returns_unused_fresh_ids",
        system_parallel_returns_unused_fresh_ids
    },
    {
        "parallel_reservations_suspend_for_exclusive_callbacks",
        system_parallel_reservations_suspend_for_exclusive_callbacks
    },
    {
        "parallel_batched_commands_keep_logical_order",
        system_parallel_batched_commands_keep_logical_order
    }
};

bake_test_case observer_testcases[] = {
    {
        "global_registration_does_not_duplicate_queries",
        observer_global_registration_does_not_duplicate_queries
    },
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
        "modified_emits_current_component_value",
        observer_modified_emits_current_component_value
    },
    {
        "modified_supports_zero_sized_tags",
        observer_modified_supports_zero_sized_tags
    },
    {
        "on_set_reports_component",
        observer_on_set_reports_component
    },
    {
        "on_add_reports_component",
        observer_on_add_reports_component
    },
    {
        "observers_see_set_after_edge_is_cached",
        observer_observers_see_set_after_edge_is_cached
    },
    {
        "kill_runs_on_remove_hook_of_tag",
        observer_kill_runs_on_remove_hook_of_tag
    },
    {
        "on_remove_reports_component",
        observer_on_remove_reports_component
    },
    {
        "custom_event_reports_zero_component",
        observer_custom_event_reports_zero_component
    },
    {
        "deferred_on_set_reports_component_at_flush",
        observer_deferred_on_set_reports_component_at_flush
    },
    {
        "target_exact_entity",
        observer_target_exact_entity
    },
    {
        "target_no_query",
        observer_target_no_query
    },
    {
        "target_filtered",
        observer_target_filtered
    },
    {
        "target_is_a_instances_use_concrete_query",
        observer_target_is_a_instances_use_concrete_query
    },
    {
        "target_destroy",
        observer_target_destroy
    },
    {
        "target_entity_kill",
        observer_target_entity_kill
    },
    {
        "target_index_reuse",
        observer_target_index_reuse
    },
    {
        "target_on_remove",
        observer_target_on_remove
    },
    {
        "target_many",
        observer_target_many
    },
    {
        "user_data_lifetime",
        observer_user_data_lifetime
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
    },
    {
        "dynamic_load",
        module_dynamic_load
    },
    {
        "dynamic_missing",
        module_dynamic_missing
    },
    {
        "owns_queries_created_during_import",
        module_owns_queries_created_during_import
    },
    {
        "forgets_manually_destroyed_query",
        module_forgets_manually_destroyed_query
    },
    {
        "query_id_reuse_does_not_keep_old_owner",
        module_query_id_reuse_does_not_keep_old_owner
    },
    {
        "queries_survive_disable_enable",
        module_queries_survive_disable_enable
    }
};


static bake_test_suite suites[] = {
    {
        "public",
        NULL,
        NULL,
        8,
        public_testcases
    },
    {
        "registry",
        NULL,
        NULL,
        6,
        registry_testcases
    },
    {
        "naming",
        NULL,
        NULL,
        17,
        naming_testcases
    },
    {
        "entity",
        NULL,
        NULL,
        21,
        entity_testcases
    },
    {
        "component",
        NULL,
        NULL,
        30,
        component_testcases
    },
    {
        "resource",
        NULL,
        NULL,
        14,
        resource_testcases
    },
    {
        "world",
        NULL,
        NULL,
        2,
        world_testcases
    },
    {
        "rest",
        NULL,
        NULL,
        4,
        rest_testcases
    },
    {
        "rest_reflection",
        NULL,
        NULL,
        2,
        rest_reflection_testcases
    },
    {
        "childof",
        NULL,
        NULL,
        51,
        childof_testcases
    },
    {
        "query",
        NULL,
        NULL,
        27,
        query_testcases
    },
    {
        "system",
        NULL,
        NULL,
        61,
        system_testcases
    },
    {
        "observer",
        NULL,
        NULL,
        24,
        observer_testcases
    },
    {
        "module",
        NULL,
        NULL,
        11,
        module_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.test", argc, argv, suites, 14);
}
