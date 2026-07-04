
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'entity_state'
void entity_state_enable_disable(void);
void entity_state_disabled_entities_are_skipped(void);

// Testsuite 'resource'
void resource_world_api(void);
void resource_presence_checks_do_not_register(void);
void resource_system_read(void);
void resource_system_write(void);
void resource_system_without_query_runs_once(void);
void resource_system_empty_callback_runs_once(void);
void resource_query_read(void);
void resource_does_not_create_query_term(void);
void resource_field_index_stays_correct(void);
void resource_cpp_raii_component_survives_table_migrations(void);
void resource_capturing_system_keeps_state(void);

// Testsuite 'query'
void query_reads_shared_inherited_field(void);
void query_mutable_does_not_match_shared_inherited_field(void);
void query_reads_shared_and_writes_owned_field(void);
void query_owned_override_wins_over_shared_field(void);

// Testsuite 'observer'
void observer_custom_event(void);
void observer_const_arg(void);
void observer_multi_arg_terms(void);
void observer_does_not_match_missing_component(void);
void observer_resource_read(void);
void observer_resource_write(void);
void observer_resource_does_not_create_query_term(void);
void observer_resource_field_index_stays_correct(void);

// Testsuite 'module_import'
void module_import_import_without_props(void);
void module_import_import_with_props(void);
void module_import_double_import(void);

// Testsuite 'module_state'
void module_state_lookup_empty(void);
void module_state_enable(void);
void module_state_reimport_after_world_fini(void);

bake_test_case entity_state_testcases[] = {
    {
        "enable_disable",
        entity_state_enable_disable
    },
    {
        "disabled_entities_are_skipped",
        entity_state_disabled_entities_are_skipped
    }
};

bake_test_case resource_testcases[] = {
    {
        "world_api",
        resource_world_api
    },
    {
        "presence_checks_do_not_register",
        resource_presence_checks_do_not_register
    },
    {
        "system_read",
        resource_system_read
    },
    {
        "system_write",
        resource_system_write
    },
    {
        "system_without_query_runs_once",
        resource_system_without_query_runs_once
    },
    {
        "system_empty_callback_runs_once",
        resource_system_empty_callback_runs_once
    },
    {
        "query_read",
        resource_query_read
    },
    {
        "does_not_create_query_term",
        resource_does_not_create_query_term
    },
    {
        "field_index_stays_correct",
        resource_field_index_stays_correct
    },
    {
        "cpp_raii_component_survives_table_migrations",
        resource_cpp_raii_component_survives_table_migrations
    },
    {
        "capturing_system_keeps_state",
        resource_capturing_system_keeps_state
    }
};

bake_test_case query_testcases[] = {
    {
        "reads_shared_inherited_field",
        query_reads_shared_inherited_field
    },
    {
        "mutable_does_not_match_shared_inherited_field",
        query_mutable_does_not_match_shared_inherited_field
    },
    {
        "reads_shared_and_writes_owned_field",
        query_reads_shared_and_writes_owned_field
    },
    {
        "owned_override_wins_over_shared_field",
        query_owned_override_wins_over_shared_field
    }
};

bake_test_case observer_testcases[] = {
    {
        "custom_event",
        observer_custom_event
    },
    {
        "const_arg",
        observer_const_arg
    },
    {
        "multi_arg_terms",
        observer_multi_arg_terms
    },
    {
        "does_not_match_missing_component",
        observer_does_not_match_missing_component
    },
    {
        "resource_read",
        observer_resource_read
    },
    {
        "resource_write",
        observer_resource_write
    },
    {
        "resource_does_not_create_query_term",
        observer_resource_does_not_create_query_term
    },
    {
        "resource_field_index_stays_correct",
        observer_resource_field_index_stays_correct
    }
};

bake_test_case module_import_testcases[] = {
    {
        "import_without_props",
        module_import_import_without_props
    },
    {
        "import_with_props",
        module_import_import_with_props
    },
    {
        "double_import",
        module_import_double_import
    }
};

bake_test_case module_state_testcases[] = {
    {
        "lookup_empty",
        module_state_lookup_empty
    },
    {
        "enable",
        module_state_enable
    },
    {
        "reimport_after_world_fini",
        module_state_reimport_after_world_fini
    }
};


static bake_test_suite suites[] = {
    {
        "entity_state",
        NULL,
        NULL,
        2,
        entity_state_testcases
    },
    {
        "resource",
        NULL,
        NULL,
        11,
        resource_testcases
    },
    {
        "query",
        NULL,
        NULL,
        4,
        query_testcases
    },
    {
        "observer",
        NULL,
        NULL,
        8,
        observer_testcases
    },
    {
        "module_import",
        NULL,
        NULL,
        3,
        module_import_testcases
    },
    {
        "module_state",
        NULL,
        NULL,
        3,
        module_state_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.cpp.test", argc, argv, suites, 6);
}
