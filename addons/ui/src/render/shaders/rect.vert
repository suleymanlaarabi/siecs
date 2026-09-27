#version 450
layout(location=0) in vec4 in_rect;
layout(location=1) in vec4 in_background;
layout(location=2) in vec4 in_borders;
layout(location=3) in vec4 in_top;
layout(location=4) in vec4 in_right;
layout(location=5) in vec4 in_bottom;
layout(location=6) in vec4 in_left;
layout(location=0) out vec2 out_local;
layout(location=1) flat out vec2 out_size;
layout(location=2) flat out vec4 out_background;
layout(location=3) flat out vec4 out_borders;
layout(location=4) flat out vec4 out_top;
layout(location=5) flat out vec4 out_right;
layout(location=6) flat out vec4 out_bottom;
layout(location=7) flat out vec4 out_left;
layout(std140,set=1,binding=0) uniform Viewport {vec4 dimensions;} viewport;
void main() {
    vec2 unit=vec2(float(gl_VertexIndex==1 || gl_VertexIndex==2 || gl_VertexIndex==4),
                   float(gl_VertexIndex==2 || gl_VertexIndex==4 || gl_VertexIndex==5));
    vec2 pixel=in_rect.xy+unit*in_rect.zw;
    gl_Position=vec4(pixel.x*2.0/viewport.dimensions.x-1.0,
                     1.0-pixel.y*2.0/viewport.dimensions.y,0,1);
    out_local=unit*in_rect.zw;out_size=in_rect.zw;
    out_background=in_background;out_borders=in_borders;
    out_top=in_top;out_right=in_right;out_bottom=in_bottom;out_left=in_left;
}
