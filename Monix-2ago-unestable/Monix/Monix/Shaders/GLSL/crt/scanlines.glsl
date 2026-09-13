/*
    MONIX CRT Scanlines Shader
    Based on CRT-Royale's Gaussian beam scanline model
    License: GPL-2.0 (based on crt-royale by TroggleMonkey)
    Reference: https://github.com/libretro/slang-shaders
*/

#pragma parameter scanline_sigma "Scanline Sharpness" 0.3 0.05 1.0 0.05
#pragma parameter scanline_strength "Scanline Strength" 0.3 0.0 1.0 0.02

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
uniform COMPAT_PRECISION float scanline_sigma;
uniform COMPAT_PRECISION float scanline_strength;
#else
#define scanline_sigma 0.3
#define scanline_strength 0.3
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
uniform COMPAT_PRECISION float scanline_sigma;
uniform COMPAT_PRECISION float scanline_strength;
#else
#define scanline_sigma 0.3
#define scanline_strength 0.3
#endif

void main()
{
    vec4 color = COMPAT_TEXTURE(Texture, vTexCoord);

    float sigma = mix(0.05, 0.5, 1.0 - scanline_sigma);

    // Vertical scanlines (horizontal lines, based on Y)
    float vy = vTexCoord.y * OutputSize.y;
    float vy_center = floor(vy) + 0.5;
    float vy_dist = abs(vy - vy_center);
    float v_beam = exp(-(vy_dist * vy_dist) / (2.0 * sigma * sigma));

    // Horizontal scanlines (vertical lines, based on X) — same strength for symmetry
    float vx = vTexCoord.x * OutputSize.x;
    float vx_center = floor(vx) + 0.5;
    float vx_dist = abs(vx - vx_center);
    float h_beam = exp(-(vx_dist * vx_dist) / (2.0 * sigma * sigma));

    // Symmetric: both axes use the same Gaussian with same strength
    float scanline = mix(1.0, v_beam * h_beam, scanline_strength);

    color.rgb *= scanline;

    FragColor = color;
}

#endif
