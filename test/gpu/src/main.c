
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'state'
void state_descriptor_adds_state(void);
void state_ready_after_frame(void);
void state_isa_instance_starts_pending(void);
void state_isa_objects_released_once(void);
void state_migration_keeps_gpu_objects(void);
void state_observer_sees_transitions(void);

// Testsuite 'lifecycle'
void lifecycle_created_out_of_order(void);
void lifecycle_sets_coalesce(void);
void lifecycle_waiting_is_not_retried(void);
void lifecycle_failure_then_fix(void);
void lifecycle_invalid_then_corrected(void);
void lifecycle_removed_before_ready(void);
void lifecycle_removed_after_ready(void);
void lifecycle_resize_replaces_object(void);
void lifecycle_size_from_follows_source(void);
void lifecycle_size_from_cycle_fails(void);
void lifecycle_dependency_killed_fails(void);
void lifecycle_entry_missing_fails(void);
void lifecycle_budget_defers(void);
void lifecycle_no_device_stays_pending(void);
void lifecycle_second_device_waits(void);
void lifecycle_state_before_device_opens(void);
void lifecycle_device_recreated(void);

// Testsuite 'upload'
void upload_buffer_upload_lands(void);
void upload_upload_waits_for_ready(void);
void upload_upload_out_of_range_rejected(void);
void upload_texture_upload_pitch(void);
void upload_upload_dropped_with_buffer(void);

// Testsuite 'frame'
void frame_upload_every_frame(void);
void frame_uploads_share_one_submission(void);

// Testsuite 'pack'
void pack_truncated_pack_fails(void);
void pack_fingerprint_mismatch_fails(void);

// Testsuite 'graph'
void graph_order_deduced(void);
void graph_clear_load_store(void);
void graph_uninitialized_read_rejected(void);
void graph_sampled_attachment_rejected(void);
void graph_unused_pass_culled(void);
void graph_compiles_once(void);
void graph_resize_without_compile(void);
void graph_waits_for_resource(void);
void graph_compute_pass(void);
void graph_copy_pass(void);
void graph_callback_destroyed_once(void);

// Testsuite 'window'
void window_surface_waits_for_window(void);
void window_surface_presents(void);
void window_resize_reallocates(void);
void window_minimized_suspends(void);
void window_closed_waits_again(void);
void window_two_windows(void);
void window_split_screen(void);

// Testsuite 'feature'
void feature_draws_instances(void);
void feature_unchanged_table_not_extracted(void);
void feature_change_reextracts(void);
void feature_swap_remove_reextracts(void);
void feature_range_by_table(void);
void feature_empty_range_released(void);
void feature_parallel_portions(void);
void feature_material_retarget(void);

// Testsuite 'culling'
void culling_all_or_none_visible(void);
void culling_partition_on_change_only(void);
void culling_lod_without_mutation(void);
void culling_merge_materials_same_textures(void);
void culling_layered_order(void);

// Testsuite 'material'
void material_isa_override(void);
void material_dead_texture_waits(void);
void material_packing(void);
void material_mismatch_fails(void);

// Testsuite 'invariant'
void invariant_still_scene(void);
void invariant_moving_view(void);

bake_test_case state_testcases[] = {
    {
        "descriptor_adds_state",
        state_descriptor_adds_state
    },
    {
        "ready_after_frame",
        state_ready_after_frame
    },
    {
        "isa_instance_starts_pending",
        state_isa_instance_starts_pending
    },
    {
        "isa_objects_released_once",
        state_isa_objects_released_once
    },
    {
        "migration_keeps_gpu_objects",
        state_migration_keeps_gpu_objects
    },
    {
        "observer_sees_transitions",
        state_observer_sees_transitions
    }
};

bake_test_case lifecycle_testcases[] = {
    {
        "created_out_of_order",
        lifecycle_created_out_of_order
    },
    {
        "sets_coalesce",
        lifecycle_sets_coalesce
    },
    {
        "waiting_is_not_retried",
        lifecycle_waiting_is_not_retried
    },
    {
        "failure_then_fix",
        lifecycle_failure_then_fix
    },
    {
        "invalid_then_corrected",
        lifecycle_invalid_then_corrected
    },
    {
        "removed_before_ready",
        lifecycle_removed_before_ready
    },
    {
        "removed_after_ready",
        lifecycle_removed_after_ready
    },
    {
        "resize_replaces_object",
        lifecycle_resize_replaces_object
    },
    {
        "size_from_follows_source",
        lifecycle_size_from_follows_source
    },
    {
        "size_from_cycle_fails",
        lifecycle_size_from_cycle_fails
    },
    {
        "dependency_killed_fails",
        lifecycle_dependency_killed_fails
    },
    {
        "entry_missing_fails",
        lifecycle_entry_missing_fails
    },
    {
        "budget_defers",
        lifecycle_budget_defers
    },
    {
        "no_device_stays_pending",
        lifecycle_no_device_stays_pending
    },
    {
        "second_device_waits",
        lifecycle_second_device_waits
    },
    {
        "state_before_device_opens",
        lifecycle_state_before_device_opens
    },
    {
        "device_recreated",
        lifecycle_device_recreated
    }
};

bake_test_case upload_testcases[] = {
    {
        "buffer_upload_lands",
        upload_buffer_upload_lands
    },
    {
        "upload_waits_for_ready",
        upload_upload_waits_for_ready
    },
    {
        "upload_out_of_range_rejected",
        upload_upload_out_of_range_rejected
    },
    {
        "texture_upload_pitch",
        upload_texture_upload_pitch
    },
    {
        "upload_dropped_with_buffer",
        upload_upload_dropped_with_buffer
    }
};

bake_test_case frame_testcases[] = {
    {
        "upload_every_frame",
        frame_upload_every_frame
    },
    {
        "uploads_share_one_submission",
        frame_uploads_share_one_submission
    }
};

bake_test_case pack_testcases[] = {
    {
        "truncated_pack_fails",
        pack_truncated_pack_fails
    },
    {
        "fingerprint_mismatch_fails",
        pack_fingerprint_mismatch_fails
    }
};

bake_test_case graph_testcases[] = {
    {
        "order_deduced",
        graph_order_deduced
    },
    {
        "clear_load_store",
        graph_clear_load_store
    },
    {
        "uninitialized_read_rejected",
        graph_uninitialized_read_rejected
    },
    {
        "sampled_attachment_rejected",
        graph_sampled_attachment_rejected
    },
    {
        "unused_pass_culled",
        graph_unused_pass_culled
    },
    {
        "compiles_once",
        graph_compiles_once
    },
    {
        "resize_without_compile",
        graph_resize_without_compile
    },
    {
        "waits_for_resource",
        graph_waits_for_resource
    },
    {
        "compute_pass",
        graph_compute_pass
    },
    {
        "copy_pass",
        graph_copy_pass
    },
    {
        "callback_destroyed_once",
        graph_callback_destroyed_once
    }
};

bake_test_case window_testcases[] = {
    {
        "surface_waits_for_window",
        window_surface_waits_for_window
    },
    {
        "surface_presents",
        window_surface_presents
    },
    {
        "resize_reallocates",
        window_resize_reallocates
    },
    {
        "minimized_suspends",
        window_minimized_suspends
    },
    {
        "closed_waits_again",
        window_closed_waits_again
    },
    {
        "two_windows",
        window_two_windows
    },
    {
        "split_screen",
        window_split_screen
    }
};

bake_test_case feature_testcases[] = {
    {
        "draws_instances",
        feature_draws_instances
    },
    {
        "unchanged_table_not_extracted",
        feature_unchanged_table_not_extracted
    },
    {
        "change_reextracts",
        feature_change_reextracts
    },
    {
        "swap_remove_reextracts",
        feature_swap_remove_reextracts
    },
    {
        "range_by_table",
        feature_range_by_table
    },
    {
        "empty_range_released",
        feature_empty_range_released
    },
    {
        "parallel_portions",
        feature_parallel_portions
    },
    {
        "material_retarget",
        feature_material_retarget
    }
};

bake_test_case culling_testcases[] = {
    {
        "all_or_none_visible",
        culling_all_or_none_visible
    },
    {
        "partition_on_change_only",
        culling_partition_on_change_only
    },
    {
        "lod_without_mutation",
        culling_lod_without_mutation
    },
    {
        "merge_materials_same_textures",
        culling_merge_materials_same_textures
    },
    {
        "layered_order",
        culling_layered_order
    }
};

bake_test_case material_testcases[] = {
    {
        "isa_override",
        material_isa_override
    },
    {
        "dead_texture_waits",
        material_dead_texture_waits
    },
    {
        "packing",
        material_packing
    },
    {
        "mismatch_fails",
        material_mismatch_fails
    }
};

bake_test_case invariant_testcases[] = {
    {
        "still_scene",
        invariant_still_scene
    },
    {
        "moving_view",
        invariant_moving_view
    }
};


static bake_test_suite suites[] = {
    {
        "state",
        NULL,
        NULL,
        6,
        state_testcases
    },
    {
        "lifecycle",
        NULL,
        NULL,
        17,
        lifecycle_testcases
    },
    {
        "upload",
        NULL,
        NULL,
        5,
        upload_testcases
    },
    {
        "frame",
        NULL,
        NULL,
        2,
        frame_testcases
    },
    {
        "pack",
        NULL,
        NULL,
        2,
        pack_testcases
    },
    {
        "graph",
        NULL,
        NULL,
        11,
        graph_testcases
    },
    {
        "window",
        NULL,
        NULL,
        7,
        window_testcases
    },
    {
        "feature",
        NULL,
        NULL,
        8,
        feature_testcases
    },
    {
        "culling",
        NULL,
        NULL,
        5,
        culling_testcases
    },
    {
        "material",
        NULL,
        NULL,
        4,
        material_testcases
    },
    {
        "invariant",
        NULL,
        NULL,
        2,
        invariant_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("sigpu.test", argc, argv, suites, 11);
}
