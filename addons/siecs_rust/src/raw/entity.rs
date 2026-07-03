use super::{world::WorldRaw, EntityId};

extern "C" {
    pub fn ecs_new(world: *mut WorldRaw) -> EntityId;
    pub fn ecs_is_alive(world: *const WorldRaw, entity: EntityId) -> bool;
    pub fn ecs_is(world: *mut WorldRaw, entity: EntityId, target: EntityId) -> bool;
    pub fn ecs_is_a(world: *mut WorldRaw, entity: EntityId, target: EntityId);
    pub fn ecs_kill(world: *mut WorldRaw, entity: EntityId);
}
