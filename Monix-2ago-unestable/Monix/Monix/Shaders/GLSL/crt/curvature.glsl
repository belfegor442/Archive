/*
    MONIX CRT Curvature Shader
    Based on CRT-Geom's barrel distortion model
    License: GPL-2.0 (based on crt-geom by cgwg, Themaister, DOLLS)
    Reference: https://github.com/libretro/slang-shaders
*/

#pragma parameter curvature_strength "Curvature" 0.06 0.0 0.5 0.01
#pragma parameter curvature_radius "Curvature Radius" 12.0 2.0 30.0 1.0

#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
#define COMPAT_PRECISION highp
#else
#define COMPAT_PRECISION mediump
#endif
#else
#define COMPAT_PRECISION
#endif

COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec2 vScreenCoord;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float curvature_strength;
uniform COMPAT_PRECISION float curvature_radius;
#else
#define curvature_strength 0.06
#define curvature_radius 12.0
#endif

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    TEX0.xy = TexCoord.xy;
    vScreenCoord = TexCoord.xy * 2.0 - 1.0;
}

#elif defined(FRAGMENT)

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#ifdef GL_FRAGMENT_PRECISION_HIGH
#define COMPAT_PRECISION highp
#else
#define COMPAT_PRECISION mediump
#endif
#else
#define COMPAT_PRECISION
#endif

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out COMPAT_PRECISION vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define COMPAT_TEXTURE texture2D
#endif

COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec2 vScreenCoord;

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform sampler2D Texture;

#define SourceSize vec4(TextureSize, 1.0 / TextureSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float curvature_strength;
uniform COMPAT_PRECISION float curvature_radius;
#else
#define curvature_strength 0.06
#define curvature_radius 12.0
#endif

vec2 crtCurvature(vec2 coord, float strength) {
    vec2 cc = coord;
    float delta = 1.0 - strength;
    cc.x = cc.x * (1.0 + delta) / (1.0 + strength * (cc.x * cc.x));
    cc.y = cc.y * (1.0 + delta) / (1.0 + strength * (cc.y * cc.y));
    return cc * 0.5 + 0.5;
}

float borderMask(vec2 coord) {
    vec2 border = smoothstep(0.0, 0.02, coord) * (1.0 - smoothstep(0.98, 1.0, coord));
    return border.x * border.y;
}

void main()
{
    vec2 curvedCoord = crtCurvature(vScreenCoord, curvature_strength);

    if (curvedCoord.x < 0.0 || curvedCoord.x > 1.0 ||
        curvedCoord.y < 0.0 || curvedCoord.y > 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 color = COMPAT_TEXTURE(Texture, curvedCoord);

    float mask = borderMask(curvedCoord);
    color.rgb *= mix(0.7, 1.0, mask);

    FragColor = color;
}

#endif
