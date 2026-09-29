#pragma once
#ifndef SIECS_NO_CPP
#define SIECS_SYSTEM_PARAM_RESTORE_CPP
#define SIECS_NO_CPP
#endif
#include "siecs.h"
#ifdef SIECS_SYSTEM_PARAM_RESTORE_CPP
#undef SIECS_NO_CPP
#undef SIECS_SYSTEM_PARAM_RESTORE_CPP
#endif
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace ecs {

template <typename T> using system_param_t = std::remove_cvref_t<T>;

template <typename T>
concept SystemParam = requires {
    { system_param_t<T>::init() } -> std::same_as<system_param_t<T>>;
};

template <typename T>
concept IsSystemParamWorld = SystemParam<T> &&
    requires(system_param_t<T> &value, ecs_world_t *world) { value.world(world); };

template <typename T>
concept IsSystemParamTable = SystemParam<T> &&
    requires(system_param_t<T> &value, ecs_iter_t *it) { value.table(it); };

template <typename T>
concept IsSystemParamRow = SystemParam<T> &&
    requires(system_param_t<T> &value, ecs_iter_t *it, uint32_t row) { value.row(it, row); };

class system_param_query;
template <typename T> class optional;

template <typename T>
concept HasSystemParamQuery = SystemParam<T> &&
    requires(system_param_query &query) { system_param_t<T>::query(query); };

namespace detail { struct system_param_query_access; }

class system_param_query {
    ecs_query_desc_t &desc_;
    uint16_t &component_index_;
    uint16_t &resource_index_;
    uint16_t &relation_index_;

    system_param_query(ecs_query_desc_t &desc, uint16_t &component_index,
                       uint16_t &resource_index, uint16_t &relation_index) noexcept
        : desc_(desc), component_index_(component_index), resource_index_(resource_index),
          relation_index_(relation_index) {}
    friend struct detail::system_param_query_access;
    template <typename T> friend class optional;
    template <typename T> void optional_component();

  public:
    template <typename T> void read_resource();
    template <typename T> void write_resource();
    template <typename Relation> void require_relation();
};

} // namespace ecs

#ifndef SIECS_NO_CPP
#include "siecs/cpp.hpp"
#endif
