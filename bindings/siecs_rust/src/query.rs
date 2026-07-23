use core::marker::PhantomData;
use core::ops::ControlFlow;

use crate::{raw, Component, Entity, Res, ResMut, Resource, World};

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

pub struct QueryState<P: QueryParam, F: QueryFilter = ()> {
    world: *mut raw::WorldRaw,
    id: raw::QueryId,
    param_state: P::State,
    fini_on_drop: bool,
    _filter: PhantomData<fn() -> F>,
}

impl<P: QueryParam, F: QueryFilter> QueryState<P, F> {
    #[inline]
    pub(crate) fn new(world: &mut World) -> Result<Self, ParamError> {
        Self::new_with_drop(world, true)
    }

    #[inline]
    pub(crate) fn new_system(world: &mut World) -> Result<Self, ParamError> {
        Self::new_with_drop(world, false)
    }

    #[inline]
    fn new_with_drop(world: &mut World, fini_on_drop: bool) -> Result<Self, ParamError> {
        let mut terms = QueryTerms::default();
        let param_state = P::init_state(world, &mut terms)?;
        F::init_filter(world, &mut terms)?;

        let world = world.as_raw_mut();
        let id = unsafe { raw::ecs_query_init(world, &terms.desc) as raw::QueryId };

        Ok(Self {
            world,
            id,
            param_state,
            fini_on_drop,
            _filter: PhantomData,
        })
    }

    #[inline]
    pub fn id(&self) -> raw::QueryId {
        self.id
    }

    #[inline]
    pub fn each<Func>(&mut self, _world: &mut World, func: Func)
    where
        Func: for<'item> FnMut(P::Item<'item>),
    {
        Query::from_state(self).each(func);
    }

    #[inline]
    pub fn try_each<Func, Break>(&mut self, _world: &mut World, func: Func) -> ControlFlow<Break>
    where
        Func: for<'item> FnMut(P::Item<'item>) -> ControlFlow<Break>,
    {
        Query::from_state(self).try_each(func)
    }
}

#[inline]
unsafe fn run_rows<P, Func, Break, const OWNED: bool>(
    fetch: &mut P::Fetch<'_>,
    count: usize,
    func: &mut Func,
) -> ControlFlow<Break>
where
    P: QueryParam,
    Func: for<'item> FnMut(P::Item<'item>) -> ControlFlow<Break>,
{
    for _ in 0..count {
        if let ControlFlow::Break(value) = func(P::item::<OWNED>(fetch)) {
            return ControlFlow::Break(value);
        }
    }
    ControlFlow::Continue(())
}

#[inline]
unsafe fn run_batch<P, Func, Break>(
    state: &P::State,
    iter: *mut raw::Iter,
    func: &mut Func,
) -> ControlFlow<Break>
where
    P: QueryParam,
    Func: for<'item> FnMut(P::Item<'item>) -> ControlFlow<Break>,
{
    let mut fetch = P::fetch(state, iter);
    let count = (*iter).count as usize;
    if P::all_owned(&fetch) {
        run_rows::<P, _, _, true>(&mut fetch, count, func)
    } else {
        run_rows::<P, _, _, false>(&mut fetch, count, func)
    }
}

#[inline]
unsafe fn run_query<P, Func, Break>(
    world: *mut raw::WorldRaw,
    id: raw::QueryId,
    state: &P::State,
    func: &mut Func,
) -> ControlFlow<Break>
where
    P: QueryParam,
    Func: for<'item> FnMut(P::Item<'item>) -> ControlFlow<Break>,
{
    let mut iter = raw::ecs_query_iter(world, id);
    while raw::ecs_iter_next(&mut iter) {
        if let ControlFlow::Break(value) = run_batch::<P, _, _>(state, &mut iter, func) {
            return ControlFlow::Break(value);
        }
    }
    ControlFlow::Continue(())
}

pub struct Query<P: QueryParam, F: QueryFilter = ()> {
    source: QuerySource<P, F>,
}

enum QuerySource<P: QueryParam, F: QueryFilter> {
    Owned(QueryState<P, F>),
    Borrowed(*mut QueryState<P, F>),
    Batch {
        iter: *mut raw::Iter,
        param_state: *const P::State,
    },
}

impl<P: QueryParam, F: QueryFilter> Query<P, F> {
    #[inline]
    pub(crate) fn new(world: &mut World) -> Result<Self, ParamError> {
        Ok(Self {
            source: QuerySource::Owned(QueryState::new(world)?),
        })
    }

    #[inline]
    pub(crate) fn from_state(state: &mut QueryState<P, F>) -> Self {
        Self {
            source: QuerySource::Borrowed(state),
        }
    }

    #[inline]
    pub(crate) unsafe fn from_iter(iter: *mut raw::Iter, param_state: &P::State) -> Self {
        Self {
            source: QuerySource::Batch { iter, param_state },
        }
    }

    #[inline]
    pub fn each<Func>(&mut self, mut func: Func)
    where
        Func: for<'item> FnMut(P::Item<'item>),
    {
        let result = self.try_each(|item| {
            func(item);
            ControlFlow::<()>::Continue(())
        });
        debug_assert!(result.is_continue());
    }

    #[inline]
    pub fn try_each<Func, Break>(&mut self, mut func: Func) -> ControlFlow<Break>
    where
        Func: for<'item> FnMut(P::Item<'item>) -> ControlFlow<Break>,
    {
        unsafe {
            match &mut self.source {
                QuerySource::Owned(state) => {
                    run_query::<P, _, _>(state.world, state.id, &state.param_state, &mut func)
                }
                QuerySource::Borrowed(state) => {
                    let state = &**state;
                    run_query::<P, _, _>(state.world, state.id, &state.param_state, &mut func)
                }
                QuerySource::Batch { iter, param_state } => {
                    run_batch::<P, _, _>(&**param_state, *iter, &mut func)
                }
            }
        }
    }
}

impl<P: QueryParam, F: QueryFilter> Drop for QueryState<P, F> {
    #[inline]
    fn drop(&mut self) {
        if !self.fini_on_drop {
            return;
        }

        unsafe {
            raw::ecs_query_fini(self.world, self.id);
        }
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
    pub fn field<T: Component>(
        &mut self,
        world: &mut World,
        access: raw::TermAccess,
    ) -> Result<u16, ParamError> {
        let id = T::id(world);
        if self
            .terms()
            .iter()
            .any(|term| term.id == id && is_returned_field(term.access))
        {
            return Err(ParamError::DuplicateComponentField);
        }

        let index = self.term_index;
        append_term(&mut self.desc, &mut self.term_index, id, access)?;
        Ok(index)
    }

    #[inline]
    pub fn require<T: Component>(&mut self, world: &mut World) -> Result<(), ParamError> {
        let id = T::id(world);
        match self.terms().iter().find(|term| term.id == id) {
            Some(term) if term.access == raw::TermAccess::Not => {
                Err(ParamError::DuplicateComponentField)
            }
            Some(_) => Ok(()),
            None => append_term(
                &mut self.desc,
                &mut self.term_index,
                id,
                raw::TermAccess::Filter,
            ),
        }
    }

    #[inline]
    pub fn exclude<T: Component>(&mut self, world: &mut World) -> Result<(), ParamError> {
        let id = T::id(world);
        match self.terms().iter().find(|term| term.id == id) {
            Some(term) if term.access == raw::TermAccess::Not => Ok(()),
            Some(_) => Err(ParamError::DuplicateComponentField),
            None => append_term(
                &mut self.desc,
                &mut self.term_index,
                id,
                raw::TermAccess::Not,
            ),
        }
    }

    #[inline]
    fn terms(&self) -> &[raw::QueryTerm] {
        &self.desc.terms[..self.term_index as usize]
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct With<T: Component>(PhantomData<fn() -> T>);

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct Without<T: Component>(PhantomData<fn() -> T>);

pub unsafe trait QueryFilter {
    fn init_filter(world: &mut World, terms: &mut QueryTerms) -> Result<(), ParamError>;
}

unsafe impl QueryFilter for () {
    #[inline]
    fn init_filter(_world: &mut World, _terms: &mut QueryTerms) -> Result<(), ParamError> {
        Ok(())
    }
}

unsafe impl<T: Component> QueryFilter for With<T> {
    #[inline]
    fn init_filter(world: &mut World, terms: &mut QueryTerms) -> Result<(), ParamError> {
        terms.require::<T>(world)
    }
}

unsafe impl<T: Component> QueryFilter for Without<T> {
    #[inline]
    fn init_filter(world: &mut World, terms: &mut QueryTerms) -> Result<(), ParamError> {
        terms.exclude::<T>(world)
    }
}

macro_rules! impl_query_filter_tuple {
    ($($name:ident),+ $(,)?) => {
        unsafe impl<$($name),+> QueryFilter for ($($name,)+)
        where
            $($name: QueryFilter),+
        {
            #[inline]
            fn init_filter(world: &mut World, terms: &mut QueryTerms) -> Result<(), ParamError> {
                $($name::init_filter(world, terms)?;)+
                Ok(())
            }
        }
    };
}

impl_query_filter_tuple!(A);
impl_query_filter_tuple!(A, B);
impl_query_filter_tuple!(A, B, C);
impl_query_filter_tuple!(A, B, C, D);
impl_query_filter_tuple!(A, B, C, D, E);
impl_query_filter_tuple!(A, B, C, D, E, F);
impl_query_filter_tuple!(A, B, C, D, E, F, G);
impl_query_filter_tuple!(A, B, C, D, E, F, G, H);

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
    entries: [ResourceEntry; 16],
    len: usize,
}

#[derive(Clone, Copy, Default)]
struct ResourceEntry {
    id: raw::ResourceId,
    mutable: bool,
}

#[derive(Clone, Copy)]
enum ResourceAccessKind {
    Read,
    Write,
}

impl ResourceAccess {
    #[inline]
    pub(crate) fn read<T: Resource>(&mut self, world: &mut World) -> Result<(), ParamError> {
        self.register::<T>(world, ResourceAccessKind::Read, true)
    }

    #[inline]
    pub(crate) fn write<T: Resource>(&mut self, world: &mut World) -> Result<(), ParamError> {
        self.register::<T>(world, ResourceAccessKind::Write, true)
    }

    #[inline]
    pub(crate) fn optional_read<T: Resource>(
        &mut self,
        world: &mut World,
    ) -> Result<(), ParamError> {
        self.register::<T>(world, ResourceAccessKind::Read, false)
    }

    #[inline]
    pub(crate) fn optional_write<T: Resource>(
        &mut self,
        world: &mut World,
    ) -> Result<(), ParamError> {
        self.register::<T>(world, ResourceAccessKind::Write, false)
    }

    #[inline]
    fn register<T: Resource>(
        &mut self,
        world: &mut World,
        kind: ResourceAccessKind,
        required: bool,
    ) -> Result<(), ParamError> {
        let id = T::id(world);
        if required && !unsafe { raw::ecs_has_resource_rid(world.as_raw(), id) } {
            return Err(ParamError::MissingRequiredResource);
        }

        let mutable = matches!(kind, ResourceAccessKind::Write);
        if let Some(previous) = self.entries[..self.len].iter().find(|entry| entry.id == id) {
            if previous.mutable {
                return Err(ParamError::DuplicateMutableResource);
            }
            if mutable {
                return Err(ParamError::ResourceReadWriteConflict);
            }
        }

        if self.len == self.entries.len() {
            return Err(ParamError::TooManyResources);
        }
        self.entries[self.len] = ResourceEntry { id, mutable };
        self.len += 1;
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
    fn all_owned(fetch: &Self::Fetch<'_>) -> bool;
    unsafe fn item<'world, const OWNED: bool>(
        fetch: &mut Self::Fetch<'world>,
    ) -> Self::Item<'world>;
}

#[doc(hidden)]
pub struct ComponentFetch<'world, T> {
    ptr: *mut T,
    step: usize,
    _marker: PhantomData<&'world mut T>,
}

impl<T> ComponentFetch<'_, T> {
    #[inline]
    fn all_owned(&self) -> bool {
        self.ptr.is_null() || self.step == 1
    }

    #[inline]
    unsafe fn next<const OWNED: bool>(&mut self) -> *mut T {
        let ptr = self.ptr;
        self.ptr = ptr.add(if OWNED { 1 } else { self.step });
        ptr
    }

    #[inline]
    unsafe fn next_optional<const OWNED: bool>(&mut self) -> Option<*mut T> {
        (!self.ptr.is_null()).then(|| self.next::<OWNED>())
    }
}

#[inline]
unsafe fn field_fetch<'world, T>(iter: *mut raw::Iter, index: u16) -> ComponentFetch<'world, T> {
    ComponentFetch {
        ptr: raw::ecs_field(iter, index).cast::<T>(),
        step: usize::from(raw::ecs_field_kind(iter, index) != raw::FieldKind::Shared),
        _marker: PhantomData,
    }
}

#[inline]
unsafe fn field_ref<'world, T, const OWNED: bool>(
    fetch: &mut ComponentFetch<'world, T>,
) -> &'world T {
    &*fetch.next::<OWNED>()
}

macro_rules! impl_component_param {
    (
        <$component:ident> for<$world:lifetime>
        $param:ty => $item:ty,
        $access:ident,
        |$fetch:ident, $owned:ident| $body:expr $(,)?
    ) => {
        unsafe impl<$component: Component> QueryParam for $param {
            type State = u16;
            type Fetch<$world> = ComponentFetch<$world, $component>;
            type Item<$world> = $item;

            #[inline]
            fn init_state(
                world: &mut World,
                terms: &mut QueryTerms,
            ) -> Result<Self::State, ParamError> {
                terms.field::<$component>(world, raw::TermAccess::$access)
            }

            #[inline]
            unsafe fn fetch<$world>(
                state: &$world Self::State,
                iter: *mut raw::Iter,
            ) -> Self::Fetch<$world> {
                field_fetch(iter, *state)
            }

            #[inline]
            fn all_owned(fetch: &Self::Fetch<'_>) -> bool {
                fetch.all_owned()
            }

            #[inline]
            unsafe fn item<$world, const $owned: bool>(
                $fetch: &mut Self::Fetch<$world>,
            ) -> Self::Item<$world> {
                $body
            }
        }
    };
}

impl_component_param!(
    <T> for<'world> &T => &'world T,
    In,
    |fetch, OWNED| field_ref::<T, OWNED>(fetch),
);

impl_component_param!(
    <T> for<'world> &mut T => &'world mut T,
    InOut,
    |fetch, OWNED| &mut *fetch.next::<OWNED>(),
);

impl_component_param!(
    <T> for<'world> Option<&T> => Option<&'world T>,
    InOptional,
    |fetch, OWNED| fetch.next_optional::<OWNED>().map(|ptr| &*ptr),
);

impl_component_param!(
    <T> for<'world> Option<&mut T> => Option<&'world mut T>,
    InOutOptional,
    |fetch, OWNED| fetch.next_optional::<OWNED>().map(|ptr| &mut *ptr),
);

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
    fn all_owned(_fetch: &Self::Fetch<'_>) -> bool {
        true
    }

    #[inline]
    unsafe fn item<'world, const OWNED: bool>(
        fetch: &mut Self::Fetch<'world>,
    ) -> Self::Item<'world> {
        let entity = Entity::from_raw(**fetch);
        *fetch = fetch.add(1);
        entity
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
            fn all_owned(fetch: &Self::Fetch<'_>) -> bool {
                true $(&& $name::all_owned(&fetch.$index))+
            }

            #[inline]
            unsafe fn item<'world, const OWNED: bool>(
                fetch: &mut Self::Fetch<'world>,
            ) -> Self::Item<'world> {
                ($($name::item::<OWNED>(&mut fetch.$index),)+)
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
