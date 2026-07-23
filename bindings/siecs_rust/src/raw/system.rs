use super::query::Iter;

pub type Phase = super::generated::ecs_phase_t;
pub type SystemCallback = unsafe extern "C" fn(*mut Iter);
pub type SystemUserDataDtor = unsafe extern "C" fn(usize);
pub type SystemDesc = super::generated::ecs_system_desc_t;

impl Default for SystemDesc {
    fn default() -> Self {
        Self {
            name: core::ptr::null(),
            query: Default::default(),
            callback: None,
            user_data: 0,
            user_data_dtor: None,
            phase: Phase::OnUpdate,
            after: [0; 16],
            disabled: false,
        }
    }
}

pub use super::generated::{
    ecs_progress, ecs_run_phase, ecs_run_system, ecs_system_disable, ecs_system_enable,
    ecs_system_init,
};
