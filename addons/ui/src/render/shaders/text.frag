#version 450
layout(location=0) in vec2 in_uv;
layout(location=1) in vec4 in_color;
layout(location=2) flat in float in_image_type;
layout(set=2,binding=0) uniform sampler2D atlas;
layout(location=0) out vec4 out_color;
void main() {
    vec4 texel=texture(atlas,in_uv);
    float alpha=texel.a;
    if(in_image_type>2.5) alpha=smoothstep(0.45,0.55,texel.a);
    vec3 rgb=in_image_type>1.5 && in_image_type<2.5?texel.rgb*in_color.rgb:in_color.rgb;
    out_color=vec4(rgb,in_color.a*alpha);
}
