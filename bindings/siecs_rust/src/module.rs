use core::ffi::c_void;
use core::mem::size_of;

use crate::{raw, World, WorldRef};

pub trait Module: 'static {
    type Props;

    const NAME: &'static [u8];

    fn import(world: WorldRef<'_>, props: &Self::Props);
    fn id_storage() -> *mut raw::ModuleId;
}

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[repr(transparent)]
pub struct ModuleId(raw::ModuleId);

impl ModuleId {
    #[inline]
    pub const fn raw(self) -> raw::ModuleId {
        self.0
    }
}

impl From<raw::ModuleId> for ModuleId {
    #[inline]
    fn from(id: raw::ModuleId) -> Self {
        Self(id)
    }
}

#[inline]
pub(crate) fn module_desc<M: Module>(props: &M::Props, disabled: bool) -> raw::ModuleDesc {
    assert!(
        M::NAME.last().copied() == Some(0),
        "module name must be NUL terminated"
    );

    raw::ModuleDesc {
        name: M::NAME.as_ptr().cast(),
        id: M::id_storage(),
        import: Some(module_import::<M>),
        desc: (props as *const M::Props).cast::<c_void>(),
        desc_size: size_of::<M::Props>() as u32,
        disabled,
    }
}

unsafe extern "C" fn module_import<M: Module>(world: *mut raw::WorldRaw, desc: *const c_void) {
    debug_assert!(!world.is_null());
    debug_assert!(!desc.is_null());

    M::import(WorldRef::from_raw(world), &*desc.cast::<M::Props>());
}

impl World {
    #[inline]
    pub fn import<M: Module>(&mut self, props: &M::Props) -> ModuleId {
        let desc = module_desc::<M>(props, false);
        unsafe { raw::ecs_module_init(self.as_raw_mut(), &desc).into() }
    }

    #[inline]
    pub fn import_disabled<M: Module>(&mut self, props: &M::Props) -> ModuleId {
        let desc = module_desc::<M>(props, true);
        unsafe { raw::ecs_module_init(self.as_raw_mut(), &desc).into() }
    }

    #[inline]
    pub fn find_module<M: Module>(&mut self) -> Option<ModuleId> {
        let id = unsafe { raw::ecs_module_find(self.as_raw_mut(), M::id_storage()) };
        (id != 0).then_some(id.into())
    }

    #[inline]
    pub fn enable_module(&mut self, module: ModuleId) {
        unsafe { raw::ecs_module_enable(self.as_raw_mut(), module.raw()) }
    }

    #[inline]
    pub fn disable_module(&mut self, module: ModuleId) {
        unsafe { raw::ecs_module_disable(self.as_raw_mut(), module.raw()) }
    }

    #[inline]
    pub fn module_is_enabled(&self, module: ModuleId) -> bool {
        unsafe { raw::ecs_module_is_enabled(self.as_raw(), module.raw()) }
    }
}
