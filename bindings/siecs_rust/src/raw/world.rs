use core::ffi::c_void;

pub enum WorldRaw {}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct WorldFeatDesc {
    pub rest: bool,
    pub target_fps: u16,
}

extern "C" {
    pub fn ecs_init() -> *mut WorldRaw;
    pub fn ecs_init_w_features(features: *const WorldFeatDesc) -> *mut WorldRaw;
    pub fn ecs_fini(world: *mut WorldRaw);

    pub fn ecs_defer_begin(world: *mut WorldRaw);
    pub fn ecs_defer_end(world: *mut WorldRaw);
    pub fn ecs_is_deferred(world: *const WorldRaw) -> bool;
}

pub type OpaquePtr = *mut c_void;
