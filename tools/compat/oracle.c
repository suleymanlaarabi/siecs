/* External test oracle only. Never link this executable with SiECS. */
#include <flecs.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void json_string(const char *value) {
    if (!value) {
        fputs("null", stdout);
        return;
    }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 32) {
                printf("\\u%04x", *p);
            } else {
                putchar(*p);
            }
        }
    }
    putchar('"');
}

static int compare_entity(const void *a, const void *b) {
    ecs_entity_t first = *(const ecs_entity_t *)a;
    ecs_entity_t second = *(const ecs_entity_t *)b;
    return (first > second) - (first < second);
}

static char *read_source(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *source = NULL;
    size_t size = 0;
    size_t capacity = 0;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (size + 1 >= capacity) {
            size_t next_capacity = capacity ? capacity * 2 : 4096;
            char *next = realloc(source, next_capacity);
            if (!next) {
                free(source);
                fclose(file);
                return NULL;
            }
            capacity = next_capacity;
            source = next;
        }
        source[size++] = (char)ch;
    }
    if (ferror(file)) {
        free(source);
        fclose(file);
        return NULL;
    }
    fclose(file);
    if (!source) {
        source = malloc(1);
    }
    if (source) {
        source[size] = '\0';
    }
    return source;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fputs("usage: oracle <source.flecs> <ir:0|1> <skip_unknown:0|1> <run|parse>\n", stderr);
        return 2;
    }
    char *source = read_source(argv[1]);
    if (!source) {
        fputs("cannot read source\n", stderr);
        return 2;
    }
    ecs_world_t *world = ecs_init();
    ecs_log_set_level(-4);
    ecs_entities_t entities = ecs_get_entities(world);
    size_t baseline_count = (size_t)entities.alive_count;
    ecs_entity_t *baseline = malloc((baseline_count + 1) * sizeof *baseline);
    if (!baseline) {
        free(source);
        ecs_fini(world);
        return 2;
    }
    memcpy(baseline, entities.ids, baseline_count * sizeof *baseline);
    qsort(baseline, baseline_count, sizeof *baseline, compare_entity);
    char **baseline_json = calloc(baseline_count + 1, sizeof *baseline_json);
    if (!baseline_json) {
        free(baseline);
        free(source);
        ecs_fini(world);
        return 2;
    }
    ecs_entity_to_json_desc_t json_desc = ECS_ENTITY_TO_JSON_INIT;
    for (size_t i = 0; i < baseline_count; i++) {
        baseline_json[i] = ecs_entity_to_json(world, baseline[i], &json_desc);
    }

    ecs_script_eval_desc_t desc = {
        .ir = !strcmp(argv[2], "1"),
        .skip_unknown = !strcmp(argv[3], "1"),
    };
    ecs_script_eval_result_t result = {0};
    int status;
    if (!strcmp(argv[4], "parse")) {
        ecs_script_t *script = ecs_script_parse(world, "compat.flecs", source, &desc, &result);
        status = script ? 0 : -1;
        if (script) {
            ecs_script_free(script);
        }
    } else {
        status = ecs_script_run_w_desc(world, "compat.flecs", source, &desc, &result);
    }

    printf("{\"success\":%s,\"diagnostics\":[", status == 0 ? "true" : "false");
    if (result.error) {
        printf("{\"line\":%d,\"column\":%d,\"category\":\"script_error\",\"message\":",
               result.line, result.column);
        json_string(result.error);
        putchar('}');
    }
    fputs("],\"raw_entities\":[", stdout);
    entities = ecs_get_entities(world);
    bool first = true;
    bool snapshot_complete = true;
    for (size_t i = 0; i < baseline_count; i++) {
        char *after = ecs_is_alive(world, baseline[i])
            ? ecs_entity_to_json(world, baseline[i], &json_desc) : NULL;
        if (!baseline_json[i] || !after || strcmp(baseline_json[i], after)) {
            snapshot_complete = false;
        }
        ecs_os_free(after);
        ecs_os_free(baseline_json[i]);
    }
    free(baseline_json);
    for (int32_t i = 0; i < entities.alive_count; i++) {
        ecs_entity_t entity = entities.ids[i];
        if (bsearch(&entity, baseline, baseline_count, sizeof *baseline, compare_entity)) {
            continue;
        }
        char *serialized = ecs_entity_to_json(world, entity, &json_desc);
        char *path = ecs_get_path(world, entity);
        if (!serialized || !path) {
            snapshot_complete = false;
            ecs_os_free(serialized);
            ecs_os_free(path);
            continue;
        }
        if (!first) {
            putchar(',');
        }
        first = false;
        fputs("{\"path\":", stdout);
        json_string(path);
        fputs(",\"data\":", stdout);
        fputs(serialized, stdout);
        putchar('}');
        ecs_os_free(serialized);
        ecs_os_free(path);
    }
    printf("],\"snapshot_complete\":%s}\n", snapshot_complete ? "true" : "false");
    ecs_os_free(result.error);
    free(baseline);
    free(source);
    ecs_fini(world);
    return 0;
}
