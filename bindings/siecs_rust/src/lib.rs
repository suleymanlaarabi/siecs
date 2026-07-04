extern crate self as siecs;

pub mod component;
pub mod entity;
pub mod module;
pub mod observer;
#[doc(hidden)]
pub mod private;
pub mod query;
pub mod raw;
pub mod resource;
pub mod system;
pub mod world;
pub mod world_ref;

pub use component::{Abstract, ChildOf, Component, Disabled, Name};
pub use entity::Entity;
pub use module::{Module, ModuleId};
pub use observer::{
    Event, EventId, Observer, ObserverEvent, ObserverId, OnAdd, OnRemove, OnSet, RawEvent,
    TypedEvent,
};
pub use query::{
    Field, FieldKind, ParamError, Query, QueryFilter, QueryParam, QueryState, With, Without,
};
pub use resource::{Res, ResMut, Resource, ResourceId};
pub use siecs_derive::{Component, Event, Resource};
pub use system::{
    Commands, EachCtx, IntoSystem, Phase, System, SystemContext, SystemDescBuilder, SystemId,
    SystemParam,
};
pub use world::{DeferGuard, World, WorldFeatures};
pub use world_ref::WorldRef;

pub mod prelude {
    pub use crate::{
        Abstract, ChildOf, Commands, Component, DeferGuard, Disabled, EachCtx, Entity, Event,
        EventId, Field, FieldKind, IntoSystem, Module, ModuleId, Name, Observer, ObserverEvent,
        ObserverId, OnAdd, OnRemove, OnSet, ParamError, Phase, Query, QueryFilter, QueryParam,
        QueryState, RawEvent, Res, ResMut, Resource, ResourceId, System, SystemContext,
        SystemDescBuilder, SystemId, SystemParam, TypedEvent, With, Without, World, WorldFeatures,
        WorldRef,
    };
}
