use core::ffi::c_void;

pub type TermAccess = super::generated::ecs_term_access_t;
pub type QueryTerm = super::generated::ecs_query_term_t;
pub type QueryDesc = super::generated::ecs_query_desc_t;
pub type FieldKind = super::generated::ecs_field_kind_t;
pub type QueryCache = super::generated::ecs_query_cache_s;
pub type Iter = super::generated::ecs_iter_t;

impl Default for QueryTerm {
    fn default() -> Self {
        Self {
            id: 0,
            access: TermAccess::In,
        }
    }
}

impl PartialEq for QueryTerm {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id && self.access == other.access
    }
}

impl Eq for QueryTerm {}

impl Default for QueryDesc {
    fn default() -> Self {
        Self {
            terms: [QueryTerm::default(); 64],
            is_a: 0,
        }
    }
}

impl PartialEq for QueryDesc {
    fn eq(&self, other: &Self) -> bool {
        self.terms == other.terms && self.is_a == other.is_a
    }
}

impl Eq for QueryDesc {}

pub use super::generated::{ecs_iter_next, ecs_query_fini, ecs_query_init, ecs_query_iter};

#[inline]
pub unsafe fn ecs_field(it: *mut Iter, field_index: u16) -> *mut c_void {
    let field = *(*it).ptrs.add(field_index as usize);

    if *(*it).field_kinds.add(field_index as usize) == FieldKind::Owned {
        *(field as *mut *mut c_void)
    } else {
        field
    }
}

#[inline]
pub unsafe fn ecs_field_kind(it: *const Iter, field_index: u16) -> FieldKind {
    *(*it).field_kinds.add(field_index as usize)
}

#[inline]
pub unsafe fn ecs_field_is_shared(it: *const Iter, field_index: u16) -> bool {
    ecs_field_kind(it, field_index) == FieldKind::Shared
}
