
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'layout'
void layout_auto_empty(void);
void layout_auto_text(void);
void layout_text_grows(void);
void layout_max_width_wrap(void);
void layout_wrap_height(void);
void layout_min_max(void);
void layout_padding(void);
void layout_four_borders(void);
void layout_parent_child_auto(void);
void layout_row_column(void);
void layout_grow_shrink(void);
void layout_wrap(void);
void layout_justify_align_stretch(void);
void layout_percentages(void);
void layout_absolute(void);
void layout_nested_auto(void);
void layout_overflow_hidden(void);
void layout_childof_runtime(void);
void layout_text_runtime(void);
void layout_resize(void);
void layout_utf8(void);
void layout_empty_text(void);
void layout_order(void);
void layout_order_runtime(void);
void layout_deep_tree(void);
void layout_many_siblings(void);
void layout_static_clean(void);
void layout_paint_only(void);
void layout_reparent(void);
void layout_font_size_runtime(void);
void layout_auto_parent_percent(void);
void layout_aspect_ratio(void);
void layout_align_content(void);
void layout_space_between(void);
void layout_margin(void);
void layout_line_height(void);
void layout_text_cleared(void);
void layout_overflow_runtime(void);
void layout_node_after_table_move(void);
void layout_text_before_node(void);

bake_test_case layout_testcases[] = {
    {
        "auto_empty",
        layout_auto_empty
    },
    {
        "auto_text",
        layout_auto_text
    },
    {
        "text_grows",
        layout_text_grows
    },
    {
        "max_width_wrap",
        layout_max_width_wrap
    },
    {
        "wrap_height",
        layout_wrap_height
    },
    {
        "min_max",
        layout_min_max
    },
    {
        "padding",
        layout_padding
    },
    {
        "four_borders",
        layout_four_borders
    },
    {
        "parent_child_auto",
        layout_parent_child_auto
    },
    {
        "row_column",
        layout_row_column
    },
    {
        "grow_shrink",
        layout_grow_shrink
    },
    {
        "wrap",
        layout_wrap
    },
    {
        "justify_align_stretch",
        layout_justify_align_stretch
    },
    {
        "percentages",
        layout_percentages
    },
    {
        "absolute",
        layout_absolute
    },
    {
        "nested_auto",
        layout_nested_auto
    },
    {
        "overflow_hidden",
        layout_overflow_hidden
    },
    {
        "childof_runtime",
        layout_childof_runtime
    },
    {
        "text_runtime",
        layout_text_runtime
    },
    {
        "resize",
        layout_resize
    },
    {
        "utf8",
        layout_utf8
    },
    {
        "empty_text",
        layout_empty_text
    },
    {
        "order",
        layout_order
    },
    {
        "order_runtime",
        layout_order_runtime
    },
    {
        "deep_tree",
        layout_deep_tree
    },
    {
        "many_siblings",
        layout_many_siblings
    },
    {
        "static_clean",
        layout_static_clean
    },
    {
        "paint_only",
        layout_paint_only
    },
    {
        "reparent",
        layout_reparent
    },
    {
        "font_size_runtime",
        layout_font_size_runtime
    },
    {
        "auto_parent_percent",
        layout_auto_parent_percent
    },
    {
        "aspect_ratio",
        layout_aspect_ratio
    },
    {
        "align_content",
        layout_align_content
    },
    {
        "space_between",
        layout_space_between
    },
    {
        "margin",
        layout_margin
    },
    {
        "line_height",
        layout_line_height
    },
    {
        "text_cleared",
        layout_text_cleared
    },
    {
        "overflow_runtime",
        layout_overflow_runtime
    },
    {
        "node_after_table_move",
        layout_node_after_table_move
    },
    {
        "text_before_node",
        layout_text_before_node
    }
};


static bake_test_suite suites[] = {
    {
        "layout",
        NULL,
        NULL,
        40,
        layout_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siui.test", argc, argv, suites, 1);
}
