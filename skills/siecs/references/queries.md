# Query contracts

Required read, write, and read/write terms all require presence. Optional terms
return missing fields as null. Filter/exclusion terms affect matches without
producing fields. `Disabled` and `Abstract` are excluded unless explicitly
requested. A query without table criteria matches ordinary tables.

Field indices count returned component terms only, in declaration order.
Resources and relation parameters do not shift them. Honor the public descriptor
capacities and leave terminators in zero-terminated term lists.

After advancing an iterator, ordinary fields are contiguous arrays of `count`
rows. Shared inherited fields contain a single value: check field kind and do
not index them per row. Mutable terms cannot match shared inherited storage.
Optional mutable access can remain absent even when shared data exists.

Ancestor reads find the nearest effective value through an acyclic `ByTarget`
relation. Required and optional ancestor terms differ on missing values. They
read inherited storage; do not turn them into writable pointers.

Relation terms select required/optional/excluded edges, exact targets, or depth.
Target/depth ordering orders tables, not arbitrary component values or entities
within a table. Custom order callbacks must not mutate the world.

Retain persistent queries for recurring work; short-lived helpers allocate a
temporary query. Create queries during setup, not in parallel system callbacks.
Destroy owned queries before the world ends. Defer structural
mutation while iterating, and never retain field pointers between batches.
