/*
    MONIX CRT Flicker Shader
    Simulates CRT phosphor temporal decay and refresh flicker
    Custom implementation for Monix
*/

#pragma parameter flicker_strength "Flicker Strength" 0.01 0.0 0.05 0.005

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

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

#define vTexCoord TEX0.xy

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float flicker_strength;
#else
#define flicker_strength 0.01
#endif

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    TEX0.xy = TexCoord.xy;
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

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform sampler2D Texture;

#define SourceSize vec4(TextureSize, 1.0 / TextureSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float flicker_strength;
#else
#define flicker_strength 0.01
#endif

void main()
{
    vec4 color = COMPAT_TEXTURE(Texture, vTexCoord);

    // CRT flicker: subtle brightness variation based on scanline position and frame count
    float scanline = vTexCoord.y * OutputSize.y;
    float frame = float(FrameCount);

    // Two interleaved patterns: simulates interlaced field flicker
    float fieldPhase = mod(scanline + frame, 2.0);
    float flicker = 1.0 - flicker_strength * (fieldPhase < 1.0 ? 1.0 : 0.0);

    // Micro-flicker based on phosphor position
    float phosphorPhase = mod(vTexCoord.x * OutputSize.x * 3.0, 1.0);
    float microFlicker = 1.0 + flicker_strength * 0.3 * sin(phosphorPhase * 6.2831853);

    color.rgb *= flicker * microFlicker;

    FragColor = color;
}

#endif
