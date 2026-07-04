use core::ffi::c_char;
use core::ops::{Deref, DerefMut};
use core::ptr::NonNull;
use std::ffi::{CStr, CString};

use crate::{
    raw, ChildOf, Component, Disabled, Entity, Event, EventId, Name, Observer, ObserverId, OnAdd,
    OnRemove, OnSet, Query, RawEvent, Resource, System, SystemId, TypedEvent,
};

pub struct World {
    raw: NonNull<raw::WorldRaw>,
    system_names: Vec<CString>,
    observer_callbacks: Vec<BoxedObserverCallback>,
}

struct BoxedObserverCallback {
    ptr: usize,
    drop_fn: unsafe fn(usize),
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
#[repr(C)]
pub struct WorldFeatures {
    pub rest: bool,
    pub target_fps: u16,
}

pub struct DeferGuard<'world> {
    world: &'world mut World,
}

impl Drop for DeferGuard<'_> {
    #[inline]
    fn drop(&mut self) {
        self.world.defer_end();
    }
}

impl Deref for DeferGuard<'_> {
    type Target = World;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.world
    }
}

impl DerefMut for DeferGuard<'_> {
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.world
    }
}

impl World {
    #[inline]
    pub fn new() -> Self {
        let raw = unsafe { raw::ecs_init() };
        let raw = NonNull::new(raw).expect("ecs_init returned null");

        Self {
            raw,
            system_names: Vec::new(),
            observer_callbacks: Vec::new(),
        }
    }

    #[inline]
    pub fn with_features(features: WorldFeatures) -> Self {
        let features = raw::WorldFeatDesc {
            rest: features.rest,
            target_fps: features.target_fps,
        };
        let raw = unsafe { raw::ecs_init_w_features(&features) };
        let raw = NonNull::new(raw).expect("ecs_init_w_features returned null");

        Self {
            raw,
            system_names: Vec::new(),
            observer_callbacks: Vec::new(),
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
    pub fn is(&mut self, entity: Entity, target: Entity) -> bool {
        unsafe { raw::ecs_is(self.raw.as_ptr(), entity.id, target.id) }
    }

    #[inline]
    pub fn is_a(&mut self, entity: Entity, base: Entity) {
        unsafe {
            raw::ecs_is_a(self.raw.as_ptr(), entity.id, base.id);
        }
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
    pub fn observe_raw(&mut self, event: EventId) -> Observer<'_, RawEvent> {
        Observer::new(self, event)
    }

    #[inline]
    pub fn observer(&mut self, event: EventId) -> Observer<'_, RawEvent> {
        self.observe_raw(event)
    }

    #[inline]
    pub fn observe<E: Event>(&mut self) -> Observer<'_, E> {
        let event = E::id(self);
        Observer::new(self, event)
    }

    #[inline]
    pub fn on_add<T: Component>(&mut self) -> Observer<'_, OnAdd<T>> {
        self.observe::<OnAdd<T>>().require::<T>()
    }

    #[inline]
    pub fn on_remove<T: Component>(&mut self) -> Observer<'_, OnRemove<T>> {
        self.observe::<OnRemove<T>>().require::<T>()
    }

    #[inline]
    pub fn on_set<T: Component>(&mut self) -> Observer<'_, OnSet<T>> {
        self.observe::<OnSet<T>>().require::<T>()
    }

    #[inline]
    pub(crate) fn retain_system_name(&mut self, name: &str) -> *const c_char {
        let name = CString::new(name).expect("system name cannot contain NUL bytes");
        let ptr = name.as_ptr();
        self.system_names.push(name);
        ptr
    }

    #[inline]
    pub(crate) fn retain_observer_callback<F: 'static>(&mut self, ptr: *mut F) {
        unsafe fn drop_box<F>(ptr: usize) {
            drop(unsafe { Box::from_raw(ptr as *mut F) });
        }

        self.observer_callbacks.push(BoxedObserverCallback {
            ptr: ptr as usize,
            drop_fn: drop_box::<F>,
        });
    }

    #[inline]
    pub fn progress(&mut self) -> bool {
        unsafe { raw::ecs_progress(self.raw.as_ptr()) }
    }

    #[inline]
    pub fn defer_begin(&mut self) {
        unsafe { raw::ecs_defer_begin(self.raw.as_ptr()) }
    }

    #[inline]
    pub fn defer_end(&mut self) {
        unsafe { raw::ecs_defer_end(self.raw.as_ptr()) }
    }

    #[inline]
    pub fn is_deferred(&self) -> bool {
        unsafe { raw::ecs_is_deferred(self.raw.as_ptr()) }
    }

    #[inline]
    pub fn defer(&mut self) -> DeferGuard<'_> {
        self.defer_begin();
        DeferGuard { world: self }
    }

    #[inline]
    pub fn run_phase(&mut self, phase: raw::Phase) {
        unsafe {
            raw::ecs_run_phase(self.raw.as_ptr(), phase);
        }
    }

    #[inline]
    pub fn run_system(&mut self, system: SystemId) {
        unsafe {
            raw::ecs_run_system(self.raw.as_ptr(), system.raw());
        }
    }

    #[inline]
    pub fn enable_system(&mut self, system: SystemId) {
        unsafe {
            raw::ecs_system_enable(self.raw.as_ptr(), system.raw());
        }
    }

    #[inline]
    pub fn disable_system(&mut self, system: SystemId) {
        unsafe {
            raw::ecs_system_disable(self.raw.as_ptr(), system.raw());
        }
    }

    #[inline]
    pub fn enable_observer(&mut self, observer: ObserverId) {
        unsafe { raw::ecs_observer_enable(self.raw.as_ptr(), observer.raw()) }
    }

    #[inline]
    pub fn disable_observer(&mut self, observer: ObserverId) {
        unsafe { raw::ecs_observer_disable(self.raw.as_ptr(), observer.raw()) }
    }

    #[inline]
    pub fn with<C: Component, R: Component>(&mut self) {
        let component = C::id(self);
        let require = R::id(self);
        unsafe { raw::ecs_with(self.raw.as_ptr(), component, require) }
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
    pub fn has_owned<T: Component>(&mut self, entity: Entity) -> bool {
        let id = T::id(self);

        unsafe { raw::ecs_has_cid_owned(self.raw.as_ptr(), entity.id, id) }
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
        if !unsafe { raw::ecs_has_cid_owned(self.raw.as_ptr(), entity.id, id) } {
            return None;
        }

        let ptr = unsafe { raw::ecs_try_get_cid(self.raw.as_ptr(), entity.id, id) };

        unsafe { ptr.cast::<T>().as_mut() }
    }

    #[inline]
    pub fn disable(&mut self, entity: Entity) {
        self.add::<Disabled>(entity);
    }

    #[inline]
    pub fn enable(&mut self, entity: Entity) {
        self.remove::<Disabled>(entity);
    }

    #[inline]
    pub fn is_disabled(&mut self, entity: Entity) -> bool {
        self.has::<Disabled>(entity)
    }

    #[inline]
    pub fn is_enabled(&mut self, entity: Entity) -> bool {
        !self.is_disabled(entity)
    }

    #[inline]
    pub fn child_of(&mut self, entity: Entity, parent: Entity) {
        self.set(entity, ChildOf::new(parent));
    }

    #[inline]
    pub fn parent(&mut self, entity: Entity) -> Option<Entity> {
        self.get::<ChildOf>(entity)
            .map(|child_of| child_of.parent())
    }

    #[inline]
    pub fn set_name(&mut self, entity: Entity, name: &str) {
        let name = CString::new(name).expect("entity name cannot contain NUL bytes");
        self.set(
            entity,
            Name {
                value: name.into_raw(),
            },
        );
    }

    #[inline]
    pub fn name(&mut self, entity: Entity) -> Option<&str> {
        let name = self.get::<Name>(entity)?;
        if name.value.is_null() {
            return None;
        }

        unsafe { CStr::from_ptr(name.value).to_str().ok() }
    }

    #[inline]
    pub fn set_resource<T: Resource>(&mut self, value: T) {
        let id = T::id(self);
        unsafe { raw::ecs_set_resource_rid(self.raw.as_ptr(), id, (&value as *const T).cast()) }
    }

    #[inline]
    pub fn resource<T: Resource>(&mut self) -> &T {
        let id = T::id(self);
        unsafe { &*raw::ecs_resource_rid(self.raw.as_ptr(), id).cast::<T>() }
    }

    #[inline]
    pub fn resource_mut<T: Resource>(&mut self) -> &mut T {
        let id = T::id(self);
        unsafe { &mut *raw::ecs_resource_rid(self.raw.as_ptr(), id).cast::<T>() }
    }

    #[inline]
    pub fn try_resource<T: Resource>(&mut self) -> Option<&T> {
        let id = T::id(self);
        unsafe {
            raw::ecs_try_resource_rid(self.raw.as_ptr(), id)
                .cast::<T>()
                .as_ref()
        }
    }

    #[inline]
    pub fn try_resource_mut<T: Resource>(&mut self) -> Option<&mut T> {
        let id = T::id(self);
        unsafe {
            raw::ecs_try_resource_rid(self.raw.as_ptr(), id)
                .cast::<T>()
                .as_mut()
        }
    }

    #[inline]
    pub fn has_resource<T: Resource>(&mut self) -> bool {
        let id = T::id(self);
        unsafe { raw::ecs_has_resource_rid(self.raw.as_ptr(), id) }
    }

    #[inline]
    pub fn remove_resource<T: Resource>(&mut self) {
        let id = T::id(self);
        unsafe { raw::ecs_remove_resource_rid(self.raw.as_ptr(), id) }
    }

    #[inline]
    pub fn alloc_event(&mut self) -> EventId {
        unsafe { raw::ecs_event(self.raw.as_ptr()).into() }
    }

    #[inline]
    pub fn alloc_typed_event<P>(&mut self) -> TypedEvent<P> {
        TypedEvent::new(unsafe { raw::ecs_event(self.raw.as_ptr()) })
    }

    #[inline]
    pub fn event<E: Event>(&mut self) -> EventId {
        E::id(self)
    }

    #[inline]
    pub fn observe_typed<P: 'static>(
        &mut self,
        event: TypedEvent<P>,
    ) -> Observer<'_, TypedEvent<P>> {
        Observer::new(self, event.id())
    }

    #[inline]
    pub unsafe fn trigger_raw<T>(&mut self, entity: Entity, event: EventId, value: &T) {
        unsafe {
            raw::ecs_observer_trigger(
                self.raw.as_ptr(),
                entity.id(),
                event.raw(),
                (value as *const T).cast(),
            );
        }
    }

    #[inline]
    pub unsafe fn trigger_id<T>(&mut self, entity: Entity, event: EventId, value: &T) {
        unsafe { self.trigger_raw(entity, event, value) }
    }

    #[inline]
    pub fn trigger_typed<P>(&mut self, entity: Entity, event: TypedEvent<P>, value: &P) {
        unsafe { self.trigger_raw(entity, event.id(), value) };
    }

    #[inline]
    pub fn trigger<E>(&mut self, entity: Entity, value: E)
    where
        E: Event<Payload = E>,
    {
        self.trigger_ref::<E>(entity, &value);
    }

    #[inline]
    pub fn trigger_ref<E: Event>(&mut self, entity: Entity, value: &E::Payload) {
        let event = E::id(self);
        unsafe { self.trigger_raw(entity, event, value) };
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
        for callback in self.observer_callbacks.drain(..) {
            unsafe {
                (callback.drop_fn)(callback.ptr);
            }
        }
    }
}
