#version 450
layout(push_constant) uniform ScenePushConstant {
    mat4 viewProjection;
    vec4 spotPositionAndRange;
    vec4 spotDirectionAndInnerCosine;
    vec4 spotColorAndIntensity;
    vec4 lightControls;
} scene;
layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec2 inTextureCoordinates;
layout(location = 0) out vec2 textureCoordinates;
void main() {
    textureCoordinates = inTextureCoordinates;
    gl_Position = scene.viewProjection * vec4(inPosition, 1.0);
}
