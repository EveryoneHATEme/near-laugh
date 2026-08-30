#version 450

layout(push_constant) uniform ScenePushConstant
{
    mat4 viewProjection;
    vec4 lightDirectionAndDirectionalIntensity;
    vec4 ambientIntensity;
    uvec4 presentationMasks;
} scene;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inSolidMask;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragWorldNormal;
layout(location = 2) flat out uint fragSolidMask;

void main()
{
    fragColor = inColor;
    fragWorldNormal = inNormal;
    fragSolidMask = inSolidMask;
    gl_Position = scene.viewProjection * vec4(inPosition, 1.0);
}
