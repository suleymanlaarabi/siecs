pub enum WorldRaw {}

extern "C" {
    pub fn ecs_init() -> *mut WorldRaw;
    pub fn ecs_fini(world: *mut WorldRaw);
}
