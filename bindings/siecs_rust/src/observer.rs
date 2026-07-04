use core::marker::PhantomData;
use core::mem::{needs_drop, size_of, MaybeUninit};

use crate::query::{
    append_term, resource_mut, resource_ref, validate_returned_fields, ParamError, ResourceAccess,
};
use crate::{raw, Component, Entity, Res, ResMut, Resource, World, WorldRef};

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[repr(transparent)]
pub struct EventId(raw::EventId);

impl EventId {
    #[inline]
    pub const fn raw(self) -> raw::EventId {
        self.0
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[repr(transparent)]
pub struct TypedEvent<P> {
    id: raw::EventId,
    _payload: PhantomData<fn() -> P>,
}

impl<P> TypedEvent<P> {
    #[inline]
    pub(crate) const fn new(id: raw::EventId) -> Self {
        Self {
            id,
            _payload: PhantomData,
        }
    }

    #[inline]
    pub const fn raw(self) -> raw::EventId {
        self.id
    }

    #[inline]
    pub const fn id(self) -> EventId {
        EventId(self.id)
    }
}

impl<P: 'static> Event for TypedEvent<P> {
    type Payload = P;

    #[inline]
    unsafe fn id_raw(_world: *mut raw::WorldRaw) -> raw::EventId {
        panic!("typed event handles must be passed with World::observe_typed")
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

/// Typed SIECS event.
///
/// Safe observers can only receive this event's associated payload type.
///
/// ```compile_fail
/// use siecs::{Component, Event, World};
///
/// #[derive(Component)]
/// struct Target;
/// #[derive(Event)]
/// struct Damage { amount: i32 }
/// #[derive(Event)]
/// struct Heal { amount: i32 }
///
/// fn wrong_payload(data: &Heal, _target: &Target) {
///     let _ = data.amount;
/// }
///
/// let mut world = World::new();
/// world.observe::<Damage>().each(wrong_payload);
/// ```
///
/// ```compile_fail
/// use siecs::{Event, World};
///
/// #[derive(Event)]
/// struct Damage { amount: i32 }
/// #[derive(Event)]
/// struct Heal { amount: i32 }
///
/// let mut world = World::new();
/// let entity = world.entity();
/// world.trigger_ref::<Damage>(entity, &Heal { amount: 10 });
/// ```
pub trait Event: Sized + 'static {
    type Payload: Sized + 'static;

    unsafe fn id_raw(world: *mut raw::WorldRaw) -> raw::EventId;

    #[inline]
    unsafe fn payload_raw<'a>(event: *mut raw::ObserverEvent) -> &'a Self::Payload {
        let ptr = (*event).trigger_data.cast::<Self::Payload>();
        assert!(!ptr.is_null(), "event trigger data is null");
        &*ptr
    }

    #[inline]
    fn id(world: &mut World) -> EventId {
        unsafe { Self::id_raw(world.as_raw_mut()).into() }
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
pub struct OnAdd<T: Component>(PhantomData<T>);
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
pub struct OnRemove<T: Component>(PhantomData<T>);
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
pub struct OnSet<T: Component>(PhantomData<T>);

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
pub struct RawEvent;

macro_rules! builtin_event {
    ($ty:ident, $id:expr) => {
        impl<T: Component> Event for $ty<T> {
            type Payload = T;

            #[inline]
            unsafe fn id_raw(_world: *mut raw::WorldRaw) -> raw::EventId {
                $id
            }

            #[inline]
            unsafe fn payload_raw<'a>(event: *mut raw::ObserverEvent) -> &'a Self::Payload {
                let data = (*event).trigger_data.cast::<T>();
                if let Some(value) = data.as_ref() {
                    return value;
                }
                if size_of::<T>() == 0 {
                    &*core::ptr::NonNull::<T>::dangling().as_ptr()
                } else {
                    let id = T::id_raw((*event).world);
                    &*raw::ecs_get_cid((*event).world, (*event).entity, id).cast::<T>()
                }
            }
        }
    };
}

builtin_event!(OnAdd, raw::ECS_ON_ADD);
builtin_event!(OnRemove, raw::ECS_ON_REMOVE);
builtin_event!(OnSet, raw::ECS_ON_SET);

impl Event for RawEvent {
    type Payload = ();

    #[inline]
    unsafe fn id_raw(_world: *mut raw::WorldRaw) -> raw::EventId {
        0
    }
}

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
    pub unsafe fn data_unchecked<T>(self) -> Option<&'a T> {
        unsafe { (*self.raw).trigger_data.cast::<T>().as_ref() }
    }
}

pub struct Observer<'world, E: Event> {
    world: &'world mut World,
    desc: raw::ObserverDesc,
    term_index: u16,
    _event: PhantomData<E>,
}

impl<'world, E: Event> Observer<'world, E> {
    #[inline]
    pub(crate) fn new(world: &'world mut World, event: EventId) -> Self {
        Self {
            world,
            desc: raw::ObserverDesc {
                on: event.raw(),
                ..raw::ObserverDesc::default()
            },
            term_index: 0,
            _event: PhantomData,
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
    pub fn each<F, Marker>(self, func: F) -> ObserverId
    where
        F: ObserverEach<E, Marker> + 'static,
    {
        self.try_each(func).unwrap_or_else(|err| panic!("{err}"))
    }

    #[inline]
    pub fn try_each<F, Marker>(mut self, func: F) -> Result<ObserverId, ParamError>
    where
        F: ObserverEach<E, Marker> + 'static,
    {
        assert!(
            size_of::<F>() == 0 && !needs_drop::<F>(),
            "observer callbacks must be stateless"
        );
        let _ = func;

        F::append_terms(self.world, &mut self.desc.query, &mut self.term_index)?;
        let mut resource_access = ResourceAccess::default();
        F::validate_resources(self.world, &mut resource_access)?;
        validate_returned_fields(&self.desc.query)?;

        self.desc.callback = Some(observer_callback::<E, F, Marker>);

        Ok(unsafe { raw::ecs_observer_init(self.world.as_raw_mut(), &self.desc).into() })
    }

    #[inline]
    pub fn each_boxed<F, Marker>(self, func: F) -> ObserverId
    where
        F: ObserverEach<E, Marker> + 'static,
    {
        self.try_each_boxed(func)
            .unwrap_or_else(|err| panic!("{err}"))
    }

    #[inline]
    pub fn try_each_boxed<F, Marker>(mut self, func: F) -> Result<ObserverId, ParamError>
    where
        F: ObserverEach<E, Marker> + 'static,
    {
        F::append_terms(self.world, &mut self.desc.query, &mut self.term_index)?;
        let mut resource_access = ResourceAccess::default();
        F::validate_resources(self.world, &mut resource_access)?;
        validate_returned_fields(&self.desc.query)?;

        let ptr = Box::into_raw(Box::new(func));
        self.desc.user_data = ptr as usize;
        self.desc.callback = Some(observer_boxed_callback::<E, F, Marker>);

        let id = unsafe { raw::ecs_observer_init(self.world.as_raw_mut(), &self.desc).into() };
        self.world.retain_observer_callback(ptr);
        Ok(id)
    }

    #[inline]
    fn append_component<T: Component>(&mut self, access: raw::TermAccess) {
        let id = T::id(self.world);
        append_term(&mut self.desc.query, &mut self.term_index, id, access)
            .expect("too many query terms");
    }
}

#[doc(hidden)]
pub trait ObserverEach<E: Event, Marker> {
    fn append_terms(
        world: &mut World,
        desc: &mut raw::QueryDesc,
        term_index: &mut u16,
    ) -> Result<(), ParamError>;
    fn validate_resources(
        _world: &mut World,
        _access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        Ok(())
    }
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent);
}

unsafe extern "C" fn observer_callback<E, F, Marker>(event: *mut raw::ObserverEvent)
where
    E: Event,
    F: ObserverEach<E, Marker> + 'static,
{
    debug_assert!(!event.is_null());
    debug_assert_eq!(size_of::<F>(), 0);
    debug_assert!(!needs_drop::<F>());

    let mut func = MaybeUninit::<F>::zeroed().assume_init();
    F::run(&mut func, event);
}

unsafe extern "C" fn observer_boxed_callback<E, F, Marker>(event: *mut raw::ObserverEvent)
where
    E: Event,
    F: ObserverEach<E, Marker> + 'static,
{
    debug_assert!(!event.is_null());

    let func = (*event).user_data as *mut F;
    debug_assert!(!func.is_null());
    F::run(&mut *func, event);
}

#[doc(hidden)]
pub struct ObserverPayload;

#[doc(hidden)]
pub struct ObserverRef<T>(PhantomData<T>);

#[doc(hidden)]
pub struct ObserverMut<T>(PhantomData<T>);

#[doc(hidden)]
pub struct ObserverOptRef<T>(PhantomData<T>);

#[doc(hidden)]
pub struct ObserverOptMut<T>(PhantomData<T>);

#[doc(hidden)]
pub struct ObserverRes<T>(PhantomData<T>);

#[doc(hidden)]
pub struct ObserverResMut<T>(PhantomData<T>);

#[doc(hidden)]
pub struct ObserverOptRes<T>(PhantomData<T>);

#[doc(hidden)]
pub struct ObserverOptResMut<T>(PhantomData<T>);

macro_rules! observer_marker_ty {
    (ref $component:ident) => {
        ObserverRef<$component>
    };
    (mut $component:ident) => {
        ObserverMut<$component>
    };
    (opt_ref $component:ident) => {
        ObserverOptRef<$component>
    };
    (opt_mut $component:ident) => {
        ObserverOptMut<$component>
    };
}

macro_rules! observer_marker_arg {
    (ref $component:ident) => {
        &$component
    };
    (mut $component:ident) => {
        &mut $component
    };
    (opt_ref $component:ident) => {
        Option<&$component>
    };
    (opt_mut $component:ident) => {
        Option<&mut $component>
    };
}

macro_rules! observer_access {
    (ref) => {
        raw::TermAccess::In
    };
    (mut) => {
        raw::TermAccess::InOut
    };
    (opt_ref) => {
        raw::TermAccess::InOptional
    };
    (opt_mut) => {
        raw::TermAccess::InOutOptional
    };
}

macro_rules! observer_arg {
    (ref $event:ident $component:ident) => {{
        if size_of::<$component>() == 0 {
            &*core::ptr::NonNull::<$component>::dangling().as_ptr()
        } else {
            let id = $component::id_raw((*$event).world);
            &*raw::ecs_get_cid((*$event).world, (*$event).entity, id).cast::<$component>()
        }
    }};
    (mut $event:ident $component:ident) => {{
        if size_of::<$component>() == 0 {
            &mut *core::ptr::NonNull::<$component>::dangling().as_ptr()
        } else {
            let id = $component::id_raw((*$event).world);
            &mut *raw::ecs_get_cid((*$event).world, (*$event).entity, id).cast::<$component>()
        }
    }};
    (opt_ref $event:ident $component:ident) => {{
        if size_of::<$component>() == 0 {
            Some(&*core::ptr::NonNull::<$component>::dangling().as_ptr())
        } else {
            let id = $component::id_raw((*$event).world);
            if raw::ecs_has_cid((*$event).world, (*$event).entity, id) {
                raw::ecs_try_get_cid((*$event).world, (*$event).entity, id)
                    .cast::<$component>()
                    .as_ref()
            } else {
                None
            }
        }
    }};
    (opt_mut $event:ident $component:ident) => {{
        if size_of::<$component>() == 0 {
            Some(&mut *core::ptr::NonNull::<$component>::dangling().as_ptr())
        } else {
            let id = $component::id_raw((*$event).world);
            if raw::ecs_has_cid_owned((*$event).world, (*$event).entity, id) {
                raw::ecs_try_get_cid((*$event).world, (*$event).entity, id)
                    .cast::<$component>()
                    .as_mut()
            } else {
                None
            }
        }
    }};
}

macro_rules! observer_data_arg {
    ($event:ident $event_ty:ident) => {{
        <$event_ty as Event>::payload_raw($event)
    }};
}

macro_rules! observer_res_arg {
    (ref $event:ident $resource:ident) => {{
        resource_ref::<$resource>((*$event).world)
    }};
    (mut $event:ident $resource:ident) => {{
        resource_mut::<$resource>((*$event).world)
    }};
    (opt_ref $event:ident $resource:ident) => {{
        let id = $resource::id_raw((*$event).world);
        raw::ecs_try_resource_rid((*$event).world, id)
            .cast::<$resource>()
            .as_ref()
            .map(Res::new)
    }};
    (opt_mut $event:ident $resource:ident) => {{
        let id = $resource::id_raw((*$event).world);
        raw::ecs_try_resource_rid((*$event).world, id)
            .cast::<$resource>()
            .as_mut()
            .map(ResMut::new)
    }};
}

macro_rules! impl_observer_each {
    ($(($component:ident, $kind:ident)),+ $(,)?) => {
        impl<E, F, $($component),+> ObserverEach<E, fn(ObserverPayload, $(observer_marker_ty!($kind $component)),+)> for F
        where
            E: Event,
            F: FnMut(&<E as Event>::Payload, $(observer_marker_arg!($kind $component)),+),
            $($component: Component),+
        {
            #[inline]
            fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16) -> Result<(), ParamError> {
                $(
                    let id = $component::id(world);
                    append_term(desc, term_index, id, observer_access!($kind))?;
                )+
                Ok(())
            }

            #[inline]
            unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
                self(observer_data_arg!(event E), $(observer_arg!($kind event $component)),+);
            }
        }

        impl<E, F, $($component),+> ObserverEach<E, fn(ObserverEvent<'_>, ObserverPayload, $(observer_marker_ty!($kind $component)),+)> for F
        where
            E: Event,
            F: FnMut(ObserverEvent<'_>, &<E as Event>::Payload, $(observer_marker_arg!($kind $component)),+),
            $($component: Component),+
        {
            #[inline]
            fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16) -> Result<(), ParamError> {
                $(
                    let id = $component::id(world);
                    append_term(desc, term_index, id, observer_access!($kind))?;
                )+
                Ok(())
            }

            #[inline]
            unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
                self(ObserverEvent::from_raw(event), observer_data_arg!(event E), $(observer_arg!($kind event $component)),+);
            }
        }

        impl<E, F, ResA, ResB, $($component),+>
            ObserverEach<E, fn(ObserverPayload, $(observer_marker_ty!($kind $component)),+, ObserverRes<ResA>, ObserverResMut<ResB>)> for F
        where
            E: Event,
                F: FnMut(&<E as Event>::Payload, $(observer_marker_arg!($kind $component)),+, Res<ResA>, ResMut<ResB>),
            ResA: Resource,
            ResB: Resource,
            $($component: Component),+
        {
            #[inline]
            fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16) -> Result<(), ParamError> {
                $(
                    let id = $component::id(world);
                    append_term(desc, term_index, id, observer_access!($kind))?;
                )+
                Ok(())
            }

            #[inline]
            fn validate_resources(world: &mut World, access: &mut ResourceAccess) -> Result<(), ParamError> {
                access.read::<ResA>(world)?;
                access.write::<ResB>(world)?;
                Ok(())
            }

            #[inline]
            unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
                self(
                    observer_data_arg!(event E),
                    $(observer_arg!($kind event $component)),+,
                    observer_res_arg!(ref event ResA),
                    observer_res_arg!(mut event ResB),
                );
            }
        }
    };
}

impl<E, F> ObserverEach<E, fn(ObserverPayload)> for F
where
    E: Event,
    F: FnMut(&<E as Event>::Payload),
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(observer_data_arg!(event E));
    }
}

impl<E, F> ObserverEach<E, fn(ObserverEvent<'_>)> for F
where
    E: Event,
    F: FnMut(ObserverEvent<'_>),
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(ObserverEvent::from_raw(event));
    }
}

impl<E, F> ObserverEach<E, fn(ObserverEvent<'_>, ObserverPayload)> for F
where
    E: Event,
    F: FnMut(ObserverEvent<'_>, &<E as Event>::Payload),
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(ObserverEvent::from_raw(event), observer_data_arg!(event E));
    }
}

impl<E, F> ObserverEach<E, fn(Entity, ObserverPayload)> for F
where
    E: Event,
    F: FnMut(Entity, &<E as Event>::Payload),
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(
            Entity::from_raw((*event).entity),
            observer_data_arg!(event E),
        );
    }
}

impl<F, A> ObserverEach<RawEvent, fn(ObserverEvent<'_>, ObserverRef<A>)> for F
where
    F: FnMut(ObserverEvent<'_>, &A),
    A: Component,
{
    #[inline]
    fn append_terms(
        world: &mut World,
        desc: &mut raw::QueryDesc,
        term_index: &mut u16,
    ) -> Result<(), ParamError> {
        let id = A::id(world);
        append_term(desc, term_index, id, raw::TermAccess::In)
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(ObserverEvent::from_raw(event), observer_arg!(ref event A));
    }
}

impl<F, A> ObserverEach<RawEvent, fn(ObserverEvent<'_>, ObserverMut<A>)> for F
where
    F: FnMut(ObserverEvent<'_>, &mut A),
    A: Component,
{
    #[inline]
    fn append_terms(
        world: &mut World,
        desc: &mut raw::QueryDesc,
        term_index: &mut u16,
    ) -> Result<(), ParamError> {
        let id = A::id(world);
        append_term(desc, term_index, id, raw::TermAccess::InOut)
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(ObserverEvent::from_raw(event), observer_arg!(mut event A));
    }
}

impl<E, F, ResA> ObserverEach<E, fn(ObserverPayload, ObserverRes<ResA>)> for F
where
    E: Event,
    F: FnMut(&<E as Event>::Payload, Res<ResA>),
    ResA: Resource,
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    fn validate_resources(
        world: &mut World,
        access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        access.read::<ResA>(world)
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(
            observer_data_arg!(event E),
            observer_res_arg!(ref event ResA),
        );
    }
}

impl<E, F, ResA> ObserverEach<E, fn(ObserverPayload, ObserverResMut<ResA>)> for F
where
    E: Event,
    F: FnMut(&<E as Event>::Payload, ResMut<ResA>),
    ResA: Resource,
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    fn validate_resources(
        world: &mut World,
        access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        access.write::<ResA>(world)
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(
            observer_data_arg!(event E),
            observer_res_arg!(mut event ResA),
        );
    }
}

impl<E, F, ResA> ObserverEach<E, fn(ObserverPayload, ObserverOptRes<ResA>)> for F
where
    E: Event,
    F: FnMut(&<E as Event>::Payload, Option<Res<ResA>>),
    ResA: Resource,
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    fn validate_resources(
        world: &mut World,
        access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        access.optional_read::<ResA>(world)
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(
            observer_data_arg!(event E),
            observer_res_arg!(opt_ref event ResA),
        );
    }
}

impl<E, F, ResA> ObserverEach<E, fn(ObserverPayload, ObserverOptResMut<ResA>)> for F
where
    E: Event,
    F: FnMut(&<E as Event>::Payload, Option<ResMut<ResA>>),
    ResA: Resource,
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    fn validate_resources(
        world: &mut World,
        access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        access.optional_write::<ResA>(world)
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(
            observer_data_arg!(event E),
            observer_res_arg!(opt_mut event ResA),
        );
    }
}

impl<E, F, ResA, ResB> ObserverEach<E, fn(ObserverPayload, ObserverRes<ResA>, ObserverResMut<ResB>)>
    for F
where
    E: Event,
    F: FnMut(&<E as Event>::Payload, Res<ResA>, ResMut<ResB>),
    ResA: Resource,
    ResB: Resource,
{
    #[inline]
    fn append_terms(
        _world: &mut World,
        _desc: &mut raw::QueryDesc,
        _term_index: &mut u16,
    ) -> Result<(), ParamError> {
        Ok(())
    }

    #[inline]
    fn validate_resources(
        world: &mut World,
        access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        access.read::<ResA>(world)?;
        access.write::<ResB>(world)?;
        Ok(())
    }

    #[inline]
    unsafe fn run(&mut self, event: *mut raw::ObserverEvent) {
        self(
            observer_data_arg!(event E),
            observer_res_arg!(ref event ResA),
            observer_res_arg!(mut event ResB),
        );
    }
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
        impl_observer_each_perms_inner!(($($acc)* ($component, opt_ref),) ());
        impl_observer_each_perms_inner!(($($acc)* ($component, opt_mut),) ());
    };
    (($($acc:tt)*) ($component:ident, $($rest:tt)+)) => {
        impl_observer_each_perms_inner!(($($acc)* ($component, ref),) ($($rest)+));
        impl_observer_each_perms_inner!(($($acc)* ($component, mut),) ($($rest)+));
        impl_observer_each_perms_inner!(($($acc)* ($component, opt_ref),) ($($rest)+));
        impl_observer_each_perms_inner!(($($acc)* ($component, opt_mut),) ($($rest)+));
    };
}

impl_observer_each_perms!(A);
impl_observer_each_perms!(A, B);
impl_observer_each_perms!(A, B, C);
impl_observer_each_perms!(A, B, C, D);
