# Entity identity and naming

An entity handle contains index and generation; a recycled index is a different
entity. Preserve handles, not component addresses. Liveness checks apply to
handles from the active world, not arbitrary integers or another world.

Fresh creation differs from find-or-create by path. Names are case-sensitive,
paths use `::`, and parent-relative lookup does not search ancestors. Named
creation can create missing parents. Anonymous path segments use
`#index:generation`; resolve those rather than inventing name parsing.

Renaming or reparenting onto an existing sibling name deletes the previous
entity and its owned subtree. Invalid names/ancestor collisions fail without
mutation. A successful deferred rename means queued; final validation happens
at flush. Find-or-create sees pending names in its execution context; ordinary
lookup sees committed state.

`Disabled` and `Abstract` are structural tags excluded by ordinary queries.
Explicitly request either tag when querying those entities. Toggling tags can
migrate storage. Use a component value for state that changes frequently and
does not drive repeated query selection.

`Name` stores a copied name. C absolute-path strings are caller-owned and use
`free()`; C++ returns `std::string`. Diagnostic anonymous names are not a
substitute for application identity. Use [relations](relations.md) for ownership.
