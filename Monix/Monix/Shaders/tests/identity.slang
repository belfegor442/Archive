// identity.slang — Pass-through shader for runtime testing
// Reads Source texture and outputs it unchanged.

#version 450

#pragma stage vertex

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

layout(push_constant) uniform PushConstants {
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    vec4 OriginalViewportSize;
    uint FrameCount;
    int FrameDirection;
} pc;

layout(location = 0) out vec2 v_texCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    v_texCoord = texCoord;
}

#pragma stage fragment

layout(set = 0, binding = 0) uniform UBO {
    mat4 MVP;
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    vec4 FinalViewportSize;
    uint FrameCount;
    uint FrameDirection;
    vec2 _padding;
};

layout(set = 0, binding = 2) uniform sampler2D Source;

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(Source, v_texCoord);
}
