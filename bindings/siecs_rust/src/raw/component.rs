use core::ffi::{c_char, c_void};

use super::{world::WorldRaw, ComponentId, EntityId, SireflectStructDesc};

pub const ECS_RELATION_TARGET: u32 = 1 << 0;
pub const ECS_RELATION_SOURCE: u32 = 1 << 1;
pub const ECS_RELATION_CASCADE_DELETE: u32 = 1 << 2;
pub const ECS_RELATION_ONE_TO_ONE: u32 = 1 << 3;
pub const ECS_RELATION_ONE_TO_MANY: u32 = 1 << 4;

pub type ComponentOnAdd =
    Option<unsafe extern "C" fn(*mut WorldRaw, EntityId, ComponentId, *mut c_void)>;
pub type ComponentOnSet =
    Option<unsafe extern "C" fn(*mut WorldRaw, EntityId, ComponentId, *const c_void, *mut c_void)>;
pub type ComponentOnRemove =
    Option<unsafe extern "C" fn(*mut WorldRaw, EntityId, ComponentId, *mut c_void)>;
pub type TypeCtor = Option<unsafe extern "C" fn(*mut c_void, u32)>;
pub type TypeDtor = Option<unsafe extern "C" fn(*mut c_void, u32)>;
pub type TypeCopy = Option<unsafe extern "C" fn(*mut c_void, *const c_void, u32)>;
pub type TypeMove = Option<unsafe extern "C" fn(*mut c_void, *mut c_void, u32)>;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct TypeOps {
    pub ctor: TypeCtor,
    pub dtor: TypeDtor,
    pub copy_ctor: TypeCopy,
    pub copy: TypeCopy,
    pub move_ctor: TypeMove,
    pub move_: TypeMove,
}

#[repr(C)]
pub struct ComponentDesc {
    pub name: *const c_char,
    pub size: u64,
    pub ops: TypeOps,
    pub on_set: ComponentOnSet,
    pub on_remove: ComponentOnRemove,
    pub on_add: ComponentOnAdd,
    pub relation_flags: u32,
    pub struct_desc: *const SireflectStructDesc,
}

extern "C" {
    pub fn ecs_component_init(world: *mut WorldRaw, desc: *const ComponentDesc) -> ComponentId;
    pub fn ecs_component_register(
        world: *mut WorldRaw,
        id: *mut ComponentId,
        desc: *const ComponentDesc,
    ) -> ComponentId;

    pub fn ecs_add_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId);
    pub fn ecs_remove_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId);
    pub fn ecs_has_cid(world: *const WorldRaw, entity: EntityId, id: ComponentId) -> bool;
    pub fn ecs_has_cid_owned(world: *const WorldRaw, entity: EntityId, id: ComponentId) -> bool;
    pub fn ecs_get_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId) -> *mut c_void;
    pub fn ecs_try_get_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId) -> *mut c_void;
    pub fn ecs_set_cid(
        world: *mut WorldRaw,
        entity: EntityId,
        id: ComponentId,
        data: *const c_void,
    );
    pub fn ecs_move_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId, data: *mut c_void);
    pub fn ecs_with(world: *mut WorldRaw, component: ComponentId, require: ComponentId);
}
