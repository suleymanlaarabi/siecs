#include <siecs/cpp/entity.hpp>

void cpp_entity_header_is_self_contained() {
    ecs::entity value = ecs::entity::null();
    (void)value;
}
