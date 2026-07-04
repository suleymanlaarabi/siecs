use core::ffi::c_char;

use crate::{raw, Entity, World};

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
    #[link_name = "_ecs_id_ChildOf__"]
    static mut ECS_ID_CHILD_OF: raw::ComponentId;
    #[link_name = "_ecs_id_Name__"]
    static mut ECS_ID_NAME: raw::ComponentId;
    #[link_name = "_ecs_id_Disabled__"]
    static mut ECS_ID_DISABLED: raw::ComponentId;
    #[link_name = "_ecs_id_Abstract__"]
    static mut ECS_ID_ABSTRACT: raw::ComponentId;
}

macro_rules! builtin_component {
    ($ty:ty, $static_id:ident) => {
        impl $ty {
            #[inline]
            pub fn id(_world: &mut World) -> raw::ComponentId {
                let id = unsafe { $static_id };
                debug_assert_ne!(id, 0);
                id
            }
        }

        impl Component for $ty {
            #[inline]
            unsafe fn id_raw(_world: *mut raw::WorldRaw) -> raw::ComponentId {
                let id = $static_id;
                debug_assert_ne!(id, 0);
                id
            }
        }
    };
}

/// Builtin parent relation component.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(C)]
pub struct ChildOf {
    pub target: raw::EntityId,
}

impl ChildOf {
    #[inline]
    pub const fn new(parent: Entity) -> Self {
        Self {
            target: parent.id(),
        }
    }

    #[inline]
    pub const fn parent(self) -> Entity {
        Entity::from_raw(self.target)
    }
}

/// Builtin entity name component.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(C)]
pub struct Name {
    pub value: *mut c_char,
}

/// Builtin marker component excluded from queries by default.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
pub struct Disabled;

/// Builtin marker component required for inheritance bases.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
pub struct Abstract;

builtin_component!(ChildOf, ECS_ID_CHILD_OF);
builtin_component!(Name, ECS_ID_NAME);
builtin_component!(Disabled, ECS_ID_DISABLED);
builtin_component!(Abstract, ECS_ID_ABSTRACT);
