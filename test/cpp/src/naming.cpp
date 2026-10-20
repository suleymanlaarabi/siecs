#include <siecs.h>
#include <test.h>

void naming_paths_and_relative_creation(void) {
    ecs_test_scope scope;
    auto first = ecs::ensure("SceneA::Player");
    auto second = ecs::ensure("SceneB::Player");
    auto scene = ecs::entity::lookup("SceneA");
    test_true(first.id() != second.id());
    test_uint(first.id(), ecs::ensure(scene, "Player").id());
    test_uint(first.id(), ecs::entity::lookup(scene, "Player").id());
    test_uint(second.id(), ecs::entity::lookup(scene, "::SceneB::Player").id());
    test_str("Player", first.name());
    test_str("::SceneA::Player", first.path().c_str());
    test_uint(0, ecs::entity::lookup("Player").id());
    test_uint(0, ecs::entity::lookup(nullptr).id());
    test_uint(0, ecs::entity::lookup(scene, nullptr).id());
}

void naming_rename_and_reparent_replace(void) {
    ecs_test_scope scope;
    auto previous = ecs::ensure("Scene::Player");
    auto child = ecs::ensure(previous, "Child");
    auto replacement = ecs::ensure("Other::Replacement");
    test_true(replacement.set_name("Player"));
    test_true(previous.is_alive());
    replacement.child_of(ecs::entity::lookup("Scene"));
    test_false(previous.is_alive());
    test_false(child.is_alive());
    test_uint(replacement.id(), ecs::entity::lookup("Scene::Player").id());
    test_str("::Scene::Player", replacement.path().c_str());
    test_false(replacement.set_name("Invalid::Local"));
}

void naming_deferred_creation_and_anonymous_paths(void) {
    ecs_test_scope scope;
    ecs_defer_begin();
    auto first = ecs::ensure("Scene::Player");
    test_uint(first.id(), ecs::ensure("Scene::Player").id());
    test_uint(0, ecs::entity::lookup("Scene::Player").id());
    ecs_defer_end();
    auto anonymous = ecs::spawn(first);
    test_uint(anonymous.id(), ecs::entity::lookup(anonymous.path()).id());
    test_true(first.set_name(nullptr));
    test_uint(anonymous.id(), ecs::entity::lookup(anonymous.path()).id());
}

namespace NamingTypes {
struct Structure {};
class Class {};
enum Enumeration { Value };
union Union {
    int value;
};
} // namespace NamingTypes

void naming_cpp_type_names(void) {
    test_str(
        std::string(ecs::type_name<NamingTypes::Structure>()).c_str(),
        "NamingTypes::Structure"
    );
    test_str(std::string(ecs::type_name<NamingTypes::Class>()).c_str(), "NamingTypes::Class");
    test_str(
        std::string(ecs::type_name<NamingTypes::Enumeration>()).c_str(),
        "NamingTypes::Enumeration"
    );
    test_str(std::string(ecs::type_name<NamingTypes::Union>()).c_str(), "NamingTypes::Union");
}
