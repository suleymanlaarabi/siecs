use core::marker::PhantomData;
use core::mem::{needs_drop, size_of, MaybeUninit};

use crate::query::{append_term, validate_returned_fields};
use crate::{raw, Component, Entity, World, WorldRef};

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[repr(transparent)]
pub struct EventId(raw::EventId);

impl EventId {
    #[inline]
    pub const fn raw(self) -> raw::EventId {
        self.0
    }
}

impl From<raw::EventId> for EventId {
    #[inline]
    fn from(id: raw::EventId) -> Self {
        Self(id)
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[repr(transparent)]
pub struct ObserverId(raw::ObserverId);

impl ObserverId {
    #[inline]
    pub const fn raw(self) -> raw::ObserverId {
        self.0
    }
}

impl From<raw::ObserverId> for ObserverId {
    #[inline]
    fn from(id: raw::ObserverId) -> Self {
        Self(id)
    }
}

#[allow(non_upper_case_globals)]
pub const OnAdd: EventId = EventId(raw::ECS_ON_ADD);
#[allow(non_upper_case_globals)]
pub const OnRemove: EventId = EventId(raw::ECS_ON_REMOVE);
#[allow(non_upper_case_globals)]
pub const OnSet: EventId = EventId(raw::ECS_ON_SET);

#[derive(Clone, Copy)]
pub struct ObserverEvent<'a> {
    raw: *mut raw::ObserverEvent,
    _marker: PhantomData<&'a raw::ObserverEvent>,
}

impl<'a> ObserverEvent<'a> {
    #[inline]
    pub(crate) unsafe fn from_raw(raw: *mut raw::ObserverEvent) -> Self {
        Self {
            raw,
            _marker: PhantomData,
        }
    }

    #[inline]
    pub fn world(self) -> WorldRef<'a> {
        unsafe { WorldRef::from_raw((*self.raw).world) }
    }

    #[inline]
    pub fn entity(self) -> Entity {
        unsafe { Entity::from_raw((*self.raw).entity) }
    }

    #[inline]
    pub fn event(self) -> EventId {
        unsafe { (*self.raw).event.into() }
    }

    #[inline]
    pub fn data<T>(self) -> Option<&'a T> {
        unsafe { (*self.raw).trigger_data.cast::<T>().as_ref() }
    }
}

pub struct Observer<'world> {
    world: &'world mut World,
    desc: raw::ObserverDesc,
    term_index: u16,
}

impl<'world> Observer<'world> {
    #[inline]
    pub(crate) fn new(world: &'world mut World, event: EventId) -> Self {
        Self {
            world,
            desc: raw::ObserverDesc {
                on: event.raw(),
                ..raw::ObserverDesc::default()
            },
            term_index: 0,
        }
    }

    #[inline]
    pub fn require<T: Component>(mut self) -> Self {
        self.append_component::<T>(raw::TermAccess::Filter);
        self
    }

    #[inline]
    pub fn exclude<T: Component>(mut self) -> Self {
        self.append_component::<T>(raw::TermAccess::Not);
        self
    }

    #[inline]
    pub fn each<F, Marker>(mut self, func: F) -> ObserverId
    where
        F: ObserverEach<Marker> + 'static,
    {
        assert!(
            size_of::<F>() == 0 && !needs_drop::<F>(),
            "observer callbacks must be stateless"
        );
        let _ = func;

        F::append_terms(self.world, &mut self.desc.query, &mut self.term_index);
        validate_returned_fields(&self.desc.query);

        self.desc.callback = Some(observer_callback::<F, Marker>);

        unsafe { raw::ecs_observer_init(self.world.as_raw_mut(), &self.desc).into() }
    }

    #[inline]
    fn append_component<T: Component>(&mut self, access: raw::TermAccess) {
        let id = T::id(self.world);
        append_term(&mut self.desc.query, &mut self.term_index, id, access);
    }
}

#[doc(hidden)]
pub trait ObserverEach<Marker> {
    fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16);
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent);
}

unsafe extern "C" fn observer_callback<F, Marker>(event: *mut raw::ObserverEvent)
where
    F: ObserverEach<Marker> + 'static,
{
    debug_assert!(!event.is_null());
    debug_assert_eq!(size_of::<F>(), 0);
    debug_assert!(!needs_drop::<F>());

    let mut func = MaybeUninit::<F>::zeroed().assume_init();
    F::run(&mut func, event);
}

#[doc(hidden)]
pub struct ObserverRef<T>(PhantomData<T>);

#[doc(hidden)]
pub struct ObserverMut<T>(PhantomData<T>);

macro_rules! observer_marker_ty {
    (ref $component:ident) => {
        ObserverRef<$component>
    };
    (mut $component:ident) => {
        ObserverMut<$component>
    };
}

macro_rules! observer_marker_arg {
    (ref $component:ident) => {
        &$component
    };
    (mut $component:ident) => {
        &mut $component
    };
}

macro_rules! observer_access {
    (ref) => {
        raw::TermAccess::In
    };
    (mut) => {
        raw::TermAccess::InOut
    };
}

macro_rules! observer_arg {
    (ref $event:ident $component:ident) => {{
        let id = $component::id_raw((*$event).world);
        &*raw::ecs_get_cid((*$event).world, (*$event).entity, id).cast::<$component>()
    }};
    (mut $event:ident $component:ident) => {{
        let id = $component::id_raw((*$event).world);
        &mut *raw::ecs_get_cid((*$event).world, (*$event).entity, id).cast::<$component>()
    }};
}

macro_rules! impl_observer_each {
    ($(($component:ident, $kind:ident)),+ $(,)?) => {
        impl<F, $($component),+> ObserverEach<fn($(observer_marker_ty!($kind $component)),+)> for F
        where
            F: FnMut($(observer_marker_arg!($kind $component)),+),
            $($component: Component),+
        {
            #[inline]
            fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16) {
                $(
                    let id = $component::id(world);
                    append_term(desc, term_index, id, observer_access!($kind));
                )+
            }

            #[inline]
            unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
                self($(observer_arg!($kind event $component)),+);
            }
        }

        impl<F, $($component),+> ObserverEach<fn(ObserverEvent<'_>, $(observer_marker_ty!($kind $component)),+)> for F
        where
            F: FnMut(ObserverEvent<'_>, $(observer_marker_arg!($kind $component)),+),
            $($component: Component),+
        {
            #[inline]
            fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16) {
                $(
                    let id = $component::id(world);
                    append_term(desc, term_index, id, observer_access!($kind));
                )+
            }

            #[inline]
            unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
                self(ObserverEvent::from_raw(event), $(observer_arg!($kind event $component)),+);
            }
        }
    };
}

macro_rules! impl_observer_each_perms {
    ($($component:ident),+ $(,)?) => {
        impl_observer_each_perms_inner!(() ($($component),+));
    };
}

macro_rules! impl_observer_each_perms_inner {
    (($($acc:tt)*) ()) => {
        impl_observer_each!($($acc)*);
    };
    (($($acc:tt)*) ($component:ident)) => {
        impl_observer_each_perms_inner!(($($acc)* ($component, ref),) ());
        impl_observer_each_perms_inner!(($($acc)* ($component, mut),) ());
    };
    (($($acc:tt)*) ($component:ident, $($rest:tt)+)) => {
        impl_observer_each_perms_inner!(($($acc)* ($component, ref),) ($($rest)+));
        impl_observer_each_perms_inner!(($($acc)* ($component, mut),) ($($rest)+));
    };
}

impl_observer_each_perms!(A);
impl_observer_each_perms!(A, B);
impl_observer_each_perms!(A, B, C);
impl_observer_each_perms!(A, B, C, D);
