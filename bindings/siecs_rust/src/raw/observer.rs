use super::EventId;

pub const ECS_ON_ADD: EventId = super::generated::EcsOnAdd as EventId;
pub const ECS_ON_REMOVE: EventId = super::generated::EcsOnRemove as EventId;
pub const ECS_ON_SET: EventId = super::generated::EcsOnSet as EventId;

pub type ObserverEvent = super::generated::ecs_observer_event_t;
pub type ObserverCallback = unsafe extern "C" fn(*mut ObserverEvent);
pub type ObserverDesc = super::generated::ecs_observer_desc_t;

#[allow(clippy::derivable_impls)]
impl Default for ObserverDesc {
    fn default() -> Self {
        Self {
            on: 0,
            query: Default::default(),
            callback: None,
            user_data: 0,
        }
    }
}

pub use super::generated::{
    ecs_event, ecs_event_register, ecs_observer_disable, ecs_observer_enable, ecs_observer_init,
    ecs_observer_trigger,
};
