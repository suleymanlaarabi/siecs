use core::marker::PhantomData;

use crate::{raw, Component, Entity, Resource};

#[derive(Clone, Copy)]
pub struct WorldRef<'world> {
    raw: *mut raw::WorldRaw,
    _marker: PhantomData<&'world mut raw::WorldRaw>,
}

impl<'world> WorldRef<'world> {
    #[inline]
    pub(crate) unsafe fn from_raw(raw: *mut raw::WorldRaw) -> Self {
        Self {
            raw,
            _marker: PhantomData,
        }
    }

    #[inline]
    pub fn as_raw(&self) -> *const raw::WorldRaw {
        self.raw
    }

    #[inline]
    pub fn as_raw_mut(&self) -> *mut raw::WorldRaw {
        self.raw
    }

    #[inline]
    pub fn entity(&self) -> Entity {
        Entity::from_raw(unsafe { raw::ecs_new(self.raw) })
    }

    #[inline]
    pub fn is_alive(&self, entity: Entity) -> bool {
        unsafe { raw::ecs_is_alive(self.raw, entity.id()) }
    }

    #[inline]
    pub fn is(&self, entity: Entity, target: Entity) -> bool {
        unsafe { raw::ecs_is(self.raw, entity.id(), target.id()) }
    }

    #[inline]
    pub fn is_a(&self, entity: Entity, base: Entity) {
        unsafe { raw::ecs_is_a(self.raw, entity.id(), base.id()) }
    }

    #[inline]
    pub fn kill(&self, entity: Entity) {
        unsafe { raw::ecs_kill(self.raw, entity.id()) }
    }

    #[inline]
    pub fn add<T: Component>(&self, entity: Entity) {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_add_cid(self.raw, entity.id(), id) }
    }

    #[inline]
    pub fn remove<T: Component>(&self, entity: Entity) {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_remove_cid(self.raw, entity.id(), id) }
    }

    #[inline]
    pub fn has<T: Component>(&self, entity: Entity) -> bool {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_has_cid(self.raw, entity.id(), id) }
    }

    #[inline]
    pub fn has_owned<T: Component>(&self, entity: Entity) -> bool {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_has_cid_owned(self.raw, entity.id(), id) }
    }

    #[inline]
    pub fn set<T: Component>(&self, entity: Entity, value: T) {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe {
            raw::ecs_set_cid(self.raw, entity.id(), id, (&value as *const T).cast());
        }
    }

    #[inline]
    pub fn get<T: Component>(&self, entity: Entity) -> Option<&'world T> {
        let id = unsafe { T::id_raw(self.raw) };
        let ptr = unsafe { raw::ecs_try_get_cid(self.raw, entity.id(), id) };
        unsafe { ptr.cast::<T>().as_ref() }
    }

    #[inline]
    pub fn get_mut<T: Component>(&self, entity: Entity) -> Option<&'world mut T> {
        let id = unsafe { T::id_raw(self.raw) };
        if !unsafe { raw::ecs_has_cid_owned(self.raw, entity.id(), id) } {
            return None;
        }

        let ptr = unsafe { raw::ecs_try_get_cid(self.raw, entity.id(), id) };
        unsafe { ptr.cast::<T>().as_mut() }
    }

    #[inline]
    pub fn set_resource<T: Resource>(&self, value: T) {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_set_resource_rid(self.raw, id, (&value as *const T).cast()) }
    }

    #[inline]
    pub fn resource<T: Resource>(&self) -> &'world T {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { &*raw::ecs_resource_rid(self.raw, id).cast::<T>() }
    }

    #[inline]
    pub fn resource_mut<T: Resource>(&self) -> &'world mut T {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { &mut *raw::ecs_resource_rid(self.raw, id).cast::<T>() }
    }

    #[inline]
    pub fn try_resource<T: Resource>(&self) -> Option<&'world T> {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_try_resource_rid(self.raw, id).cast::<T>().as_ref() }
    }

    #[inline]
    pub fn try_resource_mut<T: Resource>(&self) -> Option<&'world mut T> {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_try_resource_rid(self.raw, id).cast::<T>().as_mut() }
    }

    #[inline]
    pub fn has_resource<T: Resource>(&self) -> bool {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_has_resource_rid(self.raw, id) }
    }

    #[inline]
    pub fn remove_resource<T: Resource>(&self) {
        let id = unsafe { T::id_raw(self.raw) };
        unsafe { raw::ecs_remove_resource_rid(self.raw, id) }
    }
}
