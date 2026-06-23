use crate::{raw, World};

/// Rust component type registered in SIECS.
///
/// Implementations are expected to lazily register the component the first time
/// `id` is called, then register that stable id in each world that uses it.
pub trait Component: Sized + 'static {
    fn id(world: &mut World) -> raw::ComponentId;
}
