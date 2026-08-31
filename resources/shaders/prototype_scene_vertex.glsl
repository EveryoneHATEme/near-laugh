#version 450

layout(push_constant) uniform ScenePushConstant
{
    mat4 viewProjection;
    uvec4 presentationMasks;
} scene;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inSolidMask;
layout(location = 4) in vec2 inTextureCoordinates;
layout(location = 5) in uint inTextureLayer;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragWorldNormal;
layout(location = 2) flat out uint fragSolidMask;
layout(location = 3) out vec2 fragTextureCoordinates;
layout(location = 4) flat out uint fragTextureLayer;
layout(location = 5) out vec3 fragWorldPosition;

void main()
{
    fragColor = inColor;
    fragWorldNormal = inNormal;
    fragSolidMask = inSolidMask;
    fragTextureCoordinates = inTextureCoordinates;
    fragTextureLayer = inTextureLayer;
    fragWorldPosition = inPosition;
    gl_Position = scene.viewProjection * vec4(inPosition, 1.0);
}
