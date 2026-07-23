#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]

pub mod component;
pub mod entity;
pub mod generated;
pub mod module;
pub mod observer;
pub mod query;
pub mod resource;
pub mod system;
pub mod world;

pub use generated::*;

pub type SireflectStructDesc = sireflect_struct_desc_t;
pub type EntityId = ecs_entity_t;
pub type ComponentId = ecs_component_t;
pub type QueryId = ecs_query_id_t;
pub type SystemId = ecs_system_id_t;
pub type EventId = ecs_event_t;
pub type ModuleId = ecs_module_id_t;
pub type ResourceId = ecs_resource_t;
pub type ObserverId = ecs_observer_id_t;

pub use component::{
    ComponentDesc, ComponentOnAdd, ComponentOnRemove, ComponentOnSet, TypeOps,
    ECS_RELATION_CASCADE_DELETE, ECS_RELATION_ONE_TO_MANY, ECS_RELATION_ONE_TO_ONE,
    ECS_RELATION_SOURCE, ECS_RELATION_TARGET,
};
pub use module::{ModuleDesc, ModuleImport};
pub use observer::{
    ObserverCallback, ObserverDesc, ObserverEvent, ECS_ON_ADD, ECS_ON_REMOVE, ECS_ON_SET,
};
pub use query::{
    ecs_field, ecs_field_is_shared, ecs_field_kind, FieldKind, Iter, QueryDesc, QueryTerm,
    TermAccess,
};
pub use resource::{ResourceDesc, ResourceHook};
pub use system::{Phase, SystemCallback, SystemDesc, SystemUserDataDtor};
pub use world::{WorldFeatDesc, WorldRaw};
