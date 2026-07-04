use core::marker::PhantomData;
use std::ffi::{CStr, CString};

use crate::{raw, ChildOf, Component, Disabled, Entity, Event, EventId, Name, Resource};

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
    pub fn disable(&self, entity: Entity) {
        self.add::<Disabled>(entity);
    }

    #[inline]
    pub fn enable(&self, entity: Entity) {
        self.remove::<Disabled>(entity);
    }

    #[inline]
    pub fn is_disabled(&self, entity: Entity) -> bool {
        self.has::<Disabled>(entity)
    }

    #[inline]
    pub fn is_enabled(&self, entity: Entity) -> bool {
        !self.is_disabled(entity)
    }

    #[inline]
    pub fn child_of(&self, entity: Entity, parent: Entity) {
        self.set(entity, ChildOf::new(parent));
    }

    #[inline]
    pub fn parent(&self, entity: Entity) -> Option<Entity> {
        self.get::<ChildOf>(entity)
            .map(|child_of| child_of.parent())
    }

    #[inline]
    pub fn set_name(&self, entity: Entity, name: &str) {
        let name = CString::new(name).expect("entity name cannot contain NUL bytes");
        self.set(
            entity,
            Name {
                value: name.into_raw(),
            },
        );
    }

    #[inline]
    pub fn name(&self, entity: Entity) -> Option<&'world str> {
        let name = self.get::<Name>(entity)?;
        if name.value.is_null() {
            return None;
        }

        unsafe { CStr::from_ptr(name.value).to_str().ok() }
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

    #[inline]
    pub fn event<E: Event>(&self) -> EventId {
        unsafe { E::id_raw(self.raw).into() }
    }

    #[inline]
    pub unsafe fn trigger_raw<T>(&self, entity: Entity, event: EventId, value: &T) {
        unsafe {
            raw::ecs_observer_trigger(
                self.raw,
                entity.id(),
                event.raw(),
                (value as *const T).cast(),
            );
        }
    }

    #[inline]
    pub fn trigger<E>(&self, entity: Entity, value: E)
    where
        E: Event<Payload = E>,
    {
        self.trigger_ref::<E>(entity, &value);
    }

    #[inline]
    pub fn trigger_ref<E: Event>(&self, entity: Entity, value: &E::Payload) {
        unsafe { self.trigger_raw(entity, self.event::<E>(), value) }
    }
}
