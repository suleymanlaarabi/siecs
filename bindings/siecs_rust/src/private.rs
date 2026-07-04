use core::cell::UnsafeCell;

use crate::raw;

pub struct StaticComponentId(UnsafeCell<raw::ComponentId>);
pub struct StaticResourceId(UnsafeCell<raw::ResourceId>);
pub struct StaticEventId(UnsafeCell<raw::EventId>);
pub struct StaticModuleId(UnsafeCell<raw::ModuleId>);

unsafe impl Sync for StaticComponentId {}
unsafe impl Sync for StaticResourceId {}
unsafe impl Sync for StaticEventId {}
unsafe impl Sync for StaticModuleId {}

impl StaticComponentId {
    #[inline]
    pub const fn new() -> Self {
        Self(UnsafeCell::new(0))
    }

    #[inline]
    pub fn as_mut_ptr(&self) -> *mut raw::ComponentId {
        self.0.get()
    }
}

impl StaticResourceId {
    #[inline]
    pub const fn new() -> Self {
        Self(UnsafeCell::new(0))
    }

    #[inline]
    pub fn as_mut_ptr(&self) -> *mut raw::ResourceId {
        self.0.get()
    }
}

impl StaticEventId {
    #[inline]
    pub const fn new() -> Self {
        Self(UnsafeCell::new(raw::EventId::MAX))
    }

    #[inline]
    pub fn as_mut_ptr(&self) -> *mut raw::EventId {
        self.0.get()
    }
}

impl StaticModuleId {
    #[inline]
    pub const fn new() -> Self {
        Self(UnsafeCell::new(0))
    }

    #[inline]
    pub fn as_mut_ptr(&self) -> *mut raw::ModuleId {
        self.0.get()
    }
}
