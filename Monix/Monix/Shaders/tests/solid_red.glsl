#version 450
#pragma stage vertex
layout(set = 0, binding = 0) uniform VertexUniforms { mat4 MVP; };
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(Position, 1.0);
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
