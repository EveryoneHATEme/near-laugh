#version 450

layout(push_constant) uniform CameraPushConstant
{
    mat4 viewProjection;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = inColor;
    gl_Position = camera.viewProjection * vec4(inPosition, 1.0);
}
