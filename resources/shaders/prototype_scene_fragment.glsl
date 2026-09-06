#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) in vec2 fragTextureCoordinates;
layout(location = 3) flat in uint fragTextureLayer;
layout(location = 4) in vec3 fragWorldPosition;
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
    vec4 spotPositionAndRange;
    vec4 spotDirectionAndInnerCosine;
    vec4 spotColorAndIntensity;
    vec4 lightControls;
} scene;

void main()
{
    vec3 sampledColor = texture(
        surfaceTextures, vec3(fragTextureCoordinates, float(fragTextureLayer)))
                            .rgb;
    vec3 presentedColor = sampledColor * fragColor.rgb;
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
            light.colorAndIntensity.w * lambert * falloff * scene.lightControls[2 + lightIndex];
    }
    if (scene.lightControls.y > 0.5)
    {
        vec3 vectorFromLight = fragWorldPosition - scene.spotPositionAndRange.xyz;
        float distanceToLight = length(vectorFromLight);
        vec3 directionFromLight = vectorFromLight / max(distanceToLight, 0.0001);
        vec3 spotDirection = normalize(scene.spotDirectionAndInnerCosine.xyz);
        float directionCosine = dot(directionFromLight, spotDirection);
        float angularFalloff = smoothstep(
            scene.lightControls.x,
            scene.spotDirectionAndInnerCosine.w,
            directionCosine);
        float normalizedDistance = clamp(
            distanceToLight / scene.spotPositionAndRange.w, 0.0, 1.0);
        float distanceFalloff = 1.0 - normalizedDistance * normalizedDistance;
        distanceFalloff *= distanceFalloff;
        float lambert = max(dot(normal, -directionFromLight), 0.0);
        accumulatedLighting += scene.spotColorAndIntensity.rgb *
            scene.spotColorAndIntensity.w * lambert * distanceFalloff *
            angularFalloff;
    }
    outColor = vec4(
        presentedColor * clamp(accumulatedLighting, vec3(0.0), vec3(1.0)),
        1.0);
}
