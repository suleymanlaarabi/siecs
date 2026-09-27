
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'ecs'
void ecs_components(void);
void ecs_child_of(void);
void ecs_text_component(void);
void ecs_component_updates(void);

bake_test_case ecs_testcases[] = {
    {
        "components",
        ecs_components
    },
    {
        "child_of",
        ecs_child_of
    },
    {
        "text_component",
        ecs_text_component
    },
    {
        "component_updates",
        ecs_component_updates
    }
};


static bake_test_suite suites[] = {
    {
        "ecs",
        NULL,
        NULL,
        4,
        ecs_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siui.cpp.test", argc, argv, suites, 1);
}
