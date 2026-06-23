use core::ffi::c_char;

use super::{query::Iter, query::QueryDesc, world::WorldRaw, SystemId};

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Phase {
    PreStart,
    Start,
    PostStart,
    OnLoad,
    PostLoad,
    PreUpdate,
    OnUpdate,
    PostUpdate,
    PreRender,
    OnRender,
    PostRender,
    PhaseCount,
}

pub type SystemCallback = unsafe extern "C" fn(*mut Iter);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SystemDesc {
    pub name: *const c_char,
    pub query: QueryDesc,
    pub callback: Option<SystemCallback>,
    pub phase: Phase,
    pub after: [SystemId; 4],
    pub disabled: bool,
}

impl Default for SystemDesc {
    #[inline]
    fn default() -> Self {
        Self {
            name: core::ptr::null(),
            query: QueryDesc::default(),
            callback: None,
            phase: Phase::OnUpdate,
            after: [0; 4],
            disabled: false,
        }
    }
}

extern "C" {
    pub fn ecs_system_init(world: *mut WorldRaw, system: *const SystemDesc) -> SystemId;
    pub fn ecs_progress(world: *mut WorldRaw) -> bool;
    pub fn ecs_run_phase(world: *mut WorldRaw, phase: Phase);
    pub fn ecs_run_system(world: *mut WorldRaw, system: SystemId);
    pub fn ecs_system_enable(world: *mut WorldRaw, system: SystemId);
    pub fn ecs_system_disable(world: *mut WorldRaw, system: SystemId);
}
