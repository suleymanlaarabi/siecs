use core::marker::PhantomData;
use core::ops::Deref;

use crate::{raw, Component, EachCtx, Entity, Res, ResMut, Resource, World};

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
                "query callback cannot request the same component field more than once"
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

pub struct Query<'world> {
    world: &'world mut World,
    desc: raw::QueryDesc,
    term_index: u16,
}

impl<'world> Query<'world> {
    #[inline]
    pub(crate) fn new(world: &'world mut World) -> Self {
        Self {
            world,
            desc: raw::QueryDesc::default(),
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
        self.desc.is_a = base.id();
        self
    }

    #[inline]
    pub fn each<F, Marker>(self, func: F)
    where
        F: QueryEach<Marker>,
    {
        self.try_each(func).unwrap_or_else(|err| panic!("{err}"));
    }

    #[inline]
    pub fn try_each<F, Marker>(mut self, mut func: F) -> Result<(), ParamError>
    where
        F: QueryEach<Marker>,
    {
        F::append_terms(self.world, &mut self.desc, &mut self.term_index)?;
        let mut resource_access = ResourceAccess::default();
        F::validate_resources(self.world, &mut resource_access)?;
        validate_returned_fields(&self.desc)?;

        let query = QueryHandle {
            world: self.world.as_raw_mut(),
            id: unsafe { raw::ecs_query_init(self.world.as_raw_mut(), &self.desc) as raw::QueryId },
        };
        let mut iter = unsafe { raw::ecs_query_iter(query.world, query.id) };

        while unsafe { raw::ecs_iter_next(&mut iter) } {
            unsafe {
                func.run(&mut iter);
            }
        }

        Ok(())
    }

    #[inline]
    fn append_component<T: Component>(&mut self, access: raw::TermAccess) {
        let id = T::id(self.world);
        append_term(&mut self.desc, &mut self.term_index, id, access)
            .expect("too many query terms");
    }
}

#[doc(hidden)]
pub trait QueryEach<Marker> {
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
    unsafe fn run(&mut self, iter: &mut raw::Iter);
}

struct QueryHandle {
    world: *mut raw::WorldRaw,
    id: raw::QueryId,
}

impl Drop for QueryHandle {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            raw::ecs_query_fini(self.world, self.id);
        }
    }
}

#[doc(hidden)]
pub struct Ref<T>(PhantomData<T>);

#[doc(hidden)]
pub struct Mut<T>(PhantomData<T>);

#[doc(hidden)]
pub struct OptRef<T>(PhantomData<T>);

#[doc(hidden)]
pub struct OptMut<T>(PhantomData<T>);

#[doc(hidden)]
pub struct FieldRef<T>(PhantomData<T>);

#[doc(hidden)]
pub struct OptFieldRef<T>(PhantomData<T>);

#[doc(hidden)]
pub struct QueryRes<T>(PhantomData<T>);

#[doc(hidden)]
pub struct QueryResMut<T>(PhantomData<T>);

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
pub(crate) fn resource_ref<'a, T: Resource>(world: *mut raw::WorldRaw) -> Res<'a, T> {
    let id = unsafe { T::id_raw(world) };
    unsafe { Res::new(&*raw::ecs_resource_rid(world, id).cast::<T>()) }
}

#[inline]
pub(crate) fn resource_mut<'a, T: Resource>(world: *mut raw::WorldRaw) -> ResMut<'a, T> {
    let id = unsafe { T::id_raw(world) };
    unsafe { ResMut::new(&mut *raw::ecs_resource_rid(world, id).cast::<T>()) }
}

macro_rules! marker_ty {
    (ref $component:ident) => {
        Ref<$component>
    };
    (mut $component:ident) => {
        Mut<$component>
    };
    (opt_ref $component:ident) => {
        OptRef<$component>
    };
    (opt_mut $component:ident) => {
        OptMut<$component>
    };
    (field_ref $component:ident) => {
        FieldRef<$component>
    };
    (opt_field_ref $component:ident) => {
        OptFieldRef<$component>
    };
}

macro_rules! marker_arg {
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
    (field_ref $component:ident) => {
        Field<'_, $component>
    };
    (opt_field_ref $component:ident) => {
        Option<Field<'_, $component>>
    };
}

macro_rules! term_access {
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
    (field_ref) => {
        raw::TermAccess::In
    };
    (opt_field_ref) => {
        raw::TermAccess::InOptional
    };
}

macro_rules! row_arg {
    (ref $field:ident $row:ident) => {
        if $field.1 == raw::FieldKind::Shared {
            &*$field.0
        } else {
            &*$field.0.add($row)
        }
    };
    (mut $field:ident $row:ident) => {
        &mut *$field.0.add($row)
    };
    (opt_ref $field:ident $row:ident) => {
        if $field.0.is_null() {
            None
        } else if $field.1 == raw::FieldKind::Shared {
            Some(&*$field.0)
        } else {
            Some(&*$field.0.add($row))
        }
    };
    (opt_mut $field:ident $row:ident) => {
        if $field.0.is_null() {
            None
        } else {
            Some(&mut *$field.0.add($row))
        }
    };
    (field_ref $field:ident $row:ident) => {
        Field::new(row_arg!(ref $field $row), $field.1)
    };
    (opt_field_ref $field:ident $row:ident) => {
        if $field.0.is_null() {
            None
        } else {
            Some(Field::new(
                if $field.1 == raw::FieldKind::Shared {
                    &*$field.0
                } else {
                    &*$field.0.add($row)
                },
                $field.1,
            ))
        }
    };
}

impl<F, R> QueryEach<fn(QueryRes<R>)> for F
where
    F: FnMut(Res<'_, R>),
    R: Resource,
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
        access.read::<R>(world)
    }

    #[inline]
    unsafe fn run(&mut self, iter: &mut raw::Iter) {
        for _row in 0..iter.count as usize {
            self(resource_ref::<R>(iter.world));
        }
    }
}

impl<F, R> QueryEach<fn(QueryResMut<R>)> for F
where
    F: FnMut(ResMut<'_, R>),
    R: Resource,
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
        access.write::<R>(world)
    }

    #[inline]
    unsafe fn run(&mut self, iter: &mut raw::Iter) {
        for _row in 0..iter.count as usize {
            self(resource_mut::<R>(iter.world));
        }
    }
}

impl<F, A, B> QueryEach<fn(QueryRes<A>, QueryResMut<B>)> for F
where
    F: FnMut(Res<'_, A>, ResMut<'_, B>),
    A: Resource,
    B: Resource,
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
        access.read::<A>(world)?;
        access.write::<B>(world)?;
        Ok(())
    }

    #[inline]
    unsafe fn run(&mut self, iter: &mut raw::Iter) {
        for _row in 0..iter.count as usize {
            self(resource_ref::<A>(iter.world), resource_mut::<B>(iter.world));
        }
    }
}

impl<F, A, B> QueryEach<fn(QueryResMut<A>, QueryResMut<B>)> for F
where
    F: FnMut(ResMut<'_, A>, ResMut<'_, B>),
    A: Resource,
    B: Resource,
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
        access.write::<A>(world)?;
        access.write::<B>(world)?;
        Ok(())
    }

    #[inline]
    unsafe fn run(&mut self, iter: &mut raw::Iter) {
        for _row in 0..iter.count as usize {
            self(resource_mut::<A>(iter.world), resource_mut::<B>(iter.world));
        }
    }
}

impl<F, A, R> QueryEach<fn(Entity, Ref<A>, QueryRes<R>)> for F
where
    F: FnMut(Entity, &A, Res<'_, R>),
    A: Component,
    R: Resource,
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
    fn validate_resources(
        world: &mut World,
        access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        access.read::<R>(world)
    }

    #[inline]
    unsafe fn run(&mut self, iter: &mut raw::Iter) {
        let field = (
            raw::ecs_field(iter, 0).cast::<A>(),
            raw::ecs_field_kind(iter, 0),
        );

        for row in 0..iter.count as usize {
            self(
                Entity::from_raw(*iter.entities.add(row)),
                row_arg!(ref field row),
                resource_ref::<R>(iter.world),
            );
        }
    }
}

impl<F, A, B, R> QueryEach<fn(Entity, FieldRef<A>, OptFieldRef<B>, QueryRes<R>)> for F
where
    F: FnMut(Entity, Field<'_, A>, Option<Field<'_, B>>, Res<'_, R>),
    A: Component,
    B: Component,
    R: Resource,
{
    #[inline]
    fn append_terms(
        world: &mut World,
        desc: &mut raw::QueryDesc,
        term_index: &mut u16,
    ) -> Result<(), ParamError> {
        let id = A::id(world);
        append_term(desc, term_index, id, raw::TermAccess::In)?;
        let id = B::id(world);
        append_term(desc, term_index, id, raw::TermAccess::InOptional)
    }

    #[inline]
    fn validate_resources(
        world: &mut World,
        access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        access.read::<R>(world)
    }

    #[inline]
    unsafe fn run(&mut self, iter: &mut raw::Iter) {
        let a = (
            raw::ecs_field(iter, 0).cast::<A>(),
            raw::ecs_field_kind(iter, 0),
        );
        let b = (
            raw::ecs_field(iter, 1).cast::<B>(),
            raw::ecs_field_kind(iter, 1),
        );

        for row in 0..iter.count as usize {
            self(
                Entity::from_raw(*iter.entities.add(row)),
                row_arg!(field_ref a row),
                row_arg!(opt_field_ref b row),
                resource_ref::<R>(iter.world),
            );
        }
    }
}

impl<F, A, B, R> QueryEach<fn(EachCtx<'_>, Mut<A>, Ref<B>, QueryRes<R>)> for F
where
    F: FnMut(EachCtx<'_>, &mut A, &B, Res<'_, R>),
    A: Component,
    B: Component,
    R: Resource,
{
    #[inline]
    fn append_terms(
        world: &mut World,
        desc: &mut raw::QueryDesc,
        term_index: &mut u16,
    ) -> Result<(), ParamError> {
        let id = A::id(world);
        append_term(desc, term_index, id, raw::TermAccess::InOut)?;
        let id = B::id(world);
        append_term(desc, term_index, id, raw::TermAccess::In)
    }

    #[inline]
    fn validate_resources(
        world: &mut World,
        access: &mut ResourceAccess,
    ) -> Result<(), ParamError> {
        access.read::<R>(world)
    }

    #[inline]
    unsafe fn run(&mut self, iter: &mut raw::Iter) {
        let a = (
            raw::ecs_field(iter, 0).cast::<A>(),
            raw::ecs_field_kind(iter, 0),
        );
        let b = (
            raw::ecs_field(iter, 1).cast::<B>(),
            raw::ecs_field_kind(iter, 1),
        );

        for row in 0..iter.count as usize {
            self(
                EachCtx::new(iter, row),
                row_arg!(mut a row),
                row_arg!(ref b row),
                resource_ref::<R>(iter.world),
            );
        }
    }
}

macro_rules! impl_query_each {
    ($(($component:ident, $field:ident, $index:expr, $kind:ident)),+ $(,)?) => {
        impl<F, $($component),+> QueryEach<fn($(marker_ty!($kind $component)),+)> for F
        where
            F: FnMut($(marker_arg!($kind $component)),+),
            $($component: Component),+
        {
            #[inline]
            fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16) -> Result<(), ParamError> {
                $(
                    let id = $component::id(world);
                    append_term(desc, term_index, id, term_access!($kind))?;
                )+
                Ok(())
            }

            #[inline]
            unsafe fn run(&mut self, iter: &mut raw::Iter) {
                $(
                    let $field = (
                        raw::ecs_field(iter, $index).cast::<$component>(),
                        raw::ecs_field_kind(iter, $index),
                    );
                )+

                for row in 0..iter.count as usize {
                    self($(row_arg!($kind $field row)),+);
                }
            }
        }
    };
}

macro_rules! impl_query_each_ctx {
    ($(($component:ident, $field:ident, $index:expr, $kind:ident)),+ $(,)?) => {
        impl<F, $($component),+> QueryEach<fn(EachCtx<'_>, $(marker_ty!($kind $component)),+)> for F
        where
            F: FnMut(EachCtx<'_>, $(marker_arg!($kind $component)),+),
            $($component: Component),+
        {
            #[inline]
            fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16) -> Result<(), ParamError> {
                $(
                    let id = $component::id(world);
                    append_term(desc, term_index, id, term_access!($kind))?;
                )+
                Ok(())
            }

            #[inline]
            unsafe fn run(&mut self, iter: &mut raw::Iter) {
                $(
                    let $field = (
                        raw::ecs_field(iter, $index).cast::<$component>(),
                        raw::ecs_field_kind(iter, $index),
                    );
                )+

                for row in 0..iter.count as usize {
                    self(EachCtx::new(iter, row), $(row_arg!($kind $field row)),+);
                }
            }
        }
    };
}

macro_rules! impl_query_each_perms {
    ($($component:ident $field:ident $index:expr),+ $(,)?) => {
        impl_query_each_perms_inner!(() ($($component $field $index),+));
    };
}

macro_rules! impl_query_each_perms_inner {
    (($($acc:tt)*) ()) => {
        impl_query_each!($($acc)*);
        impl_query_each_ctx!($($acc)*);
    };
    (($($acc:tt)*) ($component:ident $field:ident $index:expr)) => {
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, ref),) ());
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, mut),) ());
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, opt_ref),) ());
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, opt_mut),) ());
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, field_ref),) ());
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, opt_field_ref),) ());
    };
    (($($acc:tt)*) ($component:ident $field:ident $index:expr, $($rest:tt)+)) => {
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, ref),) ($($rest)+));
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, mut),) ($($rest)+));
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, opt_ref),) ($($rest)+));
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, opt_mut),) ($($rest)+));
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, field_ref),) ($($rest)+));
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, opt_field_ref),) ($($rest)+));
    };
}

macro_rules! impl_query_each_rw_perms {
    ($($component:ident $field:ident $index:expr),+ $(,)?) => {
        impl_query_each_rw_perms_inner!(() ($($component $field $index),+));
    };
}

macro_rules! impl_query_each_rw_perms_inner {
    (($($acc:tt)*) ()) => {
        impl_query_each!($($acc)*);
    };
    (($($acc:tt)*) ($component:ident $field:ident $index:expr)) => {
        impl_query_each_rw_perms_inner!(($($acc)* ($component, $field, $index, ref),) ());
        impl_query_each_rw_perms_inner!(($($acc)* ($component, $field, $index, mut),) ());
    };
    (($($acc:tt)*) ($component:ident $field:ident $index:expr, $($rest:tt)+)) => {
        impl_query_each_rw_perms_inner!(($($acc)* ($component, $field, $index, ref),) ($($rest)+));
        impl_query_each_rw_perms_inner!(($($acc)* ($component, $field, $index, mut),) ($($rest)+));
    };
}

macro_rules! impl_query_each_perms_tuple {
    ($(($index:tt, $component:ident, $field:ident)),+ $(,)?) => {
        impl_query_each_perms!($($component $field $index),+);
    };
}

macro_rules! impl_query_each_rw_perms_tuple {
    ($(($index:tt, $component:ident, $field:ident)),+ $(,)?) => {
        impl_query_each_rw_perms!($($component $field $index),+);
    };
}

variadics_please::all_tuples_enumerated!(impl_query_each_perms_tuple, 1, 4, A, a);
variadics_please::all_tuples_enumerated!(impl_query_each_rw_perms_tuple, 5, 8, A, a);
