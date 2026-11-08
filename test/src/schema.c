#include "component_schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <test.h>

static ecs_component_t changed_component;
static ecs_entity_t changed_entity;
static unsigned changes;

static void schema_changed(ecs_observer_event_t *event) {
    test_uint(event->entity, changed_entity);
    test_uint(event->component, changed_component);
    test_int(event->event, EcsOnSet);
    const double *value = event->trigger_data;
    test_true(value != NULL);
    test_flt(*value, 0);
    test_flt(*(const double *)ecs_get_cid(changed_entity, changed_component), 0);
    test_uint(ecs_component_info(changed_component)->size, 2 * sizeof(double));
    changes++;
}

void schema_dynamic_columns_and_queries(void) {
    ecs_init();
    changed_component = ecs_component_dynamic_init(&(
        ecs_dynamic_component_desc_t
    ){ .name = "Changing", .fields = "{ char *text; }", .inheritance = EcsInheritShared });
    changed_entity = ecs_new();
    const char *text = "old owning value";
    ecs_set_cid(changed_entity, changed_component, &text);
    ecs_entity_t child = ecs_new();
    ecs_is_a(child, changed_entity);
    ecs_query_id_t query = ecs_query_init(&(ecs_query_desc_t){
        .components = { { changed_component, EcsIn }, { ecs_id(Abstract), EcsInOptional } } });
    changes = 0;
    ecs_observer_id_t observer = ecs_observer_init(&(ecs_observer_desc_t){
        .on = EcsOnSet,
        .entity = changed_entity,
        .query.components = { { changed_component, EcsIn }, { ecs_id(Abstract), EcsInOptional } },
        .callback = schema_changed });
    test_true(ecs_component_dynamic_redefine(changed_component, "{ double x; double y; }"));
    test_uint(changes, 1);
    test_uint(ecs_component_lookup("Changing"), changed_component);
    test_true(ecs_has_cid(changed_entity, changed_component));
    test_flt(*(const double *)ecs_get_cid(child, changed_component), 0);
    ecs_observer_fini(observer);
    double next[] = { 8, 9 };
    ecs_set_cid(changed_entity, changed_component, next);
    ecs_iter_t iterator = ecs_query_iter(query);
    unsigned rows = 0;
    while (ecs_iter_next(&iterator)) {
        const double *values = ecs_field(&iterator, 0);
        test_flt(values[0], 8);
        test_flt(values[1], 9);
        rows += iterator.count;
    }
    test_uint(rows, 2);
    ecs_entity_t copy = ecs_new();
    ecs_defer_begin();
    ecs_set_cid(copy, changed_component, next);
    ecs_defer_end();
    test_flt(((const double *)ecs_get_cid(copy, changed_component))[1], 9);
    test_true(ecs_component_dynamic_redefine(changed_component, "{}"));
    test_true(ecs_has_cid(changed_entity, changed_component));
    test_true(ecs_get_cid(changed_entity, changed_component) == NULL);
    test_true(ecs_component_dynamic_redefine(changed_component, "{ char *text; }"));
    test_true(*(char *const *)ecs_get_cid(changed_entity, changed_component) == NULL);
    ecs_set_cid(changed_entity, changed_component, &text);
    test_str(*(char *const *)ecs_get_cid(changed_entity, changed_component), text);
    ecs_fini();
}

void schema_failed_preparation_and_stale_plan(void) {
    ecs_init();
    ecs_component_t component =
        ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "Changing",
                                                                    .fields = "{ int x; }" });
    ecs_entity_t entity = ecs_new();
    int value = 12;
    ecs_set_cid(entity, component, &value);
    sireflect_handle_t previous = ecs_component_info(component)->type;
    test_false(ecs_component_dynamic_redefine(component, "{ Unknown value; }"));
    test_uint(sireflect_type_by_name("Changing"), previous);
    test_uint(ecs_component_info(component)->type, previous);
    test_int(*(const int *)ecs_get_cid(entity, component), 12);
    ecs_component_t fixed =
        ecs_component_init(&(ecs_component_desc_t){ .name = "Fixed", .size = sizeof(int) });
    test_false(ecs_component_dynamic_redefine(fixed, "{}"));
    ecs_defer_begin();
    test_false(ecs_component_dynamic_redefine(component, "{}"));
    ecs_defer_end();
    sireflect_transaction_t *transaction = sireflect_transaction_begin();
    sireflect_handle_t type = sireflect_try_replace_dynamic_struct("Changing", "{ double x; }");
    test_true(type != 0);
    ecs_component_schema_t *plan = ecs_component_schema_prepare(component, type, "{ double x; }");
    test_true(plan != NULL);
    test_true(ecs_component_schema_validate(plan));
    const ecs_pair_desc_t pair_desc = { { component, EcsPairComponent },
                                        { entity, EcsPairEntity },
                                        component };
    test_true(ecs_pair_init(&pair_desc) != 0);
    test_false(ecs_component_schema_validate(plan));
    ecs_component_schema_free(plan);
    plan = ecs_component_schema_prepare(component, type, "{ double x; }");
    test_true(plan != NULL);
    test_true(ecs_component_schema_validate(plan));
    ecs_entity_t other = ecs_new();
    ecs_set_cid(other, component, &value);
    test_false(ecs_component_schema_validate(plan));
    ecs_component_schema_free(plan);
    sireflect_transaction_abort(transaction);
    test_uint(ecs_component_info(component)->type, previous);
    test_int(*(const int *)ecs_get_cid(entity, component), 12);
    test_int(*(const int *)ecs_get_cid(other, component), 12);
    ecs_fini();
}

void schema_scene_roundtrip(void) {
    ecs_init();
    ecs_component_t component =
        ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "Changing",
                                                                    .fields = "{ int x; }" });
    ecs_entity_t entity = ecs_new_named(0, "Saved");
    int old = 5;
    ecs_set_cid(entity, component, &old);
    void *before = NULL;
    size_t before_size = 0;
    test_true(ecs_save_memory(&before, &before_size));
    free(before);
    test_true(ecs_component_dynamic_redefine(component, "{ char *text; double x; }"));
    struct {
        char *text;
        double x;
    } value = { "current schema", 12.5 };
    ecs_set_cid(entity, component, &value);
    void *data = NULL;
    size_t size = 0;
    test_true(ecs_save_memory(&data, &size));
    ecs_fini();
    ecs_init();
    component = ecs_component_dynamic_init(
        &(ecs_dynamic_component_desc_t){ .name = "Changing", .fields = "{ char *text; double x; }" }
    );
    test_true(component != 0);
    test_true(ecs_load_memory(data, size));
    free(data);
    component = ecs_component_lookup("Changing");
    test_true(component != 0);
    const void *loaded = ecs_get_cid(ecs_lookup("Saved"), component);
    test_true(loaded != NULL);
    const sireflect_fields_t *fields = sireflect_type_fields(ecs_component_info(component)->type);
    test_uint(fields->field_count, 2);
    test_str(*(char *const *)((const char *)loaded + fields->fields[0].offset), "current schema");
    test_flt(*(const double *)((const char *)loaded + fields->fields[1].offset), 12.5);
    ecs_fini();
}

static ecs_component_t vector_component(void) {
    sireflect_handle_t vector = sireflect_vector_type(sireflect_type_by_name("char*"));
    /* Typed char pointers are interned by the regular field parser. */
    if (!vector) {
        sireflect_handle_t strings =
            sireflect_try_register_dynamic_struct("VectorString", "{ char *text; }");
        vector = sireflect_vector_type(sireflect_type_fields(strings)->fields[0].type);
    }
    char fields[256];
    snprintf(fields, sizeof fields, "{ %s items; }", sireflect_type_name(vector));
    return ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "VectorData",
                                                                       .fields = fields });
}

void schema_vector_lifecycle_and_scene(void) {
    ecs_init();
    ecs_component_t component = vector_component();
    test_true(component != 0);
    ecs_entity_t entity = ecs_new_named(0, "Vectors");
    char *strings[] = { "alpha", "beta" };
    sireflect_vector_t source = { strings, 2 };
    ecs_set_cid(entity, component, &source);
    const sireflect_vector_t *stored = ecs_get_cid(entity, component);
    test_true(stored->data != strings);
    test_true(((char **)stored->data)[0] != strings[0]);
    test_str(((char **)stored->data)[1], "beta");
    ecs_entity_t clone = ecs_new();
    ecs_defer_begin();
    ecs_set_cid(clone, component, stored);
    ecs_defer_end();
    const sireflect_vector_t *copy = ecs_get_cid(clone, component);
    stored = ecs_get_cid(entity, component);
    test_true(copy->data != stored->data);
    test_true(ecs_reflected_value_equal(ecs_component_info(component)->type, copy, stored));
    void *bytes = NULL;
    size_t size = 0;
    test_true(ecs_save_memory(&bytes, &size));
    ecs_remove_cid(clone, component);
    test_true(ecs_component_dynamic_redefine(component, "{ int value; }"));
    test_int(*(const int *)ecs_get_cid(entity, component), 0);
    ecs_fini();
    ecs_init();
    component = vector_component();
    test_true(ecs_load_memory(bytes, size));
    free(bytes);
    stored = ecs_get_cid(ecs_lookup("Vectors"), component);
    test_uint(stored->count, 2);
    test_str(((char **)stored->data)[0], "alpha");
    test_str(((char **)stored->data)[1], "beta");
    ecs_fini();
}
