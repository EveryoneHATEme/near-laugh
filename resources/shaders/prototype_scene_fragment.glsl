#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) flat in uint fragSolidMask;
layout(location = 3) in vec2 fragTextureCoordinates;
layout(location = 4) flat in uint fragTextureLayer;
layout(location = 5) in vec3 fragWorldPosition;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2DArray surfaceTextures;

struct PointLight
{
    vec4 positionAndRadius;
    vec4 colorAndIntensity;
};

layout(std140, set = 1, binding = 0) uniform PrototypeLighting
{
    PointLight pointLights[2];
    vec4 ambientIntensity;
} environmentLighting;

layout(push_constant) uniform ScenePushConstant
{
    mat4 viewProjection;
    uvec4 presentationMasks;
} scene;

void main()
{
    vec3 sampledColor = texture(
        surfaceTextures, vec3(fragTextureCoordinates, float(fragTextureLayer)))
                            .rgb;
    vec3 presentedColor = sampledColor * fragColor.rgb;
    if ((scene.presentationMasks.x & fragSolidMask) != 0)
    {
        presentedColor = mix(presentedColor, vec3(1.0, 0.42, 0.05), 0.75);
    }
    else if ((scene.presentationMasks.y & fragSolidMask) != 0)
    {
        presentedColor *= 0.25;
    }
    vec3 normal = normalize(fragWorldNormal);
    vec3 accumulatedLighting = vec3(environmentLighting.ambientIntensity.x);
    for (int lightIndex = 0; lightIndex < 2; ++lightIndex)
    {
        PointLight light = environmentLighting.pointLights[lightIndex];
        vec3 vectorToLight = light.positionAndRadius.xyz - fragWorldPosition;
        float distanceToLight = length(vectorToLight);
        vec3 directionToLight = vectorToLight / max(distanceToLight, 0.0001);
        float lambert = max(dot(normal, directionToLight), 0.0);
        float normalizedDistance = clamp(
            distanceToLight / light.positionAndRadius.w, 0.0, 1.0);
        float falloff = 1.0 - normalizedDistance * normalizedDistance;
        falloff *= falloff;
        accumulatedLighting += light.colorAndIntensity.rgb *
            light.colorAndIntensity.w * lambert * falloff;
    }
    outColor = vec4(
        presentedColor * clamp(accumulatedLighting, vec3(0.0), vec3(1.0)),
        1.0);
}
