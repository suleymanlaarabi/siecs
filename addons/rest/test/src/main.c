
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'rest'
void rest_listener_failure_reports_details(void);
void rest_module_lifecycle(void);
void rest_schema_uses_public_metadata(void);
void rest_entity_routes_use_public_introspection(void);
void rest_component_mutation_uses_public_metadata(void);

bake_test_case rest_testcases[] = {
    {
        "listener_failure_reports_details",
        rest_listener_failure_reports_details
    },
    {
        "module_lifecycle",
        rest_module_lifecycle
    },
    {
        "schema_uses_public_metadata",
        rest_schema_uses_public_metadata
    },
    {
        "entity_routes_use_public_introspection",
        rest_entity_routes_use_public_introspection
    },
    {
        "component_mutation_uses_public_metadata",
        rest_component_mutation_uses_public_metadata
    }
};


static bake_test_suite suites[] = {
    {
        "rest",
        NULL,
        NULL,
        5,
        rest_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs_rest.test", argc, argv, suites, 1);
}
