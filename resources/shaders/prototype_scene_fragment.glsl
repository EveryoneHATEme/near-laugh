#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform ScenePushConstant
{
    mat4 viewProjection;
    vec4 lightDirectionAndDirectionalIntensity;
    vec4 ambientIntensity;
} scene;

void main()
{
    vec3 normal = normalize(fragWorldNormal);
    vec3 directionToLight =
        normalize(scene.lightDirectionAndDirectionalIntensity.xyz);
    float lambert = max(dot(normal, directionToLight), 0.0);
    float lighting = clamp(
        scene.ambientIntensity.x +
            scene.lightDirectionAndDirectionalIntensity.w * lambert,
        0.0, 1.0);
    outColor = vec4(fragColor.rgb * lighting, fragColor.a);
}
