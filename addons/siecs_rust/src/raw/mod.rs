#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]

pub mod component;
pub mod entity;
pub mod query;
pub mod world;

pub enum SireflectStructDesc {}

pub type EntityId = u64;
pub type ComponentId = u16;
pub type QueryId = u16;
pub type SystemId = u16;
pub type EventId = u16;
pub type ModuleId = u16;
pub type ResourceId = u16;
pub type ObserverId = u32;

pub use component::{
    ecs_add_cid, ecs_component_init, ecs_get_cid, ecs_has_cid, ecs_remove_cid, ecs_set_cid,
    ecs_try_get_cid, ComponentDesc, ComponentOnAdd, ComponentOnRemove, ComponentOnSet,
};
pub use entity::{ecs_is_alive, ecs_kill, ecs_new};
pub use query::{
    ecs_field, ecs_iter_next, ecs_query_fini, ecs_query_init, ecs_query_iter, Iter, QueryDesc,
    QueryTerm, TermAccess,
};
pub use world::{ecs_fini, ecs_init, WorldRaw};
