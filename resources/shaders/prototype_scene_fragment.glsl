#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) flat in uint fragSolidMask;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform ScenePushConstant
{
    mat4 viewProjection;
    vec4 lightDirectionAndDirectionalIntensity;
    vec4 ambientIntensity;
    uvec4 presentationMasks;
} scene;

void main()
{
    vec3 presentedColor = fragColor.rgb;
    if ((scene.presentationMasks.x & fragSolidMask) != 0)
    {
        presentedColor = mix(presentedColor, vec3(1.0, 0.42, 0.05), 0.75);
    }
    else if ((scene.presentationMasks.y & fragSolidMask) != 0)
    {
        presentedColor *= 0.25;
    }
    vec3 normal = normalize(fragWorldNormal);
    vec3 directionToLight =
        normalize(scene.lightDirectionAndDirectionalIntensity.xyz);
    float lambert = max(dot(normal, directionToLight), 0.0);
    float lighting = clamp(
        scene.ambientIntensity.x +
            scene.lightDirectionAndDirectionalIntensity.w * lambert,
        0.0, 1.0);
    outColor = vec4(presentedColor * lighting, fragColor.a);
}
