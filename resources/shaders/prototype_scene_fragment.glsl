#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) in vec2 fragTextureCoordinates;
layout(location = 4) in vec3 fragWorldPosition;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D baseColorTexture;
layout(std140, set = 0, binding = 1) uniform SceneMaterial
{
    vec4 baseColorFactor;
    vec4 alphaControls; // cutoff, MASK enabled, reserved, reserved
} material;

struct PointLight
{
    vec4 positionAndRadius;
    vec4 colorAndIntensity;
    vec4 controls; // enabled, first shadow layer (-1 if unshadowed)
};

layout(std140, set = 1, binding = 0) uniform PrototypeLighting
{
    PointLight pointLights[8];
    vec4 ambientIntensity;
    mat4 shadowCameras[24];
} environmentLighting;
layout(set = 1, binding = 1) uniform sampler2DArray pointShadowDepth;

int shadowFace(vec3 d) {
    vec3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) return d.x >= 0 ? 0 : 1;
    if (a.y >= a.z) return d.y >= 0 ? 2 : 3;
    return d.z >= 0 ? 4 : 5;
}
vec2 shadowUv(vec3 d, int face) {
    if (face < 2) return 0.5 + 0.5 * vec2(face == 0 ? -d.z : d.z, -d.y) / abs(d.x);
    if (face < 4) return 0.5 + 0.5 * vec2(d.x, face == 2 ? d.z : -d.z) / abs(d.y);
    return 0.5 + 0.5 * vec2(face == 4 ? d.x : -d.x, -d.y) / abs(d.z);
}
vec3 shadowDirection(int face, vec2 q) {
    if (face == 0) return vec3(1, -q.y, -q.x);
    if (face == 1) return vec3(-1, -q.y, q.x);
    if (face == 2) return vec3(q.x, 1, q.y);
    if (face == 3) return vec3(q.x, -1, -q.y);
    if (face == 4) return vec3(q.x, -q.y, 1);
    return vec3(-q.x, -q.y, -1);
}
float pointVisibility(PointLight light, vec3 fromLight, vec3 normal, float lambert) {
    if (light.controls.y < 0) return 1.0;
    float distanceFromLight = length(fromLight);
    if (distanceFromLight <= 0.01) return 1.0;
    int face = shadowFace(fromLight);
    vec2 uv = shadowUv(fromLight, face);
    float visibility = 0;
    float receiverPlane = dot(normal, fromLight);
    // Metre-scale comparison bias; each tap first intersects the receiver's
    // plane. Reusing radial depth across taps self-shadows slanted surfaces.
    float bias = 0.002 + 0.002 * (1.0-lambert);
    float a = light.positionAndRadius.w / (light.positionAndRadius.w - 0.01);
    ivec2 center = ivec2(uv * 512.0);
    // Almost every kernel stays on one face. Transform the receiver normal
    // once; only an edge tap needs the full adjacent-face reprojection.
    float forwardNormal = dot(normal, shadowDirection(face, vec2(0)));
    vec2 planeSlope = vec2(
        dot(normal, shadowDirection(face, vec2(1,0))) - forwardNormal,
        dot(normal, shadowDirection(face, vec2(0,1))) - forwardNormal);
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        int tapFace = face;
        ivec2 texel = center + ivec2(x,y);
        vec2 q = 2.0 * (vec2(texel)+0.5) / 512.0 - 1.0;
        float denominator = forwardNormal + dot(planeSlope, q);
        if (any(lessThan(texel, ivec2(0))) || any(greaterThan(texel, ivec2(511)))) {
            vec2 tap = 2.0 * (uv + vec2(x,y) / 512.0) - 1.0;
            vec3 direction = shadowDirection(face, tap);
            tapFace = shadowFace(direction);
            vec2 tapUv = shadowUv(direction, tapFace);
            texel = clamp(ivec2(tapUv * 512.0), ivec2(0), ivec2(511));
            vec3 sampleDirection = shadowDirection(tapFace, 2.0 * (vec2(texel)+0.5) / 512.0 - 1.0);
            denominator = dot(normal, sampleDirection);
        }
        if (denominator >= -0.00001) { visibility += 1.0; continue; }
        float majorDistance = max(0.01, receiverPlane / denominator - bias);
        float receiverDepth = a - a * 0.01 / majorDistance;
        float stored = texelFetch(pointShadowDepth, ivec3(texel, int(light.controls.y) + tapFace), 0).r;
        visibility += receiverDepth <= stored + environmentLighting.ambientIntensity.z ? 1.0 : 0.0;
    }
    return visibility / 9.0;
}

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
    vec4 baseColor = texture(baseColorTexture, fragTextureCoordinates) *
                     material.baseColorFactor;
    if (material.alphaControls.y > 0.5 && baseColor.a < material.alphaControls.x)
        discard;
    vec3 presentedColor = baseColor.rgb * fragColor.rgb;
    vec3 normal = normalize(fragWorldNormal);
    vec3 accumulatedLighting = vec3(environmentLighting.ambientIntensity.x);
    for (int lightIndex = 0; lightIndex < int(environmentLighting.ambientIntensity.y); ++lightIndex)
    {
        PointLight light = environmentLighting.pointLights[lightIndex];
        if (light.controls.x < 0.5) continue;
        vec3 vectorToLight = light.positionAndRadius.xyz - fragWorldPosition;
        float distanceToLight = length(vectorToLight);
        vec3 directionToLight = vectorToLight / max(distanceToLight, 0.0001);
        float lambert = max(dot(normal, directionToLight), 0.0);
        if (lambert <= 0 || distanceToLight >= light.positionAndRadius.w) continue;
        float normalizedDistance = clamp(
            distanceToLight / light.positionAndRadius.w, 0.0, 1.0);
        float falloff = 1.0 - normalizedDistance * normalizedDistance;
        falloff *= falloff;
        accumulatedLighting += light.colorAndIntensity.rgb *
            light.colorAndIntensity.w * lambert * falloff *
            pointVisibility(light, -vectorToLight, normal, lambert);
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
