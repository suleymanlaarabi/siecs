use core::marker::PhantomData;

use crate::query::{
    resource_mut, resource_ref, validate_returned_fields, ParamError, QueryParam, QueryTerms,
    ResourceAccess,
};
use crate::{raw, Component, Entity, Query, Res, ResMut, Resource, World, WorldRef};

pub use raw::Phase;

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[repr(transparent)]
pub struct SystemId(raw::SystemId);

impl SystemId {
    #[inline]
    pub const fn raw(self) -> raw::SystemId {
        self.0
    }
}

impl From<raw::SystemId> for SystemId {
    #[inline]
    fn from(id: raw::SystemId) -> Self {
        Self(id)
    }
}

#[derive(Clone, Copy)]
pub struct EachCtx<'a> {
    world: WorldRef<'a>,
    entity: Entity,
}

impl<'a> EachCtx<'a> {
    #[inline]
    #[allow(dead_code)]
    pub(crate) unsafe fn new(iter: &raw::Iter, row: usize) -> Self {
        Self {
            world: WorldRef::from_raw(iter.world),
            entity: Entity::from_raw(*iter.entities.add(row)),
        }
    }

    #[inline]
    pub const fn entity(self) -> Entity {
        self.entity
    }

    #[inline]
    pub const fn world(self) -> WorldRef<'a> {
        self.world
    }

    #[inline]
    pub fn add<T: Component>(self) {
        self.world.add::<T>(self.entity);
    }

    #[inline]
    pub fn remove<T: Component>(self) {
        self.world.remove::<T>(self.entity);
    }

    #[inline]
    pub fn set<T: Component>(self, value: T) {
        self.world.set(self.entity, value);
    }

    #[inline]
    pub fn kill(self) {
        self.world.kill(self.entity);
    }

    #[inline]
    pub fn is_a(self, base: Entity) {
        self.world.is_a(self.entity, base);
    }
}

pub struct Commands {
    world: *mut raw::WorldRaw,
}

impl Commands {
    #[inline]
    fn world(&self) -> WorldRef<'_> {
        unsafe { WorldRef::from_raw(self.world) }
    }

    #[inline]
    pub fn add<T: Component>(&self, entity: Entity) {
        self.world().add::<T>(entity);
    }

    #[inline]
    pub fn remove<T: Component>(&self, entity: Entity) {
        self.world().remove::<T>(entity);
    }

    #[inline]
    pub fn set<T: Component>(&self, entity: Entity, value: T) {
        self.world().set(entity, value);
    }

    #[inline]
    pub fn kill(&self, entity: Entity) {
        self.world().kill(entity);
    }

    #[inline]
    pub fn is_a(&self, entity: Entity, base: Entity) {
        self.world().is_a(entity, base);
    }
}

#[derive(Clone, Copy)]
pub struct SystemContext<'world> {
    world: *mut raw::WorldRaw,
    iter: *mut raw::Iter,
    _marker: PhantomData<&'world mut raw::WorldRaw>,
}

impl<'world> SystemContext<'world> {
    #[inline]
    unsafe fn new(iter: *mut raw::Iter) -> Self {
        Self {
            world: (*iter).world,
            iter,
            _marker: PhantomData,
        }
    }
}

pub struct SystemDescBuilder {
    desc: raw::SystemDesc,
    terms: QueryTerms,
    resources: ResourceAccess,
}

impl SystemDescBuilder {
    #[inline]
    fn new() -> Self {
        Self {
            desc: raw::SystemDesc::default(),
            terms: QueryTerms::default(),
            resources: ResourceAccess::default(),
        }
    }

    #[inline]
    pub fn read_resource<T: Resource>(&mut self, world: &mut World) -> Result<(), ParamError> {
        self.resources.read::<T>(world)
    }

    #[inline]
    pub fn write_resource<T: Resource>(&mut self, world: &mut World) -> Result<(), ParamError> {
        self.resources.write::<T>(world)
    }

    #[inline]
    pub fn query_terms(&mut self) -> &mut QueryTerms {
        &mut self.terms
    }

    #[inline]
    fn finish(mut self) -> Result<raw::SystemDesc, ParamError> {
        validate_returned_fields(&self.terms.desc)?;
        self.desc.query = self.terms.desc;
        Ok(self.desc)
    }
}

pub unsafe trait SystemParam {
    type State;
    type Item<'world>;

    const NEEDS_DEFER: bool = false;

    fn init_state(
        world: &mut World,
        desc: &mut SystemDescBuilder,
    ) -> Result<Self::State, ParamError>;
    unsafe fn get_param<'world>(
        state: &'world mut Self::State,
        ctx: SystemContext<'world>,
    ) -> Self::Item<'world>;
}

unsafe impl<P: QueryParam> SystemParam for Query<P> {
    type State = P::State;
    type Item<'world> = Query<P>;

    #[inline]
    fn init_state(
        world: &mut World,
        desc: &mut SystemDescBuilder,
    ) -> Result<Self::State, ParamError> {
        P::init_state(world, desc.query_terms())
    }

    #[inline]
    unsafe fn get_param<'world>(
        state: &'world mut Self::State,
        ctx: SystemContext<'world>,
    ) -> Self::Item<'world> {
        Query::from_iter(ctx.world, ctx.iter, state)
    }
}

unsafe impl<T: Resource> SystemParam for Res<T> {
    type State = ();
    type Item<'world> = Res<T>;

    #[inline]
    fn init_state(
        world: &mut World,
        desc: &mut SystemDescBuilder,
    ) -> Result<Self::State, ParamError> {
        desc.read_resource::<T>(world)
    }

    #[inline]
    unsafe fn get_param<'world>(
        _state: &'world mut Self::State,
        ctx: SystemContext<'world>,
    ) -> Self::Item<'world> {
        resource_ref::<T>(ctx.world)
    }
}

unsafe impl<T: Resource> SystemParam for ResMut<T> {
    type State = ();
    type Item<'world> = ResMut<T>;

    #[inline]
    fn init_state(
        world: &mut World,
        desc: &mut SystemDescBuilder,
    ) -> Result<Self::State, ParamError> {
        desc.write_resource::<T>(world)
    }

    #[inline]
    unsafe fn get_param<'world>(
        _state: &'world mut Self::State,
        ctx: SystemContext<'world>,
    ) -> Self::Item<'world> {
        resource_mut::<T>(ctx.world)
    }
}

unsafe impl SystemParam for Commands {
    type State = ();
    type Item<'world> = Commands;

    const NEEDS_DEFER: bool = true;

    #[inline]
    fn init_state(
        _world: &mut World,
        _desc: &mut SystemDescBuilder,
    ) -> Result<Self::State, ParamError> {
        Ok(())
    }

    #[inline]
    unsafe fn get_param<'world>(
        _state: &'world mut Self::State,
        ctx: SystemContext<'world>,
    ) -> Self::Item<'world> {
        Commands { world: ctx.world }
    }
}

pub trait IntoSystem<Marker> {
    fn into_system(self, name: &str, world: &mut World) -> Result<SystemId, ParamError>;
}

pub struct System;

trait SystemParamTuple {
    type State;

    const NEEDS_DEFER: bool;

    fn init_state(
        world: &mut World,
        builder: &mut SystemDescBuilder,
    ) -> Result<Self::State, ParamError>;
}

trait RunSystem<F>: SystemParamTuple {
    unsafe fn run(func: &mut F, state: &mut Self::State, ctx: SystemContext<'_>);
}

trait SystemParamFunction<Marker>: Sized + 'static {
    type Params: RunSystem<Self> + 'static;
}

struct SystemStorage<F, Params: SystemParamTuple> {
    func: F,
    state: Params::State,
}

unsafe extern "C" fn system_callback<F, Params>(iter: *mut raw::Iter)
where
    F: 'static,
    Params: RunSystem<F> + 'static,
    Params::State: 'static,
{
    debug_assert!(!iter.is_null());

    let ctx = SystemContext::new(iter);
    let storage = (*iter).user_data as *mut SystemStorage<F, Params>;
    debug_assert!(!storage.is_null());

    if Params::NEEDS_DEFER {
        raw::ecs_defer_begin(ctx.world);
    }

    Params::run(&mut (*storage).func, &mut (*storage).state, ctx);

    if Params::NEEDS_DEFER {
        raw::ecs_defer_end(ctx.world);
    }
}

unsafe extern "C" fn system_storage_dtor<F, Params>(user_data: usize)
where
    Params: SystemParamTuple,
    Params::State: 'static,
{
    drop(unsafe { Box::from_raw(user_data as *mut SystemStorage<F, Params>) });
}

impl<F, Marker> IntoSystem<Marker> for F
where
    F: SystemParamFunction<Marker>,
    <<F as SystemParamFunction<Marker>>::Params as SystemParamTuple>::State: 'static,
{
    #[inline]
    fn into_system(self, name: &str, world: &mut World) -> Result<SystemId, ParamError> {
        type Params<F, Marker> = <F as SystemParamFunction<Marker>>::Params;

        let mut builder = SystemDescBuilder::new();
        let state = <Params<F, Marker> as SystemParamTuple>::init_state(world, &mut builder)?;
        let storage = Box::new(SystemStorage::<F, Params<F, Marker>> { func: self, state });

        let mut desc = builder.finish()?;
        desc.name = world.retain_system_name(name);
        desc.callback = Some(system_callback::<F, Params<F, Marker>>);
        desc.user_data = Box::into_raw(storage) as usize;
        desc.user_data_dtor = Some(system_storage_dtor::<F, Params<F, Marker>>);

        Ok(unsafe { raw::ecs_system_init(world.as_raw_mut(), &desc).into() })
    }
}

macro_rules! impl_into_system {
    ($(($param:ident, $state:tt)),* $(,)?) => {
        impl<$($param),*> SystemParamTuple for ($($param,)*)
        where
            $($param: SystemParam + 'static),*
        {
            type State = ($($param::State,)*);

            const NEEDS_DEFER: bool = false $(|| $param::NEEDS_DEFER)*;

            #[inline]
            fn init_state(
                world: &mut World,
                builder: &mut SystemDescBuilder,
            ) -> Result<Self::State, ParamError> {
                Ok(($($param::init_state(world, builder)?,)*))
            }
        }

        impl<F, $($param),*> RunSystem<F> for ($($param,)*)
        where
            F: FnMut($($param),*) + 'static,
            $($param: for<'world> SystemParam<Item<'world> = $param> + 'static,)*
            <($($param,)*) as SystemParamTuple>::State: 'static,
        {
            #[inline]
            unsafe fn run(func: &mut F, state: &mut Self::State, ctx: SystemContext<'_>) {
                func($($param::get_param(&mut state.$state, ctx),)*);
            }
        }

        impl<F, $($param),*> SystemParamFunction<fn($($param),*)> for F
        where
            F: FnMut($($param),*) + 'static,
            $($param: for<'world> SystemParam<Item<'world> = $param> + 'static,)*
            <($($param,)*) as SystemParamTuple>::State: 'static,
        {
            type Params = ($($param,)*);
        }
    };
}

impl_into_system!((P1, 0));
impl_into_system!((P1, 0), (P2, 1));
impl_into_system!((P1, 0), (P2, 1), (P3, 2));
impl_into_system!((P1, 0), (P2, 1), (P3, 2), (P4, 3));
impl_into_system!((P1, 0), (P2, 1), (P3, 2), (P4, 3), (P5, 4));
impl_into_system!((P1, 0), (P2, 1), (P3, 2), (P4, 3), (P5, 4), (P6, 5));
impl_into_system!(
    (P1, 0),
    (P2, 1),
    (P3, 2),
    (P4, 3),
    (P5, 4),
    (P6, 5),
    (P7, 6)
);
impl_into_system!(
    (P1, 0),
    (P2, 1),
    (P3, 2),
    (P4, 3),
    (P5, 4),
    (P6, 5),
    (P7, 6),
    (P8, 7)
);
