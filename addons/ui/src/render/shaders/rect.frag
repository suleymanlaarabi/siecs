#version 450
layout(location=0) in vec2 in_local;
layout(location=1) flat in vec2 in_size;
layout(location=2) flat in vec4 in_background;
layout(location=3) flat in vec4 in_borders;
layout(location=4) flat in vec4 in_top;
layout(location=5) flat in vec4 in_right;
layout(location=6) flat in vec4 in_bottom;
layout(location=7) flat in vec4 in_left;
layout(location=0) out vec4 out_color;
void main() {
    if(in_local.y<in_borders.x) out_color=in_top;
    else if(in_local.y>=in_size.y-in_borders.z) out_color=in_bottom;
    else if(in_local.x<in_borders.w) out_color=in_left;
    else if(in_local.x>=in_size.x-in_borders.y) out_color=in_right;
    else out_color=in_background;
}
