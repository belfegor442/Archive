#version 450

layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;

layout(location = 0) out vec2 vTexCoord;

void main() {
  gl_Position = Position;
  vTexCoord = TexCoord;
}
