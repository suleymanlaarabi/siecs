
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
void resource_system_read(void);
void resource_system_write(void);
void resource_system_without_query_runs_once(void);
void resource_system_empty_callback_runs_once(void);
void resource_query_read(void);
void resource_does_not_create_query_term(void);
void resource_field_index_stays_correct(void);

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
        8,
        resource_testcases
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
    return bake_test_run("siecs_cpp.test", argc, argv, suites, 4);
}
