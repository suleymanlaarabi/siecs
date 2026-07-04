#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]

pub mod component;
pub mod entity;
pub mod module;
pub mod observer;
pub mod query;
pub mod resource;
pub mod system;
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
    ecs_add_cid, ecs_component_init, ecs_component_register, ecs_get_cid, ecs_has_cid,
    ecs_has_cid_owned, ecs_remove_cid, ecs_set_cid, ecs_try_get_cid, ecs_with, ComponentDesc,
    ComponentOnAdd, ComponentOnRemove, ComponentOnSet, ECS_RELATION_CASCADE_DELETE,
    ECS_RELATION_ONE_TO_MANY, ECS_RELATION_ONE_TO_ONE, ECS_RELATION_SOURCE, ECS_RELATION_TARGET,
};
pub use entity::{ecs_is, ecs_is_a, ecs_is_alive, ecs_kill, ecs_new};
pub use module::{
    ecs_module_disable, ecs_module_enable, ecs_module_find, ecs_module_init, ecs_module_is_enabled,
    ModuleDesc, ModuleImport,
};
pub use observer::{
    ecs_event, ecs_event_register, ecs_observer_disable, ecs_observer_enable, ecs_observer_init,
    ecs_observer_trigger, ObserverCallback, ObserverDesc, ObserverEvent, ECS_ON_ADD, ECS_ON_REMOVE,
    ECS_ON_SET,
};
pub use query::{
    ecs_field, ecs_field_is_shared, ecs_field_kind, ecs_iter_next, ecs_query_fini, ecs_query_init,
    ecs_query_iter, FieldKind, Iter, QueryDesc, QueryTerm, TermAccess,
};
pub use resource::{
    ecs_has_resource_rid, ecs_remove_resource_rid, ecs_resource_find, ecs_resource_init,
    ecs_resource_is_registered_rid, ecs_resource_register, ecs_resource_rid, ecs_set_resource_rid,
    ecs_try_resource_rid, ResourceDesc, ResourceHook,
};
pub use system::{
    ecs_progress, ecs_run_phase, ecs_run_system, ecs_system_disable, ecs_system_enable,
    ecs_system_init, Phase, SystemCallback, SystemDesc,
};
pub use world::{
    ecs_defer_begin, ecs_defer_end, ecs_fini, ecs_init, ecs_init_w_features, ecs_is_deferred,
    WorldFeatDesc, WorldRaw,
};
