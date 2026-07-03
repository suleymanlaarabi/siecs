use crate::{raw, World};

/// Rust component type registered in SIECS.
///
/// Implementations are expected to lazily register the component the first time
/// `id` is called, then register that stable id in each world that uses it.
pub trait Component: Sized + 'static {
    fn id(world: &mut World) -> raw::ComponentId;
}

extern "C" {
    #[link_name = "_ecs_id_Abstract__"]
    static mut ECS_ID_ABSTRACT: raw::ComponentId;
}

/// Builtin marker component required for inheritance bases.
pub struct Abstract;

impl Abstract {
    #[inline]
    pub fn id(_world: &mut World) -> raw::ComponentId {
        let id = unsafe { ECS_ID_ABSTRACT };
        debug_assert_ne!(id, 0);
        id
    }
}

impl Component for Abstract {
    #[inline]
    fn id(world: &mut World) -> raw::ComponentId {
        Self::id(world)
    }
}
