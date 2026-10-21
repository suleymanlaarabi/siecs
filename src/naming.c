#include "naming.h"
#include "command_buffer.h"
#include "event_ops.h"
#include "storage/entity_index.h"
#include "table.h"
#include "utils.h"
#include "world_internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* No formatted composite keys, path copies, or allocations on the lookup path. */
static ecs_name_index_t names;
/* Root hits retain Sicore's compact SIMD string index and skip path parsing.
 * The canonical parent/name index also accepts non-terminated path segments. */
static sicore_map_t root_names;

/* Wyhash-style word mixing (public domain), seeded by the complete parent
 * handle. Short names need two multiplies rather than one multiply per byte. */
static uint64_t ecs_name_mix(uint64_t a, uint64_t b) {
#if defined(__SIZEOF_INT128__)
    __uint128_t product = (__uint128_t)a * b;
    return (uint64_t)product ^ (uint64_t)(product >> 64);
#else
    uint64_t ah = a >> 32, al = (uint32_t)a, bh = b >> 32, bl = (uint32_t)b;
    uint64_t high = ah * bh, middle0 = ah * bl, middle1 = al * bh, low = al * bl;
    uint64_t t = low + (middle0 << 32), carry = t < low;
    uint64_t out = t + (middle1 << 32);
    carry += out < t;
    return out ^ (high + (middle0 >> 32) + (middle1 >> 32) + carry);
#endif
}

static uint64_t ecs_name_read64(const char *value) {
    uint64_t result;
    memcpy(&result, value, sizeof result);
    return result;
}

static uint64_t ecs_name_read32(const char *value) {
    uint32_t result;
    memcpy(&result, value, sizeof result);
    return result;
}

static uint32_t ecs_name_hash(ecs_entity_t parent, const char *name, size_t length) {
    const uint64_t secret = UINT64_C(0xe7037ed1a0b428db);
    uint64_t seed = parent ^ UINT64_C(0xa0761d6478bd642f);
    size_t remaining = length;
    while (remaining > 16) {
        seed = ecs_name_mix(ecs_name_read64(name) ^ secret, ecs_name_read64(name + 8) ^ seed);
        name += 16;
        remaining -= 16;
    }
    uint64_t a = 0, b = 0;
    if (remaining > 8) {
        a = ecs_name_read64(name);
        b = ecs_name_read64(name + remaining - 8);
    } else if (remaining >= 4) {
        a = ecs_name_read32(name);
        b = ecs_name_read32(name + remaining - 4);
    } else if (remaining) {
        a = ((uint64_t)(unsigned char)name[0] << 16) |
            ((uint64_t)(unsigned char)name[remaining >> 1] << 8) |
            (unsigned char)name[remaining - 1];
    }
    uint64_t hash = ecs_name_mix(secret ^ length, ecs_name_mix(a ^ secret, b ^ seed));
    uint32_t result = (uint32_t)(hash ^ (hash >> 32));
    return result >= 2 ? result : result + 2; /* 0 empty, 1 deleted */
}

static bool ecs_name_entry_matches(
    const ecs_name_entry_t *entry,
    uint32_t hash,
    ecs_entity_t parent,
    const char *name,
    size_t length
) {
    return entry->hash == hash && entry->parent == parent && entry->length == length &&
           memcmp(entry->name, name, length) == 0;
}

ecs_entity_t ecs_name_index_find(
    const ecs_name_index_t *index,
    ecs_entity_t parent,
    const char *name,
    size_t length
) {
    if (!index->count)
        return 0;
    uint32_t hash = ecs_name_hash(parent, name, length);
    uint32_t mask = index->capacity - 1;
    uint32_t at = (uint32_t)hash & mask;
    while (index->entries[at].hash) {
        if (ecs_name_entry_matches(&index->entries[at], hash, parent, name, length))
            return index->entries[at].entity;
        at = (at + 1) & mask;
    }
    return 0;
}

static void ecs_name_index_rehash(ecs_name_index_t *index, uint32_t capacity) {
    ecs_name_entry_t *entries = calloc(capacity, sizeof *entries);
    ecs_assert_not_null(entries);
    for (uint32_t i = 0; i < index->capacity; i++) {
        ecs_name_entry_t entry = index->entries[i];
        if (entry.hash < 2)
            continue;
        uint32_t at = (uint32_t)entry.hash & (capacity - 1);
        while (entries[at].hash)
            at = (at + 1) & (capacity - 1);
        entries[at] = entry;
    }
    free(index->entries);
    index->entries = entries;
    index->capacity = capacity;
    index->deleted = 0;
}

void ecs_name_index_insert(
    ecs_name_index_t *index,
    ecs_entity_t parent,
    const char *name,
    ecs_entity_t entity
) {
    if (!index->capacity || (index->count + index->deleted + 1) * 4 >= index->capacity * 3) {
        uint32_t capacity = index->capacity ? index->capacity : 16;
        if ((index->count + 1) * 4 >= capacity * 3)
            capacity *= 2;
        ecs_name_index_rehash(index, capacity);
    }
    size_t length = strlen(name);
    uint32_t hash = ecs_name_hash(parent, name, length);
    uint32_t mask = index->capacity - 1, at = (uint32_t)hash & mask;
    uint32_t deleted = UINT32_MAX;
    while (index->entries[at].hash) {
        if (ecs_name_entry_matches(&index->entries[at], hash, parent, name, length)) {
            index->entries[at].entity = entity;
            index->entries[at].name = name;
            return;
        }
        if (index->entries[at].hash == 1 && deleted == UINT32_MAX)
            deleted = at;
        at = (at + 1) & mask;
    }
    if (deleted != UINT32_MAX) {
        at = deleted;
        index->deleted--;
    }
    index->entries[at] = (ecs_name_entry_t){ .parent = parent,
                                             .entity = entity,
                                             .name = name,
                                             .hash = hash,
                                             .length = (uint32_t)length };
    index->count++;
}

void ecs_name_index_remove(
    ecs_name_index_t *index,
    ecs_entity_t parent,
    const char *name,
    ecs_entity_t entity
) {
    if (!name || !index->count)
        return;
    size_t length = strlen(name);
    uint32_t hash = ecs_name_hash(parent, name, length);
    uint32_t mask = index->capacity - 1, at = (uint32_t)hash & mask;
    while (index->entries[at].hash) {
        ecs_name_entry_t *entry = &index->entries[at];
        if (ecs_name_entry_matches(entry, hash, parent, name, length)) {
            if (entry->entity == entity) {
                *entry = (ecs_name_entry_t){ .hash = 1 };
                index->count--;
                index->deleted++;
            }
            return;
        }
        at = (at + 1) & mask;
    }
}

void ecs_name_index_fini(ecs_name_index_t *index) {
    free(index->entries);
    *index = (ecs_name_index_t){ 0 };
}

void ecs_names_init(void) { sicore_map_init(&root_names); }

void ecs_names_fini(void) {
    ecs_name_index_fini(&names);
    sicore_map_fini(&root_names);
}

bool ecs_name_entity_alive(ecs_entity_t entity) { return ecs_is_alive(entity); }

bool ecs_name_valid(const char *name) {
    if (!name || !name[0])
        return true;
    size_t length = strlen(name);
    /* A trailing : would merge with the separator before the next segment. */
    return length <= UINT32_MAX && name[0] != '#' && name[length - 1] != ':' && !strstr(name, "::");
}

const char *ecs_name_local(ecs_entity_t entity) {
    const ecs_entity_record_t *record = ecs_get_record(entity);
    const ecs_table_t *table = ecs_get_table(ecs_entity_record_table(record));
    uint16_t column = ecs_table_column_or_invalid(table, ecs_id(Name));
    if (column == UINT16_MAX)
        return NULL;
    const Name *name = ecs_table_component_at_column(table, column, record->table_row);
    return name->value && name->value[0] ? name->value : NULL;
}

ecs_entity_t ecs_name_parent(ecs_entity_t entity) {
    return ecs_target_id(entity, ecs_rid(ChildOf));
}

static void ecs_names_remove(ecs_entity_t entity, ecs_entity_t parent, const char *name) {
    ecs_name_index_remove(&names, parent, name, entity);
    if (!parent) {
        uint32_t index = sicore_map_get(&root_names, name);
        if (index != UINT32_MAX && ecs_entity_from_index(index) == entity)
            sicore_map_unset(&root_names, name);
    }
}

void ecs_name_unregister(ecs_entity_t entity) {
    const char *name = ecs_name_local(entity);
    if (name)
        ecs_names_remove(entity, ecs_name_parent(entity), name);
}

void ecs_name_on_remove(ecs_entity_t entity, ecs_component_t component, void *data) {
    (void)component;
    const Name *name = data;
    if (name->value)
        ecs_names_remove(entity, ecs_name_parent(entity), name->value);
}

static bool ecs_name_is_ancestor(ecs_entity_t ancestor, ecs_entity_t entity) {
    while (entity) {
        if (entity == ancestor)
            return true;
        entity = ecs_name_parent(entity);
    }
    return false;
}

bool ecs_name_update_now(
    ecs_entity_t entity,
    const char *name,
    bool set_name,
    ecs_entity_t parent,
    bool set_parent
) {
    if (!ecs_name_entity_alive(entity) || (set_name && !ecs_name_valid(name)))
        return false;
    ecs_entity_t old_parent = ecs_name_parent(entity);
    if (!set_parent)
        parent = old_parent;
    if (parent != old_parent && parent &&
        (!ecs_name_entity_alive(parent) || ecs_name_is_ancestor(entity, parent)))
        return false;
    const char *old_name = ecs_name_local(entity);
    if (!set_name)
        name = old_name;
    if (name && !name[0])
        name = NULL;
    bool rename = set_name && (old_name || name);
    if (old_name && name && strcmp(old_name, name) == 0)
        rename = false;
    if (!rename && parent == old_parent &&
        !(set_name && !name && ecs_has_cid_owned(entity, ecs_id(Name))))
        return true;

    ecs_entity_t victim = name ? ecs_name_index_find(&names, parent, name, strlen(name)) : 0;
    if (victim == entity)
        victim = 0;
    if (victim && (ecs_name_is_ancestor(victim, entity) || ecs_name_is_ancestor(victim, parent)))
        return false;

    /* Copy before deleting a victim: input may alias either entity's storage. */
    char *copy = rename && name ? strdup(name) : NULL;
    if (rename && name && !copy)
        return false;
    ecs_defer_begin();
    if (victim)
        ecs_kill_now(victim);
    ecs_name_unregister(entity);
    if (parent != old_parent)
        ecs_childof_update_now(entity, parent);
    if (set_name && !name) {
        ecs_remove_cid_now(entity, ecs_id(Name));
    } else if (rename) {
        ecs_add_cid_now(entity, ecs_id(Name));
        Name *value = ecs_get_cid(entity, ecs_id(Name));
        free(value->value);
        value->value = copy;
        copy = NULL;
    }
    const char *current = ecs_name_local(entity);
    if (current) {
        ecs_name_index_insert(&names, parent, current, entity);
        if (!parent)
            sicore_map_set(&root_names, current, ecs_first(entity));
    }
    if (parent != old_parent) {
        ecs_emit_relation_event(
            entity,
            ecs_rid(ChildOf),
            parent ? EcsOnRelationSet : EcsOnRelationRemove,
            old_parent,
            parent
        );
    }
    if (rename && name) {
        ecs_entity_record_t *record = ecs_get_record(entity);
        ecs_table_t *table = ecs_get_table(ecs_entity_record_table(record));
        const Name *value = ecs_get_cid(entity, ecs_id(Name));
        ecs_emit(table, entity, EcsOnSet, ecs_id(Name), value);
    }
    ecs_defer_end();
    free(copy);
    return true;
}

bool ecs_set_name(ecs_entity_t entity, const char *name) {
    ecs_assert_not_scheduler_parallel("entity naming");
    if (!ecs_name_entity_alive(entity) || !ecs_name_valid(name))
        return false;
    if (ecs_is_deferred()) {
        ecs_command_buffer_identity(entity, name, true, 0, false);
        return true;
    }
    return ecs_name_update_now(entity, name, true, 0, false);
}

/* Reserved anonymous segments include the generation, never just a recycled index. */
static ecs_entity_t ecs_name_anonymous(const char *segment, size_t length) {
    if (length < 4 || segment[0] != '#')
        return 0;
    uint64_t parts[2] = { 0 };
    unsigned part = 0;
    bool digit = false;
    for (size_t i = 1; i < length; i++) {
        unsigned char c = (unsigned char)segment[i];
        if (c == ':' && !part && digit) {
            part = 1;
            digit = false;
            continue;
        }
        if (c < '0' || c > '9')
            return 0;
        digit = true;
        parts[part] = parts[part] * 10 + c - '0';
        if (parts[part] > UINT32_MAX)
            return 0;
    }
    if (!part || !digit || !parts[0])
        return 0;
    ecs_entity_t entity = ecs_entity((uint32_t)parts[0], (uint32_t)parts[1]);
    return ecs_name_entity_alive(entity) ? entity : 0;
}

static bool ecs_name_path_valid(const char *path) {
    if (!path || !*path)
        return false;
    if (path[0] == ':' && path[1] == ':')
        path += 2;
    if (!*path)
        return false;
    while (*path) {
        const char *end = strstr(path, "::");
        size_t length = end ? (size_t)(end - path) : strlen(path);
        if (!length || path[length - 1] == ':' ||
            (path[0] == '#' && !ecs_name_anonymous(path, length)))
            return false;
        if (!end)
            return true;
        path = end + 2;
        if (!*path)
            return false;
    }
    return false;
}

ecs_entity_t ecs_name_lookup_segment(ecs_entity_t parent, const char *segment, size_t length) {
    if (segment[0] == '#') {
        ecs_entity_t entity = ecs_name_anonymous(segment, length);
        return entity && !ecs_name_local(entity) && ecs_name_parent(entity) == parent ? entity : 0;
    }
    return ecs_name_index_find(&names, parent, segment, length);
}

static ecs_entity_t ecs_name_resolve(ecs_entity_t parent, const char *path, bool create) {
    /* Read-only lookup validates while walking; creation validates before mutation. */
    if (!path || !*path || (create && !ecs_name_path_valid(path)))
        return 0;
    if (path[0] == ':' && path[1] == ':') {
        parent = 0;
        path += 2;
    }
    if (!*path)
        return 0;
    if (parent && !ecs_name_entity_alive(parent))
        return 0;
    while (*path) {
        const char *end = strstr(path, "::");
        size_t length = end ? (size_t)(end - path) : strlen(path);
        if (!length || (end && !end[2]))
            return 0;
        ecs_entity_t found = create && ecs_is_deferred()
                                 ? ecs_command_buffer_lookup_name(parent, path, length)
                                 : ecs_name_lookup_segment(parent, path, length);
        if (!found && create && path[0] != '#') {
            char *local = malloc(length + 1);
            if (!local)
                return 0;
            memcpy(local, path, length);
            local[length] = '\0';
            found = ecs_new();
            if (ecs_is_deferred())
                ecs_command_buffer_identity(found, local, true, parent, true);
            else if (!ecs_name_update_now(found, local, true, parent, true)) {
                ecs_kill_now(found);
                found = 0;
            }
            free(local);
        }
        if (!found)
            return 0;
        parent = found;
        if (!end)
            break;
        path = end + 2;
    }
    return parent;
}

ecs_entity_t ecs_lookup_from(ecs_entity_t parent, const char *path) {
    return parent ? ecs_name_resolve(parent, path, false) : ecs_lookup(path);
}

ecs_entity_t ecs_lookup(const char *path) {
    if (!path || !*path)
        return 0;
    if (path[0] != '#' && !(path[0] == ':' && path[1] == ':')) {
        uint32_t index = sicore_map_get(&root_names, path);
        if (index != UINT32_MAX)
            return ecs_entity(index, ecs_entity_index_get_record(index)->generation);
        if (!strstr(path, "::"))
            return 0;
    }
    return ecs_name_resolve(0, path, false);
}

ecs_entity_t ecs_new_named(ecs_entity_t parent, const char *path) {
    ecs_assert_not_scheduler_parallel("named entity creation");
    if (path)
        return ecs_name_resolve(parent, path, true);
    if (parent && !ecs_name_entity_alive(parent))
        return 0;
    ecs_entity_t entity = ecs_new();
    if (parent)
        ecs_relate_id(entity, ecs_rid(ChildOf), parent);
    return entity;
}

char *ecs_entity_path(ecs_entity_t entity) {
    if (!ecs_name_entity_alive(entity))
        return NULL;
    size_t size = 1;
    for (ecs_entity_t current = entity; current; current = ecs_name_parent(current)) {
        const char *name = ecs_name_local(current);
        size += 2 + (name ? strlen(name)
                          : (size_t)snprintf(
                                NULL,
                                0,
                                "#%" PRIu32 ":%" PRIu32,
                                ecs_first(current),
                                ecs_second(current)
                            ));
    }
    char *path = malloc(size);
    if (!path)
        return NULL;
    char *out = path + size - 1;
    *out = '\0';
    for (ecs_entity_t current = entity; current; current = ecs_name_parent(current)) {
        const char *name = ecs_name_local(current);
        char anonymous[32];
        if (!name) {
            snprintf(
                anonymous,
                sizeof anonymous,
                "#%" PRIu32 ":%" PRIu32,
                ecs_first(current),
                ecs_second(current)
            );
            name = anonymous;
        }
        size_t length = strlen(name);
        out -= length;
        memcpy(out, name, length);
        *--out = ':';
        *--out = ':';
    }
    return path;
}
