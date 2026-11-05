
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'Script'
void Script_parse_without_world(void);
void Script_typed_components_and_templates(void);
void Script_reusable_runtime_and_moves(void);
void Script_owned_diagnostics(void);
void Script_options_and_recovery(void);
void Script_string_views_and_null_bytes(void);
void Script_files(void);
void Script_source_documents(void);
void Script_persistent_instances(void);
void Script_document_applies_transactionally(void);
void Script_typed_worker_task(void);
void Script_cancels_worker_task(void);
void Script_owned_worker_result(void);
void Script_controlled_clock(void);
void Script_scoped_budget(void);

bake_test_case Script_testcases[] = {
    {
        "parse_without_world",
        Script_parse_without_world
    },
    {
        "typed_components_and_templates",
        Script_typed_components_and_templates
    },
    {
        "reusable_runtime_and_moves",
        Script_reusable_runtime_and_moves
    },
    {
        "owned_diagnostics",
        Script_owned_diagnostics
    },
    {
        "options_and_recovery",
        Script_options_and_recovery
    },
    {
        "string_views_and_null_bytes",
        Script_string_views_and_null_bytes
    },
    {
        "files",
        Script_files
    },
    {
        "source_documents",
        Script_source_documents
    },
    {
        "persistent_instances",
        Script_persistent_instances
    },
    {
        "document_applies_transactionally",
        Script_document_applies_transactionally
    },
    {
        "typed_worker_task",
        Script_typed_worker_task
    },
    {
        "cancels_worker_task",
        Script_cancels_worker_task
    },
    {
        "owned_worker_result",
        Script_owned_worker_result
    },
    {
        "controlled_clock",
        Script_controlled_clock
    },
    {
        "scoped_budget",
        Script_scoped_budget
    }
};


static bake_test_suite suites[] = {
    {
        "Script",
        NULL,
        NULL,
        15,
        Script_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs_script.cpp.test", argc, argv, suites, 1);
}
