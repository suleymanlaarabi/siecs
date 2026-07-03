---
title: Inheritance
description: Abstract entities, inheritance links, and inherited queries.
---

Inheritance lets an entity use another entity as a base for shared component
data. It is useful for prototypes, presets, and categories where many entities
share the same default values.

An entity can inherit from one base. That base must be marked with the built-in
`Abstract` component.

## Create A Base

```c
ecs_entity_t base = ecs_new(world);
ecs_set(world, base, Health, { 100 });
ecs_set(world, base, Speed, { 4.0f });
ecs_add(world, base, Abstract);
```

Create the base with `ecs_new()`, add the default component values, then add
`Abstract`.

Once an entity has `Abstract`, normal component add, set, and remove operations
on that entity are rejected. Put component data on the base before marking it
abstract.

Use a normal entity instead of an abstract one when you need to keep mutating its
components directly.

## Create An Instance

Create another entity and link it to the abstract base with `ecs_is_a()`:

```c
ecs_entity_t instance = ecs_new(world);
ecs_is_a(world, instance, base);
```

`ecs_is_a(world, instance, base)` moves `instance` to an archetype table whose
type records `base` as its inheritance target. Local component data already on
the instance is kept.

An entity has one base at a time. Calling `ecs_is_a()` again with the same base
is a no-op. Calling it with another abstract base replaces the inheritance
target in the entity type and moves the entity to the matching archetype table.

The target must be alive, abstract, and different from the instance. Cycles are
rejected, so an entity cannot inherit from itself or from one of its descendants.

## Inherit Components

Inherited components are shared from the base. If the base owns `Health` and the
instance does not, read-only queries can still see `Health` on the instance:

```c
ecs_entity_t base = ecs_new(world);
ecs_set(world, base, Health, { 100 });
ecs_add(world, base, Abstract);

ecs_entity_t instance = ecs_new(world);
ecs_is_a(world, instance, base);
```

In this example, `instance` inherits `Health` from `base`. The inherited value is
not copied into the instance. It remains stored on `base`, and all instances that
inherit it read the same shared value.

## Override Components

Add or set a component on the instance to override the inherited value:

```c
ecs_set(world, instance, Health, { 75 });
```

After this, `instance` owns its own `Health`. Queries read the instance value
instead of the base value, and writes affect only the instance:

```c
const Health *base_health = ecs_get(world, base, Health);
const Health *instance_health = ecs_get(world, instance, Health);
```

Local components are independent from inherited components. Adding
`ecs_set(world, instance, Mana, { 20 })` does not change the base, and changing
the base does not rewrite components already owned by instances.

## Inheritance Checks

Use `ecs_is()` to test inheritance:

```c
if (ecs_is(world, instance, base)) {
    /* instance inherits from base */
}
```

`ecs_is()` checks whether the entity is directly linked to the target.

## Query Matching

Queries can match inherited components for read-only terms. A query with
`ecs_in(Health)` matches an instance that inherits `Health` from its base:

```c
ecs_query_id_t q = ecs_query(world, {
    .terms = { ecs_in(Health) },
});

ecs_iter_t it = ecs_query_iter(world, q);
while (ecs_iter_next(&it)) {
    Health *health = ecs_field(&it, 0);

    for (uint32_t i = 0; i < it.count; i++) {
        if (ecs_field_is_shared(&it, 0)) {
            /* health points to the base value, shared by the whole batch */
            printf("%d\n", health->value);
        } else {
            /* health points to the entity-owned component array */
            printf("%d\n", health[i].value);
        }
    }
}
```

When a field is inherited, `it.field_kinds[index]` is `EcsFieldShared`. The
pointer returned by `ecs_field()` points to the component stored on the base, not
to an array with one element per entity.

When a field is owned by the matched entities, `it.field_kinds[index]` is
`EcsFieldOwned`. The pointer is a normal component array indexed with `i`.

The inheritance rules for query fields are:

| Term | Inherited component behavior |
| --- | --- |
| `ecs_in(T)` | Matches owned or inherited `T`; inherited fields are `EcsFieldShared`. |
| `ecs_out(T)` and `ecs_inout(T)` | Match only entities that own `T` locally. |
| `ecs_in_optional(T)` | May return an inherited shared field when `T` exists on a base. |
| `ecs_inout_optional(T)` | Ignores inherited shared fields and returns `NULL` unless `T` is owned locally. |

## Writable Queries

Writable terms do not match shared inherited data. A query with
`ecs_inout(Health)` only matches entities that own `Health` locally:

```c
ecs_query_id_t q = ecs_query(world, {
    .terms = { ecs_inout(Health) },
});
```

This keeps writes unambiguous. If an instance only inherits `Health`, it is
skipped by this query. If the instance overrides `Health` with `ecs_set()`, it
matches and the query writes to the instance-owned value.

Optional writable terms also ignore inherited shared values:

```c
ecs_query_id_t q = ecs_query(world, {
    .terms = { ecs_inout_optional(Health) },
});
```

For an entity that only inherits `Health`, this field is `EcsFieldNone` and
`ecs_field()` returns `NULL`.

## Restrict A Query To A Base

Queries can be restricted to entities that inherit from a base with
`ecs_query_desc_t.is_a`:

```c
ecs_query_id_t q = ecs_query(world, {
    .is_a = base,
    .terms = { ecs_in(Position) },
});
```

This matches entities that inherit from `base` and also satisfy the component
terms. The base relationship is transitive: if `Knight` inherits from `Player`
and `Player` inherits from `Character`, a query with `.is_a = Character` matches
`Knight`.

## Common Pattern

Use inheritance for shared defaults, then override only the values that differ:

```c
ecs_entity_t enemy = ecs_new(world);
ecs_set(world, enemy, Health, { 100 });
ecs_set(world, enemy, Speed, { 2.5f });
ecs_add(world, enemy, Abstract);

ecs_entity_t boss = ecs_new(world);
ecs_is_a(world, boss, enemy);
ecs_set(world, boss, Health, { 500 });
```

`boss` inherits `Speed` from `enemy`, but owns its own `Health`.
