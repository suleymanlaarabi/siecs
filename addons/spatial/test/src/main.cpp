
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'spatial'
void spatial_component_requirements_and_defaults(void);
void spatial_position_hierarchy(void);
void spatial_rotation_hierarchy(void);
void spatial_scale_hierarchy(void);
void spatial_static_is_not_updated(void);
void spatial_cpp_api(void);

bake_test_case spatial_testcases[] = {
    {
        "component_requirements_and_defaults",
        spatial_component_requirements_and_defaults
    },
    {
        "position_hierarchy",
        spatial_position_hierarchy
    },
    {
        "rotation_hierarchy",
        spatial_rotation_hierarchy
    },
    {
        "scale_hierarchy",
        spatial_scale_hierarchy
    },
    {
        "static_is_not_updated",
        spatial_static_is_not_updated
    },
    {
        "cpp_api",
        spatial_cpp_api
    }
};


static bake_test_suite suites[] = {
    {
        "spatial",
        NULL,
        NULL,
        6,
        spatial_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs_spatial.test", argc, argv, suites, 1);
}
