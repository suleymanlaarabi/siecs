#ifndef SIUI_CPP_HPP
#define SIUI_CPP_HPP
#include <siui.h>
#include <string>
#include <string_view>
namespace ui {
using color = siui_color_t;
using length = siui_length_t;
inline constexpr siui_direction_t row=SIUI_ROW, column=SIUI_COLUMN;
inline constexpr length auto_size{0,SIUI_AUTO};
inline constexpr length percent(float value) { return {value,SIUI_PERCENT}; }
inline constexpr length px(float value) { return {value,SIUI_PX}; }
class node_builder {
    UiNode desc_{};
    UiPaint paint_{};
    UiText text_{};
    std::string value_;
    ecs_entity_t parent_=0;
    bool painted_=false,texted_=false;
public:
    node_builder() {text_.wrap=SIUI_TEXT_WRAP;}
    node_builder &width(length value) {desc_.width=value;return *this;}
    node_builder &width(float value) {return width(px(value));}
    node_builder &height(length value) {desc_.height=value;return *this;}
    node_builder &height(float value) {return height(px(value));}
    node_builder &min_width(float value) {desc_.min_width=px(value);return *this;}
    node_builder &max_width(float value) {desc_.max_width=px(value);return *this;}
    node_builder &min_height(float value) {desc_.min_height=px(value);return *this;}
    node_builder &max_height(float value) {desc_.max_height=px(value);return *this;}
    node_builder &padding(float value) {desc_.padding=siui_all(value);return *this;}
    node_builder &margin(float value) {desc_.margin=siui_all(value);return *this;}
    node_builder &direction(siui_direction_t value) {desc_.direction=value;return *this;}
    node_builder &wrap(siui_wrap_t value) {desc_.wrap=value;return *this;}
    node_builder &justify(siui_justify_t value) {desc_.justify=value;return *this;}
    node_builder &align_items(siui_align_t value) {desc_.align_items=value;return *this;}
    node_builder &gap(float value) {desc_.gap=value;return *this;}
    node_builder &grow(float value) {desc_.flex_grow=value;return *this;}
    node_builder &shrink(float value) {desc_.flex_shrink=value;return *this;}
    node_builder &order(int16_t value) {desc_.order=value;return *this;}
    node_builder &z_index(int16_t value) {desc_.z_index=value;return *this;}
    node_builder &background(color value) {paint_.background=value;painted_=true;return *this;}
    node_builder &border(float width,color value) {
        desc_.border_width=siui_all(width);
        paint_.border_top=paint_.border_right=paint_.border_bottom=paint_.border_left=value;
        painted_=true;return *this;
    }
    node_builder &text(std::string_view value) {value_.assign(value.data(),value.size());texted_=true;return *this;}
    node_builder &font(siui_font_handle_t value,float size=16) {text_.font=value;text_.font_size=size;return *this;}
    node_builder &text_color(color value) {text_.color=value;return *this;}
    node_builder &child_of(ecs_entity_t entity) {parent_=entity;return *this;}
    ecs_entity_t spawn() const {
        ecs_entity_t e=ecs_new();
        ecs_set_cid(e,ecs_id(UiNode),&desc_);
        if(painted_) ecs_set_cid(e,ecs_id(UiPaint),&paint_);
        if(texted_) {
            siui_set_text_styled(e,value_.data(),value_.size(),&text_);
        }
        if(parent_) ecs_relate(e,ChildOf,parent_);
        return e;
    }
};
inline node_builder node() {return {};}
}
#endif
