
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'feature'
void feature_extract_draws(void);
void feature_unchanged_not_extracted(void);

bake_test_case feature_testcases[] = {
    {
        "extract_draws",
        feature_extract_draws
    },
    {
        "unchanged_not_extracted",
        feature_unchanged_not_extracted
    }
};


static bake_test_suite suites[] = {
    {
        "feature",
        NULL,
        NULL,
        2,
        feature_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("sigpu.cpp.test", argc, argv, suites, 1);
}
