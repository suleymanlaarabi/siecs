
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'module_import'
void module_import_import_without_props(void);
void module_import_import_with_props(void);
void module_import_double_import(void);

// Testsuite 'module_state'
void module_state_lookup_empty(void);
void module_state_enable(void);
void module_state_reimport_after_world_fini(void);

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
    return bake_test_run("siecs_cpp.test", argc, argv, suites, 2);
}
