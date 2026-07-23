use core::mem::{align_of, size_of};

use siecs::raw;

fn assert_eq_type<T: Eq>() {}

#[test]
fn raw_compatibility_names_wrap_generated_types() {
    let term = raw::QueryTerm::default();
    let query = raw::QueryDesc::default();
    let world_features = raw::WorldFeatDesc::default();
    let system = raw::SystemDesc::default();

    assert_eq!(term.access, raw::TermAccess::In);
    assert_eq!(query.terms[0], term);
    assert_eq!(system.phase, raw::Phase::OnUpdate);
    assert_eq!(world_features.target_fps, 0);

    assert_eq_type::<raw::QueryTerm>();
    assert_eq_type::<raw::QueryDesc>();
    assert_eq_type::<raw::WorldFeatDesc>();
}

#[test]
fn compatibility_aliases_have_generated_layouts() {
    assert_eq!(
        size_of::<raw::ComponentDesc>(),
        size_of::<raw::ecs_component_desc_t>()
    );
    assert_eq!(
        align_of::<raw::ComponentDesc>(),
        align_of::<raw::ecs_component_desc_t>()
    );
    assert_eq!(
        size_of::<raw::SystemDesc>(),
        size_of::<raw::ecs_system_desc_t>()
    );
    assert_eq!(size_of::<raw::Iter>(), size_of::<raw::ecs_iter_t>());
}

#[test]
fn existing_raw_constants_keep_their_values() {
    assert_eq!(raw::ECS_ON_ADD, raw::EcsOnAdd as raw::EventId);
    assert_eq!(raw::ECS_ON_REMOVE, raw::EcsOnRemove as raw::EventId);
    assert_eq!(raw::ECS_ON_SET, raw::EcsOnSet as raw::EventId);
    assert_eq!(
        raw::ECS_RELATION_CASCADE_DELETE,
        raw::ecs_relation_flags_t_EcsRelationCascadeDelete
    );
}
