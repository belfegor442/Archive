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
layout(set = 0, binding = 3) uniform sampler2D texture0;

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

void main() {
    vec4 src = texture(Source, v_texCoord);
    vec4 ext = texture(texture0, v_texCoord);
    fragColor = mix(src, ext, 0.5);
}
