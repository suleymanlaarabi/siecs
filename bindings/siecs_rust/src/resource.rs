use core::ops::{Deref, DerefMut};

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

#[derive(Clone, Copy)]
#[repr(transparent)]
pub struct Res<'a, T: Resource> {
    value: &'a T,
}

impl<'a, T: Resource> Res<'a, T> {
    #[inline]
    pub(crate) const fn new(value: &'a T) -> Self {
        Self { value }
    }

    #[inline]
    pub const fn get(self) -> &'a T {
        self.value
    }
}

impl<T: Resource> Deref for Res<'_, T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.value
    }
}

#[repr(transparent)]
pub struct ResMut<'a, T: Resource> {
    value: &'a mut T,
}

impl<'a, T: Resource> ResMut<'a, T> {
    #[inline]
    pub(crate) fn new(value: &'a mut T) -> Self {
        Self { value }
    }

    #[inline]
    pub fn get(self) -> &'a mut T {
        self.value
    }
}

impl<T: Resource> Deref for ResMut<'_, T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.value
    }
}

impl<T: Resource> DerefMut for ResMut<'_, T> {
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.value
    }
}
