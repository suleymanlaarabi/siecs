#include "render/render_internal.h"
sigpu_shared_material_t make_shared_material(primitive_size_t size, Color color, float bloom) {
    return (sigpu_shared_material_t){ .size_bloom = { size.x, size.y, size.z, bloom },
                                      .color = { SIGPU_RENDERSETTINGS->linear_lut[color.r] / 255.0f,
                                                 SIGPU_RENDERSETTINGS->linear_lut[color.g] / 255.0f,
                                                 SIGPU_RENDERSETTINGS->linear_lut[color.b] / 255.0f,
                                                 color.a / 255.0f } };
}

void sigpu_materials_init(void) {
    for (int index = 0; index < 256; index++) {
        float srgb = index / 255.0f;
        float linear = srgb <= 0.04045f ? srgb / 12.92f : powf((srgb + 0.055f) / 1.055f, 2.4f);
        SIGPU_RENDERSETTINGS->linear_lut[index] = (uint8_t)roundf(linear * 255.0f);
    }
}
