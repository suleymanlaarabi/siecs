use core::ptr::NonNull;

use crate::{raw, Component, Entity};

#[derive(Clone)]
pub struct World {
    raw: NonNull<raw::WorldRaw>,
}

impl World {
    #[inline]
    pub fn new() -> Self {
        let raw = unsafe { raw::ecs_init() };
        let raw = NonNull::new(raw).expect("ecs_init returned null");

        Self { raw }
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
        unsafe { raw::ecs_is_alive(self.raw.as_ptr(), entity.id) != 0 }
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
