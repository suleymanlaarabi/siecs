#ifndef SIUI_CPP_HPP
#define SIUI_CPP_HPP
#include <siecs/cpp.hpp>
#include <siui.h>

namespace ui {

inline UiNode hstack() { return UiNode{}.row(); }

inline UiNode vstack() { return UiNode{}.column(); }

} // namespace ui

#endif
