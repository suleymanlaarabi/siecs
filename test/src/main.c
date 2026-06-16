
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

// Testsuite 'component'
void component_reflection(void);
void component_on_add(void);

// Testsuite 'childof'
void childof_kill_parent(void);

// Testsuite 'query'
void query_terms_field_order(void);
void query_out_term_matches_and_returns_field(void);
void query_not_excludes_tables(void);

// Testsuite 'system'
void system_run(void);
void system_phase_order(void);
void system_after_order(void);
void system_enable(void);

// Testsuite 'observer'
void observer_enable(void);

// Testsuite 'string'
void string_init(void);
void string_append(void);
void string_trim(void);
void string_starts_ends_with(void);

// Testsuite 'lexer'
void lexer_single_char_tokens(void);
void lexer_two_char_tokens(void);
void lexer_keywords_and_identifiers(void);
void lexer_numbers(void);
void lexer_strings(void);
void lexer_unknown(void);

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

bake_test_case component_testcases[] = {
    {
        "reflection",
        component_reflection
    },
    {
        "on_add",
        component_on_add
    }
};

bake_test_case childof_testcases[] = {
    {
        "kill_parent",
        childof_kill_parent
    }
};

bake_test_case query_testcases[] = {
    {
        "terms_field_order",
        query_terms_field_order
    },
    {
        "out_term_matches_and_returns_field",
        query_out_term_matches_and_returns_field
    },
    {
        "not_excludes_tables",
        query_not_excludes_tables
    }
};

bake_test_case system_testcases[] = {
    {
        "run",
        system_run
    },
    {
        "phase_order",
        system_phase_order
    },
    {
        "after_order",
        system_after_order
    },
    {
        "enable",
        system_enable
    }
};

bake_test_case observer_testcases[] = {
    {
        "enable",
        observer_enable
    }
};

bake_test_case string_testcases[] = {
    {
        "init",
        string_init
    },
    {
        "append",
        string_append
    },
    {
        "trim",
        string_trim
    },
    {
        "starts_ends_with",
        string_starts_ends_with
    }
};

bake_test_case lexer_testcases[] = {
    {
        "single_char_tokens",
        lexer_single_char_tokens
    },
    {
        "two_char_tokens",
        lexer_two_char_tokens
    },
    {
        "keywords_and_identifiers",
        lexer_keywords_and_identifiers
    },
    {
        "numbers",
        lexer_numbers
    },
    {
        "strings",
        lexer_strings
    },
    {
        "unknown",
        lexer_unknown
    }
};


static bake_test_suite suites[] = {
    {
        "entity",
        NULL,
        NULL,
        2,
        entity_testcases
    },
    {
        "component",
        NULL,
        NULL,
        2,
        component_testcases
    },
    {
        "childof",
        NULL,
        NULL,
        1,
        childof_testcases
    },
    {
        "query",
        NULL,
        NULL,
        3,
        query_testcases
    },
    {
        "system",
        NULL,
        NULL,
        4,
        system_testcases
    },
    {
        "observer",
        NULL,
        NULL,
        1,
        observer_testcases
    },
    {
        "string",
        NULL,
        NULL,
        4,
        string_testcases
    },
    {
        "lexer",
        NULL,
        NULL,
        6,
        lexer_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("siecs.test", argc, argv, suites, 8);
}
