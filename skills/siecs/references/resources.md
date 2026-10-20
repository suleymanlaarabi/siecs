# Resources

A resource is one world-level value in its own namespace, not a component on
every entity. Use small resources for explicit dependencies such as configuration,
time, or an external service handle. `DeltaTime` is the built-in frame resource.

Register before setting; reads requiring presence must follow initialization.
Presence/try-get APIs handle absence. Removal of absent storage is a no-op.
Do not infer presence from a nonzero id: registration and storage presence differ.
Resource set/removal is immediate, even in a defer scope. Removal invalidates
borrowed pointers and callback `res<T>` views; do not remove a resource while
an active callback still borrows it.

Resource terms declare scheduler access; they do not filter tables or consume
component field indices. Standalone resource-only queries can match ordinary
tables; resource-only systems execute once, not once per entity. Use read-only
access when possible and declare writes even when accessed through a helper.

Set copies or moves according to type operations. Replacement/removal hooks
receive borrowed values only for the callback duration. Resources with owned
data need correct lifecycle operations just like components. Present resources
are finalized in reverse first-registration order.

Do not store borrowed table pointers in resources. Keep stable entity handles
and reacquire their components when the system executes.
