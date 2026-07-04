use core::marker::PhantomData;
use core::mem::{needs_drop, size_of, MaybeUninit};

use crate::query::{append_term, validate_returned_fields, QueryEach, ResourceAccess};
use crate::{raw, Component, Entity, World, WorldRef};

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

pub struct System<'world> {
    world: &'world mut World,
    name: String,
    desc: raw::SystemDesc,
    term_index: u16,
}

impl<'world> System<'world> {
    #[inline]
    pub(crate) fn new(world: &'world mut World, name: &str) -> Self {
        Self {
            world,
            name: name.to_owned(),
            desc: raw::SystemDesc::default(),
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
    pub fn is_a(mut self, base: Entity) -> Self {
        self.desc.query.is_a = base.id();
        self
    }

    #[inline]
    pub fn phase(mut self, phase: Phase) -> Self {
        self.desc.phase = phase;
        self
    }

    #[inline]
    pub fn disabled(mut self) -> Self {
        self.desc.disabled = true;
        self
    }

    #[inline]
    pub fn after(mut self, system: SystemId) -> Self {
        let slot = self
            .desc
            .after
            .iter_mut()
            .find(|slot| **slot == 0)
            .expect("systems can depend on at most four prior systems");
        *slot = system.raw();
        self
    }

    #[inline]
    pub fn each<F, Marker>(mut self, func: F) -> SystemId
    where
        F: QueryEach<Marker> + 'static,
    {
        assert!(
            size_of::<F>() == 0 && !needs_drop::<F>(),
            "system callbacks must be stateless"
        );
        let _ = func;

        F::append_terms(self.world, &mut self.desc.query, &mut self.term_index);
        let mut resource_access = ResourceAccess::default();
        F::validate_resources(self.world, &mut resource_access);
        validate_returned_fields(&self.desc.query);

        self.desc.name = self.world.retain_system_name(&self.name);
        self.desc.callback = Some(system_callback::<F, Marker>);

        unsafe { raw::ecs_system_init(self.world.as_raw_mut(), &self.desc).into() }
    }

    #[inline]
    fn append_component<T: Component>(&mut self, access: raw::TermAccess) {
        let id = T::id(self.world);
        append_term(&mut self.desc.query, &mut self.term_index, id, access);
    }
}

unsafe extern "C" fn system_callback<F, Marker>(iter: *mut raw::Iter)
where
    F: QueryEach<Marker> + 'static,
{
    debug_assert!(!iter.is_null());
    debug_assert_eq!(size_of::<F>(), 0);
    debug_assert!(!needs_drop::<F>());

    let mut func = MaybeUninit::<F>::zeroed().assume_init();
    F::run(&mut func, &mut *iter);
}

#[doc(hidden)]
pub struct SystemMarker<T>(PhantomData<T>);
