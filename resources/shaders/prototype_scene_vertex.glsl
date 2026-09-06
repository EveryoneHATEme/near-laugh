#version 450

layout(push_constant) uniform ScenePushConstant
{
    mat4 viewProjection;
    vec4 spotPositionAndRange;
    vec4 spotDirectionAndInnerCosine;
    vec4 spotColorAndIntensity;
    vec4 lightControls;
} scene;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTextureCoordinates;
layout(location = 4) in uint inTextureLayer;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragWorldNormal;
layout(location = 2) out vec2 fragTextureCoordinates;
layout(location = 3) flat out uint fragTextureLayer;
layout(location = 4) out vec3 fragWorldPosition;

void main()
{
    fragColor = inColor;
    fragWorldNormal = inNormal;
    fragTextureCoordinates = inTextureCoordinates;
    fragTextureLayer = inTextureLayer;
    fragWorldPosition = inPosition;
    gl_Position = scene.viewProjection * vec4(inPosition, 1.0);
}
