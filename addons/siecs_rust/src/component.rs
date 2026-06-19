use crate::{raw, World};

/// Rust component type registered in SIECS.
///
/// Implementations are expected to lazily register the component the first time
/// `id` is called, then return the cached id. The crate currently follows the C
/// and C++ addon constraint: one active world, no multi-world id cache.
pub trait Component: Sized + 'static {
    fn id(world: &mut World) -> raw::ComponentId;
}
