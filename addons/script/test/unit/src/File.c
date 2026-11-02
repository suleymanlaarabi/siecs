#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void File_run_and_errors(void) {
    const char *filename = "script-file-test.si";
    ecs_init();
    FILE *file = fopen(filename, "wb");
    test_true(file != NULL);
    fputs("Root { Child {} }\n", file);
    fclose(file);
    ecs_script_eval_result_t result = {0};
    test_int(ecs_script_run_file(filename, &result), 0);
    test_true(result.error == NULL);
    test_true(ecs_lookup("Root::Child") != 0);
    ecs_entity_t root = ecs_lookup("Root");
    ecs_script_runtime_t *runtime = ecs_script_runtime_new();
    ecs_script_eval_desc_t desc = {.runtime = runtime};
    test_int(ecs_script_run_file_w_desc(filename, &desc, &result), 0);
    test_uint(ecs_lookup("Root"), root);
    file = fopen(filename, "wb");
    test_true(file != NULL);
    fputs("Uncreated {}\nBroken {", file);
    fclose(file);
    test_int(ecs_script_run_file(filename, &result), -1);
    test_true(result.error != NULL);
    test_int(result.line, 2);
    test_uint(ecs_lookup("Uncreated"), 0);
    free(result.error);
    file = fopen(filename, "wb");
    test_true(file != NULL);
    const char embedded_null[] = "Uncreated {}\0Other {}";
    fwrite(embedded_null, 1, sizeof embedded_null - 1, file);
    fclose(file);
    test_int(ecs_script_run_file(filename, &result), -1);
    test_true(result.error != NULL);
    test_uint(ecs_lookup("Uncreated"), 0);
    free(result.error);
    file = fopen(filename, "wb");
    test_true(file != NULL);
    fclose(file);
    test_int(ecs_script_run_file(filename, &result), 0);
    test_int(remove(filename), 0);
    test_int(ecs_script_run_file(filename, &result), -1);
    test_true(result.error != NULL);
    free(result.error);
    test_int(ecs_script_run_file(NULL, &result), -1);
    test_true(result.error != NULL);
    free(result.error);
    ecs_script_runtime_free(runtime);
    ecs_fini();
}
