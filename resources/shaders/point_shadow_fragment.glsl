#version 450
layout(location = 0) in vec2 textureCoordinates;
layout(set = 0, binding = 0) uniform sampler2D baseColorTexture;
layout(std140, set = 0, binding = 1) uniform SceneMaterial {
    vec4 baseColorFactor;
    vec4 alphaControls;
} material;
void main() {
    // Match the color pass exactly: OPAQUE ignores alpha, MASK samples it.
    if (material.alphaControls.y > 0.5 &&
        texture(baseColorTexture, textureCoordinates).a * material.baseColorFactor.a <
            material.alphaControls.x)
        discard;
}
