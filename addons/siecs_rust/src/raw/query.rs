use core::ffi::c_void;

use super::{world::WorldRaw, ComponentId, EntityId, QueryId};

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TermAccess {
    In,
    Out,
    InOut,
    InOptional,
    InOutOptional,
    Filter,
    Not,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct QueryTerm {
    pub id: ComponentId,
    pub access: TermAccess,
}

impl Default for QueryTerm {
    #[inline]
    fn default() -> Self {
        Self {
            id: 0,
            access: TermAccess::In,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct QueryDesc {
    pub terms: [QueryTerm; 16],
    pub is_a: EntityId,
}

impl Default for QueryDesc {
    #[inline]
    fn default() -> Self {
        Self {
            terms: [QueryTerm::default(); 16],
            is_a: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FieldKind {
    None,
    Owned,
    Shared,
}

#[repr(C)]
pub struct QueryCache {
    _private: [u8; 0],
}

#[repr(C)]
pub struct Iter {
    pub world: *mut WorldRaw,
    pub count: u32,
    pub entities: *mut EntityId,
    pub cache: *mut QueryCache,
    pub ptrs: *mut *mut c_void,
    pub field_kinds: *mut FieldKind,
    pub table_idx: u16,
    pub table_count: u16,
}

extern "C" {
    pub fn ecs_query_init(world: *mut WorldRaw, query: *const QueryDesc) -> u32;
    pub fn ecs_query_fini(world: *mut WorldRaw, query: QueryId);
    pub fn ecs_query_iter(world: *mut WorldRaw, query_id: QueryId) -> Iter;
    pub fn ecs_iter_next(it: *mut Iter) -> bool;
}

#[inline]
pub unsafe fn ecs_field(it: *mut Iter, field_index: u16) -> *mut c_void {
    let field = *(*it).ptrs.add(field_index as usize);

    if *(*it).field_kinds.add(field_index as usize) == FieldKind::Owned {
        *(field as *mut *mut c_void)
    } else {
        field
    }
}
