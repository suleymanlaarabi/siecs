use core::ffi::c_char;
use core::ptr::NonNull;
use std::ffi::CString;

use crate::{raw, Component, Entity, Query, System};

#[derive(Clone)]
pub struct World {
    raw: NonNull<raw::WorldRaw>,
    system_names: Vec<CString>,
}

impl World {
    #[inline]
    pub fn new() -> Self {
        let raw = unsafe { raw::ecs_init() };
        let raw = NonNull::new(raw).expect("ecs_init returned null");

        Self {
            raw,
            system_names: Vec::new(),
        }
    }

    #[inline]
    pub fn as_raw(&self) -> *const raw::WorldRaw {
        self.raw.as_ptr()
    }

    #[inline]
    pub fn as_raw_mut(&mut self) -> *mut raw::WorldRaw {
        self.raw.as_ptr()
    }

    #[inline]
    pub fn entity(&mut self) -> Entity {
        let id = unsafe { raw::ecs_new(self.raw.as_ptr()) };

        Entity::from_raw(id)
    }

    #[inline]
    pub fn is_alive(&self, entity: Entity) -> bool {
        unsafe { raw::ecs_is_alive(self.raw.as_ptr(), entity.id) }
    }

    #[inline]
    pub fn kill(&mut self, entity: Entity) {
        unsafe {
            raw::ecs_kill(self.raw.as_ptr(), entity.id);
        }
    }

    #[inline]
    pub fn component_id<T: Component>(&mut self) -> raw::ComponentId {
        T::id(self)
    }

    #[inline]
    pub fn query(&mut self) -> Query<'_> {
        Query::new(self)
    }

    #[inline]
    pub fn system(&mut self, name: &str) -> System<'_> {
        System::new(self, name)
    }

    #[inline]
    pub(crate) fn retain_system_name(&mut self, name: &str) -> *const c_char {
        let name = CString::new(name).expect("system name cannot contain NUL bytes");
        let ptr = name.as_ptr();
        self.system_names.push(name);
        ptr
    }

    #[inline]
    pub fn progress(&mut self) -> bool {
        unsafe { raw::ecs_progress(self.raw.as_ptr()) }
    }

    #[inline]
    pub fn run_phase(&mut self, phase: raw::Phase) {
        unsafe {
            raw::ecs_run_phase(self.raw.as_ptr(), phase);
        }
    }

    #[inline]
    pub fn run_system(&mut self, system: raw::SystemId) {
        unsafe {
            raw::ecs_run_system(self.raw.as_ptr(), system);
        }
    }

    #[inline]
    pub fn enable_system(&mut self, system: raw::SystemId) {
        unsafe {
            raw::ecs_system_enable(self.raw.as_ptr(), system);
        }
    }

    #[inline]
    pub fn disable_system(&mut self, system: raw::SystemId) {
        unsafe {
            raw::ecs_system_disable(self.raw.as_ptr(), system);
        }
    }

    #[inline]
    pub fn add<T: Component>(&mut self, entity: Entity) {
        let id = T::id(self);

        unsafe {
            raw::ecs_add_cid(self.raw.as_ptr(), entity.id, id);
        }
    }

    #[inline]
    pub fn remove<T: Component>(&mut self, entity: Entity) {
        let id = T::id(self);

        unsafe {
            raw::ecs_remove_cid(self.raw.as_ptr(), entity.id, id);
        }
    }

    #[inline]
    pub fn has<T: Component>(&mut self, entity: Entity) -> bool {
        let id = T::id(self);

        unsafe { raw::ecs_has_cid(self.raw.as_ptr(), entity.id, id) }
    }

    #[inline]
    pub fn set<T: Component>(&mut self, entity: Entity, value: T) {
        let id = T::id(self);

        unsafe {
            raw::ecs_set_cid(
                self.raw.as_ptr(),
                entity.id,
                id,
                (&value as *const T).cast(),
            );
        }
    }

    #[inline]
    pub fn get<T: Component>(&mut self, entity: Entity) -> Option<&T> {
        let id = T::id(self);
        let ptr = unsafe { raw::ecs_try_get_cid(self.raw.as_ptr(), entity.id, id) };

        unsafe { ptr.cast::<T>().as_ref() }
    }

    #[inline]
    pub fn get_mut<T: Component>(&mut self, entity: Entity) -> Option<&mut T> {
        let id = T::id(self);
        let ptr = unsafe { raw::ecs_try_get_cid(self.raw.as_ptr(), entity.id, id) };

        unsafe { ptr.cast::<T>().as_mut() }
    }
}

impl Default for World {
    #[inline]
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for World {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            raw::ecs_fini(self.raw.as_ptr());
        }
    }
}
