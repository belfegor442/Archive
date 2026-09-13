/*
    zfast_crt_standard - A simple, fast CRT shader.
    Copyright (C) 2017 Greg Hogan (SoltanGris42)
    License: GPL
*/

#define FINEMASK

#pragma parameter BLURSCALEX "Blur Amount X-Axis" 0.30 0.0 1.0 0.05
#pragma parameter LOWLUMSCAN "Scanline Darkness - Low" 6.0 0.0 10.0 0.5
#pragma parameter HILUMSCAN "Scanline Darkness - High" 8.0 0.0 50.0 1.0
#pragma parameter BRIGHTBOOST "Dark Pixel Brightness Boost" 1.25 0.5 1.5 0.05
#pragma parameter MASK_DARK "Mask Effect Amount" 0.25 0.0 1.0 0.05
#pragma parameter MASK_FADE "Mask/Scanline Fade" 0.8 0.0 1.0 0.05

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
COMPAT_VARYING float maskFade;
COMPAT_VARYING vec2 invDims;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

#define vTexCoord TEX0.xy
#define SourceSize vec4(TextureSize, 1.0 / TextureSize)
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float BLURSCALEX;
uniform COMPAT_PRECISION float LOWLUMSCAN;
uniform COMPAT_PRECISION float HILUMSCAN;
uniform COMPAT_PRECISION float BRIGHTBOOST;
uniform COMPAT_PRECISION float MASK_DARK;
uniform COMPAT_PRECISION float MASK_FADE;
#else
#define BLURSCALEX 0.45
#define LOWLUMSCAN 5.0
#define HILUMSCAN 10.0
#define BRIGHTBOOST 1.25
#define MASK_DARK 0.25
#define MASK_FADE 0.8
#endif

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    TEX0.xy = TexCoord.xy * 1.0001;
    maskFade = 0.3333 * MASK_FADE;
    invDims = 1.0 / TextureSize.xy;
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
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform sampler2D Texture;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING float maskFade;
COMPAT_VARYING vec2 invDims;

#define Source Texture
#define vTexCoord TEX0.xy
#define texture(c, d) COMPAT_TEXTURE(c, d)
#define SourceSize vec4(TextureSize, 1.0 / TextureSize)
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float BLURSCALEX;
uniform COMPAT_PRECISION float LOWLUMSCAN;
uniform COMPAT_PRECISION float HILUMSCAN;
uniform COMPAT_PRECISION float BRIGHTBOOST;
uniform COMPAT_PRECISION float MASK_DARK;
uniform COMPAT_PRECISION float MASK_FADE;
#else
#define BLURSCALEX 0.45
#define LOWLUMSCAN 5.0
#define HILUMSCAN 10.0
#define BRIGHTBOOST 1.25
#define MASK_DARK 0.25
#define MASK_FADE 0.8
#endif

void main()
{
    COMPAT_PRECISION vec2 p = vTexCoord * TextureSize;
    COMPAT_PRECISION vec2 i = floor(p) + 0.50;
    COMPAT_PRECISION vec2 f = p - i;
    p = (i + 4.0 * f * f * f) * invDims;
    p.x = mix(p.x, vTexCoord.x, BLURSCALEX);
    COMPAT_PRECISION float Y = f.y * f.y;
    COMPAT_PRECISION float YY = Y * Y;
#define ratio SourceSize.x / InputSize.x
#if defined(FINEMASK)
    COMPAT_PRECISION float whichmask = floor(vTexCoord.x * OutputSize.x * ratio) * -0.5;
    COMPAT_PRECISION float mask = 1.0 + float(fract(whichmask) < 0.5) * -MASK_DARK;
#else
    COMPAT_PRECISION float whichmask = floor(vTexCoord.x * OutputSize.x * ratio) * -0.3333;
    COMPAT_PRECISION float mask = 1.0 + float(fract(whichmask) < 0.3333) * -MASK_DARK;
#endif
    COMPAT_PRECISION vec3 colour = COMPAT_TEXTURE(Texture, p).rgb;

    COMPAT_PRECISION float scanLineWeight = (BRIGHTBOOST - LOWLUMSCAN * (Y - 2.05 * YY));
    COMPAT_PRECISION float scanLineWeightB = 1.0 - HILUMSCAN * (YY - 2.8 * YY * Y);

    FragColor.rgba = vec4(colour.rgb * mix(scanLineWeight * mask, scanLineWeightB, dot(colour.rgb, vec3(maskFade))), 1.0);
}
#endif
