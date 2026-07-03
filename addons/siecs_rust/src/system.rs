use core::marker::PhantomData;
use core::mem::{needs_drop, size_of, MaybeUninit};

use crate::query::{append_term, validate_returned_fields, QueryEach};
use crate::{raw, Component, Entity, World};

pub use raw::Phase;

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
    pub fn each<F, Marker>(mut self, func: F) -> raw::SystemId
    where
        F: QueryEach<Marker> + 'static,
    {
        assert!(
            size_of::<F>() == 0 && !needs_drop::<F>(),
            "system callbacks must be stateless"
        );
        let _ = func;

        F::append_terms(self.world, &mut self.desc.query, &mut self.term_index);
        validate_returned_fields(&self.desc.query);

        self.desc.name = self.world.retain_system_name(&self.name);
        self.desc.callback = Some(system_callback::<F, Marker>);

        unsafe { raw::ecs_system_init(self.world.as_raw_mut(), &self.desc) }
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
