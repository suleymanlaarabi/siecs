use siecs::{private::StaticModuleId, Module, Resource, World, WorldRef};

#[derive(Resource)]
struct ModuleValue {
    value: i32,
}

struct TestModule;

struct TestProps {
    value: i32,
}

static TEST_MODULE_ID: StaticModuleId = StaticModuleId::new();

impl Module for TestModule {
    type Props = TestProps;

    const NAME: &'static [u8] = b"TestModule\0";

    fn import(world: WorldRef<'_>, props: &Self::Props) {
        world.set_resource(ModuleValue { value: props.value });
    }

    fn id_storage() -> *mut siecs::raw::ModuleId {
        TEST_MODULE_ID.as_mut_ptr()
    }
}

#[test]
fn module_import_find_enable_disable() {
    let mut world = World::new();

    assert!(world.find_module::<TestModule>().is_none());
    let module = world.import::<TestModule>(&TestProps { value: 12 });
    assert_eq!(world.find_module::<TestModule>(), Some(module));
    assert!(world.module_is_enabled(module));
    assert_eq!(world.resource::<ModuleValue>().value, 12);

    world.disable_module(module);
    assert!(!world.module_is_enabled(module));
    world.enable_module(module);
    assert!(world.module_is_enabled(module));
}
