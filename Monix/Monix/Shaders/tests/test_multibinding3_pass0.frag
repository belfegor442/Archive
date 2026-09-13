#version 450

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
layout(set = 0, binding = 3) uniform sampler2D Pass0;
layout(set = 0, binding = 4) uniform sampler2D Pass1;

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

void main() {
    vec4 src = texture(Source, v_texCoord);
    vec4 p0 = texture(Pass0, v_texCoord);
    vec4 p1 = texture(Pass1, v_texCoord);
    fragColor = src + p0 + p1;
}
