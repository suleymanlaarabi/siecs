use core::marker::PhantomData;
use core::ops::Deref;

use crate::{raw, Component, Entity, Res, ResMut, Resource, World};

pub use raw::FieldKind;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ParamError {
    DuplicateComponentField,
    ResourceReadWriteConflict,
    DuplicateMutableResource,
    MissingRequiredResource,
    TooManyTerms,
    TooManyResources,
}

impl core::fmt::Display for ParamError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(match self {
            Self::DuplicateComponentField => {
                "query cannot request the same component field more than once"
            }
            Self::ResourceReadWriteConflict => {
                "callback cannot request mutable and immutable access to the same resource"
            }
            Self::DuplicateMutableResource => {
                "callback cannot request mutable access to the same resource more than once"
            }
            Self::MissingRequiredResource => "callback requested a missing required resource",
            Self::TooManyTerms => "too many query terms",
            Self::TooManyResources => "too many resource params",
        })
    }
}

impl std::error::Error for ParamError {}

pub struct QueryState<P: QueryParam> {
    world: *mut raw::WorldRaw,
    id: raw::QueryId,
    param_state: P::State,
}

impl<P: QueryParam> QueryState<P> {
    #[inline]
    pub(crate) fn new(world: &mut World) -> Result<Self, ParamError> {
        let mut terms = QueryTerms::default();
        let param_state = P::init_state(world, &mut terms)?;
        validate_returned_fields(&terms.desc)?;

        let world = world.as_raw_mut();
        let id = unsafe { raw::ecs_query_init(world, &terms.desc) as raw::QueryId };

        Ok(Self {
            world,
            id,
            param_state,
        })
    }

    #[inline]
    pub fn id(&self) -> raw::QueryId {
        self.id
    }

    #[inline]
    pub fn query(&mut self, world: &mut World) -> Query<P> {
        Query::from_state(world.as_raw_mut(), self)
    }

    #[inline]
    pub fn each<F>(&mut self, world: &mut World, mut func: F)
    where
        F: for<'item> FnMut(P::Item<'item>),
    {
        for item in &mut self.query(world) {
            func(item);
        }
    }
}

impl<P: QueryParam> Drop for QueryState<P> {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            raw::ecs_query_fini(self.world, self.id);
        }
    }
}

pub struct Query<P: QueryParam> {
    world: *mut raw::WorldRaw,
    state: QueryStateRef<P>,
}

enum QueryStateRef<P: QueryParam> {
    Owned(QueryState<P>),
    Borrowed(*mut QueryState<P>),
    Iter {
        iter: *mut raw::Iter,
        param_state: *const P::State,
    },
}

impl<P: QueryParam> Query<P> {
    #[inline]
    pub(crate) fn new(world: &mut World) -> Result<Self, ParamError> {
        let state = QueryState::new(world)?;
        Ok(Self {
            world: world.as_raw_mut(),
            state: QueryStateRef::Owned(state),
        })
    }

    #[inline]
    fn from_state(world: *mut raw::WorldRaw, state: &mut QueryState<P>) -> Self {
        Self {
            world,
            state: QueryStateRef::Borrowed(state),
        }
    }

    #[inline]
    pub(crate) unsafe fn from_iter(
        world: *mut raw::WorldRaw,
        iter: *mut raw::Iter,
        param_state: &P::State,
    ) -> Self {
        Self {
            world,
            state: QueryStateRef::Iter { iter, param_state },
        }
    }

    #[inline]
    fn query_id(&self) -> Option<raw::QueryId> {
        match &self.state {
            QueryStateRef::Owned(state) => Some(state.id),
            QueryStateRef::Borrowed(state) => Some(unsafe { (**state).id }),
            QueryStateRef::Iter { .. } => None,
        }
    }

    #[inline]
    fn param_state(&self) -> &P::State {
        match &self.state {
            QueryStateRef::Owned(state) => &state.param_state,
            QueryStateRef::Borrowed(state) => unsafe { &(**state).param_state },
            QueryStateRef::Iter { param_state, .. } => unsafe { &**param_state },
        }
    }
}

impl<'query, P: QueryParam> IntoIterator for &'query mut Query<P> {
    type IntoIter = QueryIter<'query, P>;
    type Item = P::Item<'query>;

    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        match &self.state {
            QueryStateRef::Iter { iter, .. } => QueryIter {
                state: self.param_state(),
                iter: QueryIterSource::Borrowed(*iter),
                fetch: None,
                row: 0,
            },
            _ => {
                let id = self.query_id().expect("query state missing");
                let iter = unsafe { raw::ecs_query_iter(self.world, id) };
                QueryIter {
                    state: self.param_state(),
                    iter: QueryIterSource::Owned(iter),
                    fetch: None,
                    row: 0,
                }
            }
        }
    }
}

pub struct QueryIter<'query, P: QueryParam + 'query> {
    state: &'query P::State,
    iter: QueryIterSource,
    fetch: Option<P::Fetch<'query>>,
    row: usize,
}

enum QueryIterSource {
    Owned(raw::Iter),
    Borrowed(*mut raw::Iter),
}

impl QueryIterSource {
    #[inline]
    fn as_mut_ptr(&mut self) -> *mut raw::Iter {
        match self {
            Self::Owned(iter) => iter,
            Self::Borrowed(iter) => *iter,
        }
    }

    #[inline]
    fn is_owned(&self) -> bool {
        matches!(self, Self::Owned(_))
    }
}

impl<'query, P: QueryParam + 'query> Iterator for QueryIter<'query, P> {
    type Item = P::Item<'query>;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(fetch) = self.fetch.as_mut() {
                let iter = unsafe { &*self.iter.as_mut_ptr() };
                if self.row < iter.count as usize {
                    let row = self.row;
                    self.row += 1;
                    return Some(unsafe { P::item(fetch, row) });
                }
            }

            if !self.iter.is_owned() && self.fetch.is_some() {
                return None;
            }

            let iter = self.iter.as_mut_ptr();
            if self.iter.is_owned() && !unsafe { raw::ecs_iter_next(iter) } {
                return None;
            }

            self.fetch = Some(unsafe { P::fetch(self.state, iter) });
            self.row = 0;
        }
    }
}

pub struct Field<'a, T> {
    value: &'a T,
    kind: raw::FieldKind,
}

impl<T> Clone for Field<'_, T> {
    #[inline]
    fn clone(&self) -> Self {
        *self
    }
}

impl<T> Copy for Field<'_, T> {}

impl<'a, T> Field<'a, T> {
    #[inline]
    pub(crate) const fn new(value: &'a T, kind: raw::FieldKind) -> Self {
        Self { value, kind }
    }

    #[inline]
    pub const fn get(&self) -> &'a T {
        self.value
    }

    #[inline]
    pub const fn kind(&self) -> raw::FieldKind {
        self.kind
    }

    #[inline]
    pub const fn is_owned(&self) -> bool {
        matches!(self.kind, raw::FieldKind::Owned)
    }

    #[inline]
    pub const fn is_shared(&self) -> bool {
        matches!(self.kind, raw::FieldKind::Shared)
    }
}

impl<T> Deref for Field<'_, T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.value
    }
}

#[doc(hidden)]
#[derive(Default)]
pub struct QueryTerms {
    pub(crate) desc: raw::QueryDesc,
    term_index: u16,
}

impl QueryTerms {
    #[inline]
    pub fn read<T: Component>(&mut self, world: &mut World) -> Result<u16, ParamError> {
        let index = self.term_index;
        append_term(
            &mut self.desc,
            &mut self.term_index,
            T::id(world),
            raw::TermAccess::In,
        )?;
        Ok(index)
    }

    #[inline]
    pub fn write<T: Component>(&mut self, world: &mut World) -> Result<u16, ParamError> {
        let index = self.term_index;
        append_term(
            &mut self.desc,
            &mut self.term_index,
            T::id(world),
            raw::TermAccess::InOut,
        )?;
        Ok(index)
    }

    #[inline]
    pub fn optional_read<T: Component>(&mut self, world: &mut World) -> Result<u16, ParamError> {
        let index = self.term_index;
        append_term(
            &mut self.desc,
            &mut self.term_index,
            T::id(world),
            raw::TermAccess::InOptional,
        )?;
        Ok(index)
    }

    #[inline]
    pub fn optional_write<T: Component>(&mut self, world: &mut World) -> Result<u16, ParamError> {
        let index = self.term_index;
        append_term(
            &mut self.desc,
            &mut self.term_index,
            T::id(world),
            raw::TermAccess::InOutOptional,
        )?;
        Ok(index)
    }
}

#[inline]
pub(crate) fn append_term(
    desc: &mut raw::QueryDesc,
    term_index: &mut u16,
    id: raw::ComponentId,
    access: raw::TermAccess,
) -> Result<(), ParamError> {
    if (*term_index as usize) + 1 >= desc.terms.len() {
        return Err(ParamError::TooManyTerms);
    }

    desc.terms[*term_index as usize] = raw::QueryTerm { id, access };
    *term_index += 1;
    Ok(())
}

#[inline]
fn is_returned_field(access: raw::TermAccess) -> bool {
    matches!(
        access,
        raw::TermAccess::In
            | raw::TermAccess::Out
            | raw::TermAccess::InOut
            | raw::TermAccess::InOptional
            | raw::TermAccess::InOutOptional
    )
}

pub(crate) fn validate_returned_fields(desc: &raw::QueryDesc) -> Result<(), ParamError> {
    for (left_index, left) in desc.terms.iter().enumerate() {
        if left.id == 0 || !is_returned_field(left.access) {
            continue;
        }

        for right in desc.terms.iter().skip(left_index + 1) {
            if right.id == 0 {
                break;
            }

            if is_returned_field(right.access) && left.id == right.id {
                return Err(ParamError::DuplicateComponentField);
            }
        }
    }

    Ok(())
}

#[doc(hidden)]
#[derive(Default)]
pub struct ResourceAccess {
    reads: [raw::ResourceId; 16],
    read_count: usize,
    writes: [raw::ResourceId; 16],
    write_count: usize,
}

impl ResourceAccess {
    #[inline]
    pub(crate) fn read<T: Resource>(&mut self, world: &mut World) -> Result<(), ParamError> {
        let id = T::id(world);
        if !unsafe { raw::ecs_has_resource_rid(world.as_raw(), id) } {
            return Err(ParamError::MissingRequiredResource);
        }
        self.assert_not_written(id)?;
        self.push_read(id)
    }

    #[inline]
    pub(crate) fn write<T: Resource>(&mut self, world: &mut World) -> Result<(), ParamError> {
        let id = T::id(world);
        if !unsafe { raw::ecs_has_resource_rid(world.as_raw(), id) } {
            return Err(ParamError::MissingRequiredResource);
        }
        self.assert_not_read(id)?;
        self.assert_not_written(id)?;
        self.push_write(id)
    }

    #[inline]
    pub(crate) fn optional_read<T: Resource>(
        &mut self,
        world: &mut World,
    ) -> Result<(), ParamError> {
        let id = T::id(world);
        self.assert_not_written(id)?;
        self.push_read(id)
    }

    #[inline]
    pub(crate) fn optional_write<T: Resource>(
        &mut self,
        world: &mut World,
    ) -> Result<(), ParamError> {
        let id = T::id(world);
        self.assert_not_read(id)?;
        self.assert_not_written(id)?;
        self.push_write(id)
    }

    #[inline]
    fn push_read(&mut self, id: raw::ResourceId) -> Result<(), ParamError> {
        if self.read_count >= self.reads.len() {
            return Err(ParamError::TooManyResources);
        }
        self.reads[self.read_count] = id;
        self.read_count += 1;
        Ok(())
    }

    #[inline]
    fn push_write(&mut self, id: raw::ResourceId) -> Result<(), ParamError> {
        if self.write_count >= self.writes.len() {
            return Err(ParamError::TooManyResources);
        }
        self.writes[self.write_count] = id;
        self.write_count += 1;
        Ok(())
    }

    #[inline]
    fn assert_not_read(&self, id: raw::ResourceId) -> Result<(), ParamError> {
        if self.reads[..self.read_count].contains(&id) {
            return Err(ParamError::ResourceReadWriteConflict);
        }
        Ok(())
    }

    #[inline]
    fn assert_not_written(&self, id: raw::ResourceId) -> Result<(), ParamError> {
        if self.writes[..self.write_count].contains(&id) {
            return Err(ParamError::DuplicateMutableResource);
        }
        Ok(())
    }
}

#[inline]
pub(crate) fn resource_ref<T: Resource>(world: *mut raw::WorldRaw) -> Res<T> {
    let id = unsafe { T::id_raw(world) };
    unsafe { Res::new(&*raw::ecs_resource_rid(world, id).cast::<T>()) }
}

#[inline]
pub(crate) fn resource_mut<T: Resource>(world: *mut raw::WorldRaw) -> ResMut<T> {
    let id = unsafe { T::id_raw(world) };
    unsafe { ResMut::new(&mut *raw::ecs_resource_rid(world, id).cast::<T>()) }
}

pub unsafe trait QueryParam {
    type State;
    type Fetch<'world>;
    type Item<'world>;

    fn init_state(world: &mut World, terms: &mut QueryTerms) -> Result<Self::State, ParamError>;
    unsafe fn fetch<'world>(
        state: &'world Self::State,
        iter: *mut raw::Iter,
    ) -> Self::Fetch<'world>;
    unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world>;
}

#[doc(hidden)]
pub struct ComponentFetch<'world, T> {
    ptr: *mut T,
    kind: raw::FieldKind,
    _marker: PhantomData<&'world mut T>,
}

#[inline]
unsafe fn field_fetch<'world, T>(iter: *mut raw::Iter, index: u16) -> ComponentFetch<'world, T> {
    ComponentFetch {
        ptr: raw::ecs_field(iter, index).cast::<T>(),
        kind: raw::ecs_field_kind(iter, index),
        _marker: PhantomData,
    }
}

#[inline]
unsafe fn field_ref<'world, T>(fetch: &ComponentFetch<'world, T>, row: usize) -> &'world T {
    if fetch.kind == raw::FieldKind::Shared {
        &*fetch.ptr
    } else {
        &*fetch.ptr.add(row)
    }
}

unsafe impl<T: Component> QueryParam for &T {
    type State = u16;
    type Fetch<'world> = ComponentFetch<'world, T>;
    type Item<'world> = &'world T;

    #[inline]
    fn init_state(world: &mut World, terms: &mut QueryTerms) -> Result<Self::State, ParamError> {
        terms.read::<T>(world)
    }

    #[inline]
    unsafe fn fetch<'world>(
        state: &'world Self::State,
        iter: *mut raw::Iter,
    ) -> Self::Fetch<'world> {
        field_fetch(iter, *state)
    }

    #[inline]
    unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world> {
        field_ref(fetch, row)
    }
}

unsafe impl<T: Component> QueryParam for &mut T {
    type State = u16;
    type Fetch<'world> = ComponentFetch<'world, T>;
    type Item<'world> = &'world mut T;

    #[inline]
    fn init_state(world: &mut World, terms: &mut QueryTerms) -> Result<Self::State, ParamError> {
        terms.write::<T>(world)
    }

    #[inline]
    unsafe fn fetch<'world>(
        state: &'world Self::State,
        iter: *mut raw::Iter,
    ) -> Self::Fetch<'world> {
        field_fetch(iter, *state)
    }

    #[inline]
    unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world> {
        &mut *fetch.ptr.add(row)
    }
}

unsafe impl<T: Component> QueryParam for Option<&T> {
    type State = u16;
    type Fetch<'world> = ComponentFetch<'world, T>;
    type Item<'world> = Option<&'world T>;

    #[inline]
    fn init_state(world: &mut World, terms: &mut QueryTerms) -> Result<Self::State, ParamError> {
        terms.optional_read::<T>(world)
    }

    #[inline]
    unsafe fn fetch<'world>(
        state: &'world Self::State,
        iter: *mut raw::Iter,
    ) -> Self::Fetch<'world> {
        field_fetch(iter, *state)
    }

    #[inline]
    unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world> {
        if fetch.ptr.is_null() {
            None
        } else {
            Some(field_ref(fetch, row))
        }
    }
}

unsafe impl<T: Component> QueryParam for Option<&mut T> {
    type State = u16;
    type Fetch<'world> = ComponentFetch<'world, T>;
    type Item<'world> = Option<&'world mut T>;

    #[inline]
    fn init_state(world: &mut World, terms: &mut QueryTerms) -> Result<Self::State, ParamError> {
        terms.optional_write::<T>(world)
    }

    #[inline]
    unsafe fn fetch<'world>(
        state: &'world Self::State,
        iter: *mut raw::Iter,
    ) -> Self::Fetch<'world> {
        field_fetch(iter, *state)
    }

    #[inline]
    unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world> {
        if fetch.ptr.is_null() {
            None
        } else {
            Some(&mut *fetch.ptr.add(row))
        }
    }
}

unsafe impl QueryParam for Entity {
    type State = ();
    type Fetch<'world> = *mut raw::EntityId;
    type Item<'world> = Entity;

    #[inline]
    fn init_state(_world: &mut World, _terms: &mut QueryTerms) -> Result<Self::State, ParamError> {
        Ok(())
    }

    #[inline]
    unsafe fn fetch<'world>(
        _state: &'world Self::State,
        iter: *mut raw::Iter,
    ) -> Self::Fetch<'world> {
        (*iter).entities
    }

    #[inline]
    unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world> {
        Entity::from_raw(*fetch.add(row))
    }
}

unsafe impl<T: Component> QueryParam for Field<'_, T> {
    type State = u16;
    type Fetch<'world> = ComponentFetch<'world, T>;
    type Item<'world> = Field<'world, T>;

    #[inline]
    fn init_state(world: &mut World, terms: &mut QueryTerms) -> Result<Self::State, ParamError> {
        terms.read::<T>(world)
    }

    #[inline]
    unsafe fn fetch<'world>(
        state: &'world Self::State,
        iter: *mut raw::Iter,
    ) -> Self::Fetch<'world> {
        field_fetch(iter, *state)
    }

    #[inline]
    unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world> {
        Field::new(field_ref(fetch, row), fetch.kind)
    }
}

unsafe impl<T: Component> QueryParam for Option<Field<'_, T>> {
    type State = u16;
    type Fetch<'world> = ComponentFetch<'world, T>;
    type Item<'world> = Option<Field<'world, T>>;

    #[inline]
    fn init_state(world: &mut World, terms: &mut QueryTerms) -> Result<Self::State, ParamError> {
        terms.optional_read::<T>(world)
    }

    #[inline]
    unsafe fn fetch<'world>(
        state: &'world Self::State,
        iter: *mut raw::Iter,
    ) -> Self::Fetch<'world> {
        field_fetch(iter, *state)
    }

    #[inline]
    unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world> {
        if fetch.ptr.is_null() {
            None
        } else {
            Some(Field::new(field_ref(fetch, row), fetch.kind))
        }
    }
}

macro_rules! impl_query_param_tuple {
    ($($name:ident $field:ident $index:tt),+ $(,)?) => {
        unsafe impl<$($name),+> QueryParam for ($($name,)+)
        where
            $($name: QueryParam),+
        {
            type State = ($($name::State,)+);
            type Fetch<'world> = ($($name::Fetch<'world>,)+);
            type Item<'world> = ($($name::Item<'world>,)+);

            #[inline]
            fn init_state(world: &mut World, terms: &mut QueryTerms) -> Result<Self::State, ParamError> {
                Ok(($($name::init_state(world, terms)?,)+))
            }

            #[inline]
            unsafe fn fetch<'world>(
                state: &'world Self::State,
                iter: *mut raw::Iter,
            ) -> Self::Fetch<'world> {
                ($($name::fetch(&state.$index, iter),)+)
            }

            #[inline]
            unsafe fn item<'world>(fetch: &mut Self::Fetch<'world>, row: usize) -> Self::Item<'world> {
                ($($name::item(&mut fetch.$index, row),)+)
            }
        }
    };
}

impl_query_param_tuple!(A a 0);
impl_query_param_tuple!(A a 0, B b 1);
impl_query_param_tuple!(A a 0, B b 1, C c 2);
impl_query_param_tuple!(A a 0, B b 1, C c 2, D d 3);
impl_query_param_tuple!(A a 0, B b 1, C c 2, D d 3, E e 4);
impl_query_param_tuple!(A a 0, B b 1, C c 2, D d 3, E e 4, F f 5);
impl_query_param_tuple!(A a 0, B b 1, C c 2, D d 3, E e 4, F f 5, G g 6);
impl_query_param_tuple!(A a 0, B b 1, C c 2, D d 3, E e 4, F f 5, G g 6, H h 7);
