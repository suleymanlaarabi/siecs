use core::ffi::c_void;

pub type WorldRaw = super::generated::ecs_world_t;
pub type WorldFeatDesc = super::generated::ecs_world_feat_desc_t;

#[allow(clippy::derivable_impls)]
impl Default for WorldFeatDesc {
    fn default() -> Self {
        Self {
            rest: false,
            target_fps: 0,
        }
    }
}

impl PartialEq for WorldFeatDesc {
    fn eq(&self, other: &Self) -> bool {
        self.rest == other.rest && self.target_fps == other.target_fps
    }
}

impl Eq for WorldFeatDesc {}

pub use super::generated::{
    ecs_defer_begin, ecs_defer_end, ecs_fini, ecs_init, ecs_init_w_features, ecs_is_deferred,
};

pub type OpaquePtr = *mut c_void;
