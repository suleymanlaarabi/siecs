#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <test.h>

void Document_source_history(void) {
    const char *source = "// untouched comment\nRoot { Child {} }\n";
    ecs_script_document_t *document = ecs_script_document_new("scene.si", source, NULL);
    test_true(document != NULL);
    test_false(ecs_script_document_undo(document));
    test_false(ecs_script_document_redo(document));
    size_t offset = (size_t)(strstr(source, "Child") - source);
    test_int(ecs_script_document_edit(document, offset, 5, "Leaf", NULL), 0);
    test_str(
        ecs_script_document_script(document)->code,
        "// untouched comment\nRoot { Leaf {} }\n"
    );
    test_true(ecs_script_document_undo(document));
    test_str(ecs_script_document_script(document)->code, source);
    test_true(ecs_script_document_redo(document));
    test_true(ecs_script_document_undo(document));
    test_int(ecs_script_document_edit(document, offset, 5, "Branch", NULL), 0);
    test_false(ecs_script_document_redo(document));
    test_str(ecs_script_document_script(document)->name, "scene.si");
    ecs_script_document_free(document);
}

void Document_failed_edit_preserves_history(void) {
    ecs_script_document_t *document = ecs_script_document_new("scene.si", "Root {}", NULL);
    test_int(ecs_script_document_edit(document, 0, 4, "Leaf", NULL), 0);
    test_true(ecs_script_document_undo(document));
    const ecs_script_t *previous = ecs_script_document_script(document);
    ecs_script_eval_result_t result = { 0 };
    test_int(ecs_script_document_edit(document, 6, 1, "", &result), -1);
    test_true(result.error != NULL);
    free(result.error);
    test_true(ecs_script_document_script(document) == previous);
    test_true(ecs_script_document_redo(document));
    test_str(ecs_script_document_script(document)->code, "Leaf {}");
    test_int(ecs_script_document_edit(document, 0, 4, "Leaf", NULL), 0);
    test_true(ecs_script_document_undo(document));
    test_str(ecs_script_document_script(document)->code, "Root {}");
    ecs_script_document_free(document);
}

void Document_save_and_evaluate(void) {
    const char *path = "script-document-test.si";
    ecs_script_document_t *document = ecs_script_document_new(path, "Root {}\n", NULL);
    test_int(ecs_script_document_save(document, path, NULL), 0);
    test_int(ecs_script_document_edit(document, 0, 4, "Leaf", NULL), 0);
    test_int(ecs_script_document_save(document, path, NULL), 0);
    FILE *file = fopen(path, "rb");
    test_true(file != NULL);
    char contents[64] = { 0 };
    test_uint(fread(contents, 1, sizeof(contents) - 1, file), 8);
    test_int(fclose(file), 0);
    test_str(contents, "Leaf {}\n");
    ecs_init();
    test_uint(ecs_lookup("Leaf"), 0);
    test_int(ecs_script_eval(ecs_script_document_script(document), NULL, NULL), 0);
    test_true(ecs_lookup("Leaf") != 0);
    test_uint(ecs_lookup("Root"), 0);
    ecs_fini();
    test_true(ecs_script_document_undo(document));
    test_int(remove(path), 0);
    ecs_script_document_free(document);
}

void Document_invalid_arguments(void) {
    test_true(ecs_script_document_new("invalid.si", "Root {", NULL) == NULL);
    test_true(ecs_script_document_new("invalid.si", NULL, NULL) == NULL);
    test_true(ecs_script_document_script(NULL) == NULL);
    test_false(ecs_script_document_undo(NULL));
    test_false(ecs_script_document_redo(NULL));
    ecs_script_document_t *document = ecs_script_document_new("empty.si", "", NULL);
    test_int(ecs_script_document_edit(document, SIZE_MAX, 0, "", NULL), -1);
    test_int(ecs_script_document_edit(document, 0, SIZE_MAX, "", NULL), -1);
    test_int(ecs_script_document_edit(document, 0, 0, NULL, NULL), -1);
    test_int(ecs_script_document_save(document, "", NULL), -1);
    test_int(ecs_script_document_save(document, "missing-directory/source.si", NULL), -1);
    ecs_script_document_free(document);
    ecs_script_document_free(NULL);
}
