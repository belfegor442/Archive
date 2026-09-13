#version 450

layout(set=0, binding=0) uniform UBO {
    vec4 OutputSize;
    vec4 OriginalSize;
    vec4 SourceSize;
    mat4 MVP;
};

#pragma stage vertex
void main() {
    vec2 pos = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1) * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);
}

#pragma stage fragment
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
