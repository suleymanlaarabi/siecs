
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'window'
void window_created_on_set(void);
void window_update_applies_title_and_size(void);
void window_duplicate_slot_rejected(void);
void window_remove_destroys_window(void);
void window_remove_clears_state(void);
void window_destroy_callback_sees_live_window(void);
void window_slot_rename(void);
void window_quit_event_quits_when_kept_open(void);
void window_pointer_ignores_other_window(void);
void window_close_request_quits(void);
void window_close_request_keeps_open(void);
void window_escape_requests_close(void);
void window_escape_kept_open_is_a_key(void);
void window_key_edges_last_one_frame(void);
void window_key_repeat_is_not_a_press(void);
void window_focus_lost_releases_input(void);
void window_pointer_motion_accumulates(void);
void window_pointer_edges_overflow(void);
void window_text_input_event(void);

bake_test_case window_testcases[] = {
    {
        "created_on_set",
        window_created_on_set
    },
    {
        "update_applies_title_and_size",
        window_update_applies_title_and_size
    },
    {
        "duplicate_slot_rejected",
        window_duplicate_slot_rejected
    },
    {
        "remove_destroys_window",
        window_remove_destroys_window
    },
    {
        "remove_clears_state",
        window_remove_clears_state
    },
    {
        "destroy_callback_sees_live_window",
        window_destroy_callback_sees_live_window
    },
    {
        "slot_rename",
        window_slot_rename
    },
    {
        "quit_event_quits_when_kept_open",
        window_quit_event_quits_when_kept_open
    },
    {
        "pointer_ignores_other_window",
        window_pointer_ignores_other_window
    },
    {
        "close_request_quits",
        window_close_request_quits
    },
    {
        "close_request_keeps_open",
        window_close_request_keeps_open
    },
    {
        "escape_requests_close",
        window_escape_requests_close
    },
    {
        "escape_kept_open_is_a_key",
        window_escape_kept_open_is_a_key
    },
    {
        "key_edges_last_one_frame",
        window_key_edges_last_one_frame
    },
    {
        "key_repeat_is_not_a_press",
        window_key_repeat_is_not_a_press
    },
    {
        "focus_lost_releases_input",
        window_focus_lost_releases_input
    },
    {
        "pointer_motion_accumulates",
        window_pointer_motion_accumulates
    },
    {
        "pointer_edges_overflow",
        window_pointer_edges_overflow
    },
    {
        "text_input_event",
        window_text_input_event
    }
};


static bake_test_suite suites[] = {
    {
        "window",
        NULL,
        NULL,
        19,
        window_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siwindow.test", argc, argv, suites, 1);
}
