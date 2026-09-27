#version 450
layout(location=0) in vec2 in_position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec4 in_color;
layout(location=3) in float in_image_type;
layout(location=0) out vec2 out_uv;
layout(location=1) out vec4 out_color;
layout(location=2) flat out float out_image_type;
layout(std140,set=1,binding=0) uniform Viewport {vec4 dimensions;} viewport;
void main() {
    gl_Position=vec4(in_position.x*2.0/viewport.dimensions.x-1.0,
                     1.0-in_position.y*2.0/viewport.dimensions.y,0,1);
    out_uv=in_uv;out_color=in_color;out_image_type=in_image_type;
}
