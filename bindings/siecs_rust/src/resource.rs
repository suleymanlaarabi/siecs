use crate::{raw, World};

pub trait Resource: Sized + 'static {
    unsafe fn id_raw(world: *mut raw::WorldRaw) -> raw::ResourceId;

    #[inline]
    fn id(world: &mut World) -> raw::ResourceId {
        unsafe { Self::id_raw(world.as_raw_mut()) }
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[repr(transparent)]
pub struct ResourceId(raw::ResourceId);

impl ResourceId {
    #[inline]
    pub const fn raw(self) -> raw::ResourceId {
        self.0
    }
}

impl From<raw::ResourceId> for ResourceId {
    #[inline]
    fn from(id: raw::ResourceId) -> Self {
        Self(id)
    }
}
