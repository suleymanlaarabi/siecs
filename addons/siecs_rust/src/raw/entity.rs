use core::ffi::c_int;

use super::{world::WorldRaw, EntityId};

extern "C" {
    pub fn ecs_new(world: *mut WorldRaw) -> EntityId;
    pub fn ecs_is_alive(world: *const WorldRaw, entity: EntityId) -> c_int;
    pub fn ecs_kill(world: *mut WorldRaw, entity: EntityId);
}
