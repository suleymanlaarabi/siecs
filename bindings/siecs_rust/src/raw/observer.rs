use core::ffi::c_void;

use super::{query::QueryDesc, world::WorldRaw, EntityId, EventId, ObserverId};

pub const ECS_ON_ADD: EventId = 0;
pub const ECS_ON_REMOVE: EventId = 1;
pub const ECS_ON_SET: EventId = 2;

#[repr(C)]
pub struct ObserverEvent {
    pub world: *mut WorldRaw,
    pub entity: EntityId,
    pub event: EventId,
    pub user_data: usize,
    pub trigger_data: *const c_void,
}

pub type ObserverCallback = unsafe extern "C" fn(*mut ObserverEvent);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ObserverDesc {
    pub on: EventId,
    pub query: QueryDesc,
    pub callback: Option<ObserverCallback>,
    pub user_data: usize,
}

impl Default for ObserverDesc {
    #[inline]
    fn default() -> Self {
        Self {
            on: 0,
            query: QueryDesc::default(),
            callback: None,
            user_data: 0,
        }
    }
}

extern "C" {
    pub fn ecs_event(world: *mut WorldRaw) -> EventId;
    pub fn ecs_event_register(world: *mut WorldRaw, id: *mut EventId) -> EventId;
    pub fn ecs_observer_init(world: *mut WorldRaw, desc: *const ObserverDesc) -> ObserverId;
    pub fn ecs_observer_enable(world: *mut WorldRaw, id: ObserverId);
    pub fn ecs_observer_disable(world: *mut WorldRaw, id: ObserverId);
    pub fn ecs_observer_trigger(
        world: *mut WorldRaw,
        entity: EntityId,
        event: EventId,
        trigger_data: *const c_void,
    );
}
