// solid_red.slang — Outputs solid red for pixel verification testing

#version 450

#pragma stage vertex

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

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
};

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
