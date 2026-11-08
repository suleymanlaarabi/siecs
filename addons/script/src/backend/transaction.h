#ifndef SIECS_SCRIPT_TRANSACTION_H
#define SIECS_SCRIPT_TRANSACTION_H
#include "siecs_backend.h"

/* Native generations occupy only the low 16 bits. The unused upper half
 * of the generation word distinguishes unpublished transaction handles. */
#define SCRIPT_ENTITY_PLACEHOLDER (UINT64_C(1) << 31)
static inline bool script_entity_is_placeholder(ecs_entity_t entity) {
    return (entity & SCRIPT_ENTITY_PLACEHOLDER) != 0;
}

typedef struct script_staged_pair_t {
    ecs_pair_desc_t desc;
    script_key_t committed;
} script_staged_pair_t;

typedef struct script_staged_entity_t {
    ecs_entity_t handle;
    ecs_entity_t parent;
    char *name;
    bool abstract;
} script_staged_entity_t;
typedef struct script_staged_type_t {
    ecs_component_t handle;
    ecs_component_info_t info;
    ecs_type_ops_t ops;
    char *fields;
    ecs_component_t committed;
    ecs_component_t replaced;
} script_staged_type_t;
typedef struct script_staged_value_t {
    ecs_entity_t entity;
    script_key_t component;
    void *data;
    bool remove;
    bool unchanged;
    uint8_t *members; /* NULL owns the complete component. */
    uint32_t region;
    bool cached;
    sireflect_handle_t schema;
    ecs_type_ops_t ops;
} script_staged_value_t;
typedef struct script_staged_relation_t {
    ecs_entity_t entity;
    ecs_relation_id_t relation;
    ecs_entity_t target;
} script_staged_relation_t;
typedef struct script_read_t {
    ecs_entity_t entity;
    script_key_t component;
    sireflect_handle_t type;
    sireflect_handle_t schema;
    size_t offset;
    void *snapshot;
    bool present;
    uint32_t region;
    ecs_entity_t base; /* component == 0 tracks an IsA link. */
} script_read_t;
typedef struct script_lookup_t {
    char *name;
    ecs_entity_t parent;
    ecs_entity_t resolved;
    uint32_t region;
} script_lookup_t;
typedef struct script_region_t {
    const ecs_script_t *program;
    const ecs_script_node_t *node;
    ecs_entity_t parent;
    struct script_eval_t *captures;
    struct script_eval_t *result;
    char *name;
    uint32_t values_begin, values_end;
    uint32_t reads_begin, reads_end;
    uint32_t lookups_begin, lookups_end;
    uint32_t produced_begin, produced_end;
    uint32_t relations_begin, relations_end;
    uint32_t prefabs_begin, prefabs_end;
    uint32_t identities_begin, identities_end;
    uint32_t children_end;
    bool leaf;
    bool reusable;
} script_region_t;
typedef struct script_identity_t {
    char *source;
    uint32_t node;
    uint64_t fingerprint;
    uint32_t occurrence;
    ecs_entity_t parent;
    ecs_entity_t entity;
} script_identity_t;
typedef struct script_transaction_t {
    sicore_vec_t entities;
    sicore_vec_t types;
    sicore_vec_t pairs;
    sicore_vec_t values;
    sicore_vec_t relations;
    sicore_vec_t prefabs;
    sicore_vec_t reads;
    sicore_vec_t lookups;
    sicore_vec_t produced;
    sicore_vec_t identities;
    sicore_vec_t regions;
    uint32_t region;
    sicore_vec_t kills;
    sicore_vec_t async_blocks;
    sicore_vec_t async_launches;
    void (*publish)(struct script_transaction_t *, void *);
    void *context;
    sireflect_transaction_t *reflection;
    bool failed;
    ecs_script_instance_t *instance;
    bool reconcile_async;
    uint32_t owned_count;
} script_transaction_t;

typedef struct script_savepoint_t {
    script_transaction_t saved;
    uint32_t reflection_mark;
    uint32_t block_count;
    uint32_t launch_count;
} script_savepoint_t;
void script_transaction_savepoint(script_transaction_t *transaction, script_savepoint_t *savepoint);
void script_transaction_rewind(script_transaction_t *transaction, script_savepoint_t *savepoint);
void script_transaction_discard_savepoint(script_savepoint_t *savepoint);

#if defined(_MSC_VER)
extern __declspec(thread) script_transaction_t *script_transaction_active;
#else
extern _Thread_local script_transaction_t *script_transaction_active;
#endif
script_key_t
script_transaction_pair(script_transaction_t *transaction, const ecs_pair_desc_t *desc);
ecs_component_t script_key_payload(script_transaction_t *transaction, script_key_t key);
void script_transaction_prefab(script_transaction_t *transaction, ecs_entity_t entity);
ecs_entity_t script_transaction_parent(script_transaction_t *transaction, ecs_entity_t entity);
script_staged_value_t *script_transaction_find(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    script_key_t component
);
bool script_transaction_has(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    script_key_t component
);
void script_transaction_read(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    script_key_t component
);
void script_transaction_read_member(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    script_key_t component,
    sireflect_handle_t type,
    size_t offset
);
bool script_read_changed(const script_read_t *read);
bool script_instance_region_replay(
    ecs_script_instance_t *instance,
    script_transaction_t *transaction,
    const ecs_script_t *program,
    const ecs_script_node_t *node,
    ecs_entity_t parent,
    struct script_eval_t *context
);
bool script_instance_owns_schema(ecs_script_instance_t *instance, script_key_t component);
bool script_transaction_replaces(const script_transaction_t *transaction, script_key_t component);
ecs_entity_t script_instance_anonymous(
    ecs_script_instance_t *instance,
    script_transaction_t *transaction,
    const ecs_script_t *program,
    const ecs_script_entity_t *node,
    ecs_entity_t parent
);
void script_transaction_lookup(
    script_transaction_t *transaction,
    const char *name,
    ecs_entity_t parent
);
void script_transaction_remap_value(
    script_transaction_t *transaction,
    sireflect_handle_t type,
    void *data
);
ecs_entity_t
script_transaction_committed_entity(script_transaction_t *transaction, ecs_entity_t entity);
script_key_t
script_transaction_committed_component(script_transaction_t *transaction, script_key_t component);
void script_transaction_init(script_transaction_t *transaction);
void script_transaction_fini(script_transaction_t *transaction);
int script_transaction_commit(script_transaction_t *transaction);
ecs_entity_t script_transaction_entity(
    script_transaction_t *transaction,
    const char *name,
    ecs_entity_t parent,
    bool create
);
char *script_transaction_path(script_transaction_t *transaction, ecs_entity_t entity);
ecs_component_t script_transaction_type(
    script_transaction_t *transaction,
    const char *name,
    const char *fields,
    bool create
);
const ecs_component_info_t *
script_transaction_info(script_transaction_t *transaction, script_key_t component);
void script_transaction_set(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    script_key_t component,
    const void *value,
    bool remove
);
void script_transaction_set_members(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    script_key_t component,
    const void *value,
    const uint8_t *members
);
void script_output_fini(script_staged_value_t *output);
bool script_output_equal(const script_staged_value_t *output, const void *live);
bool script_output_reads(const script_staged_value_t *output, const script_read_t *read);
bool script_output_overlap(script_key_t component, const uint8_t *left, const uint8_t *right);
void script_output_copy_members(
    script_key_t component,
    void *destination,
    const void *source,
    const uint8_t *members
);
uint8_t *script_output_copy_mask(sireflect_handle_t type, const uint8_t *members);
const void *script_transaction_get(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    script_key_t component
);
int script_transaction_relate(
    script_transaction_t *transaction,
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
);
const ecs_type_ops_t *
script_transaction_ops(script_transaction_t *transaction, script_key_t component);
#endif
