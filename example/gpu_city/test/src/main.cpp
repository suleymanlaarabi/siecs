
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'City'
void City_small_grid(void);
void City_full_grid(void);

bake_test_case City_testcases[] = {
    {
        "small_grid",
        City_small_grid
    },
    {
        "full_grid",
        City_full_grid
    }
};


static bake_test_suite suites[] = {
    {
        "City",
        NULL,
        NULL,
        2,
        City_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.example.gpu_city.test", argc, argv, suites, 1);
}
