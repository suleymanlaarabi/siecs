
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


static bake_test_suite suites[] = {
    {
        "entity",
        NULL,
        NULL,
        2,
        entity_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.test", argc, argv, suites, 1);
}
