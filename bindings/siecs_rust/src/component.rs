use crate::{raw, World};

/// Rust component type registered in SIECS.
///
/// Implementations are expected to lazily register the component the first time
/// `id` is called, then register that stable id in each world that uses it.
pub trait Component: Sized + 'static {
    unsafe fn id_raw(world: *mut raw::WorldRaw) -> raw::ComponentId;

    #[inline]
    fn id(world: &mut World) -> raw::ComponentId {
        unsafe { Self::id_raw(world.as_raw_mut()) }
    }
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
    unsafe fn id_raw(_world: *mut raw::WorldRaw) -> raw::ComponentId {
        let id = ECS_ID_ABSTRACT;
        debug_assert_ne!(id, 0);
        id
    }
}
