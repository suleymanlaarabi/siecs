#include <cpp.hpp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ecs {

template <class T> char *type_name() {
#if defined(__clang__)
    constexpr std::string_view prefix = "char *type_name() [T = ";
    constexpr std::string_view suffix = "]";
    constexpr std::string_view func = __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
    constexpr std::string_view prefix = "char* type_name() [with T = ";
    constexpr std::string_view suffix = "]";
    constexpr std::string_view func = __PRETTY_FUNCTION__;
#else
#error "type_name<T>() only supports GCC and Clang"
#endif

    constexpr std::size_t start = prefix.size();
    constexpr std::size_t end = func.size() - suffix.size();
    constexpr std::string_view name = func.substr(start, end - start);

    char *out = static_cast<char *>(std::malloc(name.size() + 1));
    if (!out)
        return nullptr;

    std::memcpy(out, name.data(), name.size());
    out[name.size()] = '\0';

    return out;
}

class entity {
    ecs_entity_t _entity;
    ecs_world_t *_world;

  public:
    entity(ecs_world_t *world, ecs_entity_t entity) : _entity(entity), _world(world) {}

    template <typename T> void add() {}

    template <typename T> void set(T &&value) {}
};

class world {
    ecs_world_t *_world;

  public:
    world() : _world(ecs_init()) {}

    template <typename T> ecs_component_t component() {
        static ecs_component_t cid = 0;

        if (cid != 0) {
            return cid;
        }

        char *name = type_name<T>();
        ecs_component_desc_t desc = {
            .name = name,
            .size = sizeof(T),
            .struct_desc = NULL,
            .on_remove =
                [](ecs_world_t *world, ecs_entity_t, ecs_component_t, const void *ptr) {
                    T *value = static_cast<T *>(ptr);
                    value->~T();
                },
        };
    }

    ecs::entity entity() { return ecs::entity(_world, ecs_new(_world)); }
};

} // namespace ecs

struct Position {
    float x, y;
};

int main(int argc, char *argv[]) {
    ecs::world world;

    ecs::entity entity = world.entity();

    entity.add<Position>();
    entity.set<Position>({ 0, 0 });

    return 0;
}
