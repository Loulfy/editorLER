#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Return Output
layout (location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = vec4(0.0, 0.0, 0.0, 1.0);
}