#if defined(VERTEX)
uniform mat4 MVPMatrix;
uniform int FrameDirection;
uniform int FrameCount;
uniform vec2 OutputSize;
uniform vec2 TextureSize;
uniform vec2 InputSize;
COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 TEX0;
void main() {
    gl_Position = MVPMatrix * VertexCoord;
    TEX0 = TexCoord;
}
#elif defined(FRAGMENT)
out vec4 FragColor;
uniform sampler2D Texture;
COMPAT_VARYING vec4 TEX0;
void main() {
    FragColor = COMPAT_TEXTURE(Texture, TEX0.xy);
}
#endif
