/* Differential-test executable. This process links SiECS only. */
#include <siecs_script.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool complete = true;
static uint32_t baseline_components;

static void json_string(const char *value) {
    if (!value) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char*)value; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default: if (*p < 32) printf("\\u%04x", *p); else putchar(*p); break;
        }
    }
    putchar('"');
}

static char *logical_path(const char *native) {
    char *path = malloc(strlen(native) + 1);
    if (!path) exit(2);
    char *out = path;
    if (!strncmp(native, "::", 2)) native += 2;
    while (*native) {
        if (native[0] == ':' && native[1] == ':') { *out++ = '.'; native += 2; }
        else *out++ = *native++;
    }
    *out = '\0';
    if (strchr(path, '#')) complete = false;
    return path;
}

static void entity_path(ecs_entity_t entity) {
    char *native = ecs_entity_path(entity);
    if (!native) { complete = false; fputs("null", stdout); return; }
    char *path = logical_path(native);
    json_string(path);
    free(path);
    free(native);
}

static void value_json(sireflect_handle_t type, const void *ptr) {
    if (!type) { complete = false; fputs("null", stdout); return; }
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info || !ptr) { complete = false; fputs("null", stdout); return; }
    switch (info->kind) {
    case sireflect_kind_struct: {
        putchar('{');
        for (size_t i = 0; i < info->fields.field_count; i++) {
            const sireflect_field_info_t *field = &info->fields.fields[i];
            if (i) putchar(',');
            json_string(field->name); putchar(':');
            value_json(field->type, (const char*)ptr + field->offset);
        }
        putchar('}'); return;
    }
    case sireflect_kind_f32: {
        float number; memcpy(&number, ptr, sizeof number);
        if (isfinite(number)) printf("%.9g", (double)number);
        else { complete = false; fputs("null", stdout); }
        return;
    }
    case sireflect_kind_f64: {
        double number; memcpy(&number, ptr, sizeof number);
        if (isfinite(number)) printf("%.17g", number);
        else { complete = false; fputs("null", stdout); }
        return;
    }
    case sireflect_kind_bool: fputs(*(const bool*)ptr ? "true" : "false", stdout); return;
#define PRINT_SIGNED(K, T) case K: { T number; memcpy(&number, ptr, sizeof number); printf("%" PRId64, (int64_t)number); return; }
#define PRINT_UNSIGNED(K, T) case K: { T number; memcpy(&number, ptr, sizeof number); printf("%" PRIu64, (uint64_t)number); return; }
    PRINT_SIGNED(sireflect_kind_i8, int8_t)
    PRINT_SIGNED(sireflect_kind_i16, int16_t)
    PRINT_SIGNED(sireflect_kind_i32, int32_t)
    PRINT_SIGNED(sireflect_kind_i64, int64_t)
    PRINT_SIGNED(sireflect_kind_char, char)
    PRINT_SIGNED(sireflect_kind_short, short)
    PRINT_SIGNED(sireflect_kind_int, int)
    PRINT_SIGNED(sireflect_kind_long, long)
    PRINT_UNSIGNED(sireflect_kind_u8, uint8_t)
    PRINT_UNSIGNED(sireflect_kind_u16, uint16_t)
    PRINT_UNSIGNED(sireflect_kind_u32, uint32_t)
    PRINT_UNSIGNED(sireflect_kind_u64, uint64_t)
#undef PRINT_SIGNED
#undef PRINT_UNSIGNED
    default: complete = false; fputs("null", stdout); return;
    }
}

static void world_json(void) {
    putchar('[');
    ecs_query_id_t query = ecs_query_init(&(ecs_query_desc_t){ .components = {ecs_in_optional(Abstract), ecs_in_optional(Disabled)} });
    ecs_iter_t it = ecs_query_iter(query);
    bool first = true;
    while (ecs_iter_next(&it)) {
        for (uint32_t row = 0; row < it.count; row++) {
            ecs_entity_t entity = it.entities[row];
            /* Host lifecycle tags need an explicit semantic normalization.
             * Keep them visible and refuse full parity until it exists. */
            if (ecs_has(entity, Abstract) || ecs_has(entity, Disabled)) complete = false;
            if (!first) putchar(',');
            first = false;
            fputs("{\"path\":", stdout); entity_path(entity);
            fputs(",\"components\":{", stdout);
            bool component_first = true;
            for (uint32_t c = 1; c < ecs_component_count(); c++) {
                if (c == ecs_id(Name) || !ecs_has_cid(entity, (ecs_component_t)c)) continue;
                const ecs_component_info_t *info = ecs_component_info((ecs_component_t)c);
                if (!info || !info->name || ecs_component_lookup(info->name) != c) {
                    if (c >= baseline_components) complete = false;
                    continue;
                }
                if (!component_first) putchar(',');
                component_first = false;
                char *name = logical_path(info->name);
                json_string(name); free(name); putchar(':');
                if (info->size) value_json(info->type, ecs_get_cid(entity, (ecs_component_t)c));
                else fputs("null", stdout);
            }
            fputs("},\"relations\":[", stdout);
            bool relation_first = true;
            for (uint32_t r = 1; r < ecs_relation_count(); r++) {
                ecs_entity_t target = ecs_target_id(entity, (ecs_relation_id_t)r);
                if (!target) continue;
                const ecs_relation_info_t *info = ecs_relation_info((ecs_relation_id_t)r);
                if (!relation_first) putchar(',');
                relation_first = false;
                fputs("{\"relation\":", stdout);
                if (!strcmp(info->name, "ChildOf")) json_string("flecs.core.ChildOf");
                else if (!strcmp(info->name, "IsA")) json_string("flecs.core.IsA");
                else json_string(info->name);
                fputs(",\"target\":", stdout); entity_path(target); putchar('}');
            }
            fputs("]}", stdout);
        }
    }
    ecs_query_fini(query);
    putchar(']');
}

int main(int argc, char **argv) {
    if (argc != 5) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 2;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return 2; }
    long length = ftell(file);
    if (length < 0 || length > INT32_MAX / 2) { fclose(file); return 2; }
    rewind(file);
    char *source = calloc(1, (size_t)length + 1);
    if (!source || fread(source, 1, (size_t)length, file) != (size_t)length) { fclose(file); free(source); return 2; }
    fclose(file);
    ecs_init();
    baseline_components = ecs_component_count();
    ecs_script_eval_desc_t desc = {.ir = atoi(argv[2]) != 0, .skip_unknown = atoi(argv[3]) != 0};
    ecs_script_eval_result_t result = {0};
    int status = 0;
    if (!strcmp(argv[4], "parse")) {
        ecs_script_t *script = ecs_script_parse("compat", source, &desc, &result);
        if (!script) status = -1;
        ecs_script_free(script);
    } else status = ecs_script_run_w_desc("compat", source, &desc, &result);
    fputs("{\"runner\":\"siecs\",\"status\":\"OK\",\"success\":", stdout);
    fputs(status ? "false" : "true", stdout);
    fputs(",\"entities\":", stdout); world_json();
    fputs(",\"diagnostics\":[", stdout);
    if (result.error) {
        fputs("{\"category\":\"script_error\",\"symbol\":null,\"message\":", stdout);
        json_string(result.error);
        printf(",\"line\":%d,\"column\":%d}", result.line, result.column);
    }
    printf("],\"events\":[],\"snapshot_complete\":%s}\n", complete ? "true" : "false");
    free(result.error); free(source);
    ecs_fini();
    return 0;
}
