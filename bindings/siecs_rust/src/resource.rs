use core::marker::PhantomData;
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
pub struct Res<T: Resource> {
    value: *const T,
    _marker: PhantomData<T>,
}

impl<T: Resource> Res<T> {
    #[inline]
    pub(crate) const fn new(value: &T) -> Self {
        Self {
            value,
            _marker: PhantomData,
        }
    }

    #[inline]
    pub fn get(&self) -> &T {
        unsafe { &*self.value }
    }
}

impl<T: Resource> Deref for Res<T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.get()
    }
}

#[derive(Clone, Copy)]
pub struct ResMut<T: Resource> {
    value: *mut T,
    _marker: PhantomData<T>,
}

impl<T: Resource> ResMut<T> {
    #[inline]
    pub(crate) fn new(value: &mut T) -> Self {
        Self {
            value,
            _marker: PhantomData,
        }
    }

    #[inline]
    pub fn get(&mut self) -> &mut T {
        unsafe { &mut *self.value }
    }
}

impl<T: Resource> Deref for ResMut<T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &Self::Target {
        unsafe { &*self.value }
    }
}

impl<T: Resource> DerefMut for ResMut<T> {
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.get()
    }
}
