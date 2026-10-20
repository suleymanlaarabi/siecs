# C++: custom column/batch callbacks

Normal component arguments make `each` visit rows. For a column algorithm that
needs one callback per matching batch, a public SystemParam can bind a builder
field without adding another component term:

```cpp
template <typename T> struct Column {
    uint16_t index = 0;
    T *values = nullptr;
    uint32_t count = 0;
    static Column init() noexcept { return {}; }
    void field(uint16_t value) noexcept { index = value; }
    void table(ecs_iter_t *it) noexcept {
        values = static_cast<T *>(ecs_field(it, index));
        count = it->count;
    }
};

ecs::system("MassBatches").with<Moving>().optional<Mass>()
    .each([](Column<Mass> mass) {
        if (!mass.values) return;
        for (uint32_t row = 0; row < mass.count; row++) {
            mass.values[row].value *= 0.99f;
        }
    });
```

`Column` has `field/table` hooks but no `row` hook: this callback runs once per
non-empty matching batch. Its first parameter consumes field zero, supplied by
`.optional<Mass>()`; no `query` hook adds a duplicate field. The descriptor still
declares optional Mass writes for scheduler analysis. Entities without Mass
produce NULL; this recipe deliberately accesses owned writable storage.

The same pattern works with `ecs::query().with<Moving>().optional<Mass>().each(...)`.
One raw returned field must correspond to each custom `field` parameter in order.
Do not combine the builder optional with `ecs::optional<Mass>`: that wrapper adds
another term. Adding ordinary component/entity/target or custom-row parameters
changes callback frequency to per row.

For shared/ancestor reads, prefer typed const references/optionals, or the
[C field-kind iteration](../c/queries.md#fields); a shared pointer is one value,
not an array of count values. Batch views are borrowed for that callback.
Custom columns in observers describe only the emitter's one-row synthetic view,
not a world query or a way to reorder event delivery.

[Parameter protocol](parameters.md#custom-systemparam-protocol) · [Raw builder fields](filters.md#raw-field-and-ordering-caveats)
