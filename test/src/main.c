
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

// Testsuite 'component'
void component_reflection(void);
void component_on_add(void);

// Testsuite 'multi_world'
void multi_world_same_component_registered_in_two_worlds(void);
void multi_world_worlds_keep_independent_component_storage(void);
void multi_world_queries_only_see_their_world_tables(void);
void multi_world_fini_one_world_keeps_other_world_valid(void);
void multi_world_relations_remain_world_local(void);

// Testsuite 'resource'
void resource_set_get(void);
void resource_try_get_missing(void);
void resource_remove(void);
void resource_replace(void);
void resource_from_system_c(void);
void resource_hooks(void);
void resource_does_not_consume_component_ids(void);

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
void query_optional_field_present(void);
void query_optional_field_missing_keeps_field_order(void);
void query_inout_optional_mutates_when_present(void);

// Testsuite 'system'
void system_run(void);
void system_phase_order(void);
void system_start_phases_run_once(void);
void system_after_order(void);
void system_enable(void);
void system_without_query_runs_once(void);
void system_callback_can_advance_iterator(void);
void system_skips_disabled_by_default(void);
void system_can_run_on_disabled_when_requested(void);

// Testsuite 'observer'
void observer_enable(void);
void observer_skips_disabled_by_default(void);
void observer_can_match_disabled_when_requested(void);

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

bake_test_case entity_testcases[] = {
    {
        "create",
        entity_create
    },
    {
        "with",
        entity_with
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
        "from_system_c",
        resource_from_system_c
    },
    {
        "hooks",
        resource_hooks
    },
    {
        "does_not_consume_component_ids",
        resource_does_not_consume_component_ids
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


static bake_test_suite suites[] = {
    {
        "entity",
        NULL,
        NULL,
        2,
        entity_testcases
    },
    {
        "component",
        NULL,
        NULL,
        2,
        component_testcases
    },
    {
        "multi_world",
        NULL,
        NULL,
        5,
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
        8,
        query_testcases
    },
    {
        "system",
        NULL,
        NULL,
        9,
        system_testcases
    },
    {
        "observer",
        NULL,
        NULL,
        3,
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
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.test", argc, argv, suites, 11);
}
