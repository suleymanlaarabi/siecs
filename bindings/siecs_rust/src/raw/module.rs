use core::ffi::c_void;

use super::world::WorldRaw;

pub type ModuleImport = unsafe extern "C" fn(*mut WorldRaw, *const c_void);
pub type ModuleDesc = super::generated::ecs_module_desc_t;

pub use super::generated::{
    ecs_module_disable, ecs_module_enable, ecs_module_find, ecs_module_init, ecs_module_is_enabled,
};
