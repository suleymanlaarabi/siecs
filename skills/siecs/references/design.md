# Design with SIECS

Start from the data and transformations a feature needs. Identify independent
capabilities, the systems consuming them, their read/write sets, and ownership.
Represent those contracts in the ECS before choosing callbacks and queries.

For example, movement, damage, and lifetime are reusable capabilities:

- `Position` and `Velocity` feed movement; entities without `Velocity` stay still.
- `Health` and pending damage feed damage resolution; immunity can be a tag when
  it affects repeated query selection.
- A lifetime component feeds expiration; an ownership relation can cascade
  deletion without duplicating cleanup in each system.

Compose these on an entity rather than creating separate player, projectile,
and enemy classes with duplicated movement and cleanup. Group related
registrations in modules; do not use modules as containers owning all entities.

Split components when systems repeatedly access independent subsets. Keep
fields together when they are read/written together. Excessive fragmentation
also costs archetypes and migrations: measure the real workload.

Use several focused queries when one query would become a branch-heavy set of
optional fields. Retain persistent queries for repeated work. Use explicit
resource dependencies instead of a giant context object or hidden globals.

Use observers for reactions to lifecycle changes or explicit events. Use
systems for recurring bulk work. Keep expensive algorithms in reusable pure
functions called by those systems.

Before finishing a design, identify who owns each entity/resource, which
structural operations occur, when commands flush, and which system dependency
makes later readers observe earlier writes. Avoid retaining pointers across
those boundaries. Optimize based on access patterns and measurements.
