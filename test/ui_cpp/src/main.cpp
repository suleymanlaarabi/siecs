
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'builder'
void builder_card(void);
void builder_child_of(void);
void builder_string_view_copy(void);
void builder_single_mutations(void);

bake_test_case builder_testcases[] = {
    {
        "card",
        builder_card
    },
    {
        "child_of",
        builder_child_of
    },
    {
        "string_view_copy",
        builder_string_view_copy
    },
    {
        "single_mutations",
        builder_single_mutations
    }
};


static bake_test_suite suites[] = {
    {
        "builder",
        NULL,
        NULL,
        4,
        builder_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siui.cpp.test", argc, argv, suites, 1);
}
