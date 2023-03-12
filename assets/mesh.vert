#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Attributes
layout (location = 0) in vec3 inPos;

layout (push_constant) uniform constants
{
    mat4 proj;
    mat4 view;
} PushConstants;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = PushConstants.proj * PushConstants.view * vec4(inPos.xyz, 1.0);
}