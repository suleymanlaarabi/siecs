
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

// Testsuite 'system'
void system_run(void);
void system_phase_order(void);
void system_after_order(void);
void system_enable(void);

// Testsuite 'string'
void string_init(void);
void string_append(void);
void string_trim(void);
void string_starts_ends_with(void);

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
        "after_order",
        system_after_order
    },
    {
        "enable",
        system_enable
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


static bake_test_suite suites[] = {
    {
        "entity",
        NULL,
        NULL,
        2,
        entity_testcases
    },
    {
        "system",
        NULL,
        NULL,
        4,
        system_testcases
    },
    {
        "string",
        NULL,
        NULL,
        4,
        string_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.test", argc, argv, suites, 3);
}
