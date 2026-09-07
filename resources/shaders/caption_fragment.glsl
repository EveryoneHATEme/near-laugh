#version 450
layout(set = 0, binding = 0) uniform sampler2D glyph_atlas;
layout(location = 0) in vec2 glyph_uv;
layout(location = 1) in vec4 glyph_color;
layout(location = 0) out vec4 color;
void main() {
    color = vec4(glyph_color.rgb, glyph_color.a * texture(glyph_atlas, glyph_uv).a);
}
