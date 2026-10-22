
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'render'
void render_primitives_drawn(void);
void render_camera_order_split_screen(void);
void render_msaa_resolves(void);
void render_bloom_glows(void);
void render_shadows_darken(void);
void render_shadow_cascades_cached(void);
void render_graph_regenerated_on_settings(void);
void render_still_scene_invariants(void);
void render_mesh_with_material(void);

// Testsuite 'picking'
void picking_ray_hits_nearest(void);
void picking_click_events(void);
void picking_dpi_independent(void);

bake_test_case render_testcases[] = {
    {
        "primitives_drawn",
        render_primitives_drawn
    },
    {
        "camera_order_split_screen",
        render_camera_order_split_screen
    },
    {
        "msaa_resolves",
        render_msaa_resolves
    },
    {
        "bloom_glows",
        render_bloom_glows
    },
    {
        "shadows_darken",
        render_shadows_darken
    },
    {
        "shadow_cascades_cached",
        render_shadow_cascades_cached
    },
    {
        "graph_regenerated_on_settings",
        render_graph_regenerated_on_settings
    },
    {
        "still_scene_invariants",
        render_still_scene_invariants
    },
    {
        "mesh_with_material",
        render_mesh_with_material
    }
};

bake_test_case picking_testcases[] = {
    {
        "ray_hits_nearest",
        picking_ray_hits_nearest
    },
    {
        "click_events",
        picking_click_events
    },
    {
        "dpi_independent",
        picking_dpi_independent
    }
};


static bake_test_suite suites[] = {
    {
        "render",
        NULL,
        NULL,
        9,
        render_testcases
    },
    {
        "picking",
        NULL,
        NULL,
        3,
        picking_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("sirender3d.test", argc, argv, suites, 2);
}
