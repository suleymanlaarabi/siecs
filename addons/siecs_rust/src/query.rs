use core::marker::PhantomData;

use crate::{raw, Component, World};

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
    pub fn each<F, Marker>(mut self, mut func: F)
    where
        F: QueryEach<Marker>,
    {
        F::append_terms(self.world, &mut self.desc, &mut self.term_index);
        validate_returned_fields(&self.desc);

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
    }

    #[inline]
    fn append_component<T: Component>(&mut self, access: raw::TermAccess) {
        let id = T::id(self.world);
        append_term(&mut self.desc, &mut self.term_index, id, access);
    }
}

#[doc(hidden)]
pub trait QueryEach<Marker> {
    fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16);
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

#[inline]
pub(crate) fn append_term(
    desc: &mut raw::QueryDesc,
    term_index: &mut u16,
    id: raw::ComponentId,
    access: raw::TermAccess,
) {
    assert!(
        (*term_index as usize) + 1 < desc.terms.len(),
        "too many query terms"
    );

    desc.terms[*term_index as usize] = raw::QueryTerm { id, access };
    *term_index += 1;
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

pub(crate) fn validate_returned_fields(desc: &raw::QueryDesc) {
    for (left_index, left) in desc.terms.iter().enumerate() {
        if left.id == 0 || !is_returned_field(left.access) {
            continue;
        }

        for right in desc.terms.iter().skip(left_index + 1) {
            if right.id == 0 {
                break;
            }

            assert!(
                !is_returned_field(right.access) || left.id != right.id,
                "query callback cannot request the same component field more than once"
            );
        }
    }
}

#[inline]
unsafe fn field<T>(iter: &mut raw::Iter, field_index: u16) -> *mut T {
    let ptr = raw::ecs_field(iter, field_index).cast::<T>();
    debug_assert!(!ptr.is_null());
    ptr
}

macro_rules! marker_ty {
    (ref $component:ident) => {
        Ref<$component>
    };
    (mut $component:ident) => {
        Mut<$component>
    };
}

macro_rules! marker_arg {
    (ref $component:ident) => {
        &$component
    };
    (mut $component:ident) => {
        &mut $component
    };
}

macro_rules! term_access {
    (ref) => {
        raw::TermAccess::In
    };
    (mut) => {
        raw::TermAccess::InOut
    };
}

macro_rules! row_arg {
    (ref $field:ident $row:ident) => {
        &*$field.add($row)
    };
    (mut $field:ident $row:ident) => {
        &mut *$field.add($row)
    };
}

macro_rules! impl_query_each {
    ($(($component:ident, $field:ident, $index:expr, $kind:ident)),+ $(,)?) => {
        impl<F, $($component),+> QueryEach<fn($(marker_ty!($kind $component)),+)> for F
        where
            F: FnMut($(marker_arg!($kind $component)),+),
            $($component: Component),+
        {
            #[inline]
            fn append_terms(world: &mut World, desc: &mut raw::QueryDesc, term_index: &mut u16) {
                $(
                    let id = $component::id(world);
                    append_term(desc, term_index, id, term_access!($kind));
                )+
            }

            #[inline]
            unsafe fn run(&mut self, iter: &mut raw::Iter) {
                $(
                    let $field = field::<$component>(iter, $index);
                )+

                for row in 0..iter.count as usize {
                    self($(row_arg!($kind $field row)),+);
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
    };
    (($($acc:tt)*) ($component:ident $field:ident $index:expr)) => {
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, ref),) ());
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, mut),) ());
    };
    (($($acc:tt)*) ($component:ident $field:ident $index:expr, $($rest:tt)+)) => {
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, ref),) ($($rest)+));
        impl_query_each_perms_inner!(($($acc)* ($component, $field, $index, mut),) ($($rest)+));
    };
}

impl_query_each_perms!(A a 0);
impl_query_each_perms!(A a 0, B b 1);
impl_query_each_perms!(A a 0, B b 1, C c 2);
impl_query_each_perms!(A a 0, B b 1, C c 2, D d 3);
impl_query_each_perms!(A a 0, B b 1, C c 2, D d 3, E e 4);
impl_query_each_perms!(A a 0, B b 1, C c 2, D d 3, E e 4, G g 5);
impl_query_each_perms!(A a 0, B b 1, C c 2, D d 3, E e 4, G g 5, H h 6);
impl_query_each_perms!(A a 0, B b 1, C c 2, D d 3, E e 4, G g 5, H h 6, I i 7);
