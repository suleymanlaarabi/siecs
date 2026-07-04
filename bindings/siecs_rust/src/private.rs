use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::mem::size_of;
use core::ptr;
use std::sync::{Mutex, MutexGuard, OnceLock};

use crate::raw;

pub struct StaticComponentId(UnsafeCell<raw::ComponentId>);
pub struct StaticResourceId(UnsafeCell<raw::ResourceId>);
pub struct StaticEventId(UnsafeCell<raw::EventId>);
pub struct StaticModuleId(UnsafeCell<raw::ModuleId>);

unsafe impl Sync for StaticComponentId {}
unsafe impl Sync for StaticResourceId {}
unsafe impl Sync for StaticEventId {}
unsafe impl Sync for StaticModuleId {}

#[inline]
pub fn id_registration_lock() -> MutexGuard<'static, ()> {
    static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
    LOCK.get_or_init(|| Mutex::new(()))
        .lock()
        .expect("typed id registration lock poisoned")
}

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

unsafe extern "C" fn rust_dtor<T>(ptr: *mut c_void, count: u32) {
    if size_of::<T>() == 0 {
        return;
    }
    let values = ptr.cast::<T>();
    for i in 0..count as usize {
        unsafe { ptr::drop_in_place(values.add(i)) };
    }
}

unsafe extern "C" fn rust_move_ctor<T>(dst: *mut c_void, src: *mut c_void, count: u32) {
    if size_of::<T>() == 0 {
        return;
    }
    let out = dst.cast::<T>();
    let input = src.cast::<T>();
    for i in 0..count as usize {
        unsafe { out.add(i).write(input.add(i).read()) };
    }
}

unsafe extern "C" fn rust_move<T>(dst: *mut c_void, src: *mut c_void, count: u32) {
    if size_of::<T>() == 0 {
        return;
    }
    let out = dst.cast::<T>();
    let input = src.cast::<T>();
    for i in 0..count as usize {
        unsafe {
            ptr::drop_in_place(out.add(i));
            out.add(i).write(input.add(i).read());
        }
    }
}

#[inline]
pub fn type_ops<T>() -> raw::TypeOps {
    if size_of::<T>() == 0 {
        raw::TypeOps::default()
    } else {
        raw::TypeOps {
            ctor: None,
            dtor: Some(rust_dtor::<T>),
            copy_ctor: None,
            copy: None,
            move_ctor: Some(rust_move_ctor::<T>),
            move_: Some(rust_move::<T>),
        }
    }
}
