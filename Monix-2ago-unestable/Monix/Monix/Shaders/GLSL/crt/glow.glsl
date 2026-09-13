/*
    MONIX CRT Glow Shader
    Based on CRT-Royale's phosphor bloom model
    License: GPL-2.0 (based on crt-royale by TroggleMonkey)
    Reference: https://github.com/libretro/slang-shaders
*/

#pragma parameter glow_strength "Glow Strength" 0.1 0.0 1.0 0.02
#pragma parameter glow_radius "Glow Radius" 4.0 1.0 12.0 1.0
#pragma parameter glow_brightpass "Brightpass Threshold" 0.6 0.0 1.0 0.05

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
uniform COMPAT_PRECISION float glow_strength;
uniform COMPAT_PRECISION float glow_radius;
uniform COMPAT_PRECISION float glow_brightpass;
#else
#define glow_strength 0.1
#define glow_radius 4.0
#define glow_brightpass 0.6
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
uniform COMPAT_PRECISION float glow_strength;
uniform COMPAT_PRECISION float glow_radius;
uniform COMPAT_PRECISION float glow_brightpass;
#else
#define glow_strength 0.1
#define glow_radius 4.0
#define glow_brightpass 0.6
#endif

float gaussianWeight(float x, float sigma) {
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

void main()
{
    vec4 color = COMPAT_TEXTURE(Texture, vTexCoord);

    // Extract bright areas for bloom
    float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 brightpass = color.rgb * smoothstep(glow_brightpass, glow_brightpass + 0.3, luma);

    // Separable Gaussian blur (combined H+V in single pass)
    vec3 bloom = vec3(0.0);
    float totalWeight = 0.0;
    float sigma = glow_radius * 0.5;

    // Vertical samples
    for (float i = -glow_radius; i <= glow_radius; i += 1.0) {
        float weight = gaussianWeight(i, sigma);
        vec2 offset = vec2(0.0, i / OutputSize.y);
        vec3 sample_color = COMPAT_TEXTURE(Texture, vTexCoord + offset).rgb;

        float sample_luma = dot(sample_color, vec3(0.2126, 0.7152, 0.0722));
        vec3 sample_bright = sample_color * smoothstep(glow_brightpass, glow_brightpass + 0.3, sample_luma);

        bloom += sample_bright * weight;
        totalWeight += weight;
    }
    bloom /= totalWeight;

    // Horizontal samples
    totalWeight = 0.0;
    vec3 bloom_h = vec3(0.0);
    for (float i = -glow_radius; i <= glow_radius; i += 1.0) {
        float weight = gaussianWeight(i, sigma);
        vec2 offset = vec2(i / OutputSize.x, 0.0);
        vec3 sample_color = COMPAT_TEXTURE(Texture, vTexCoord + offset).rgb;

        float sample_luma = dot(sample_color, vec3(0.2126, 0.7152, 0.0722));
        vec3 sample_bright = sample_color * smoothstep(glow_brightpass, glow_brightpass + 0.3, sample_luma);

        bloom_h += sample_bright * weight;
        totalWeight += weight;
    }
    bloom_h /= totalWeight;

    bloom = (bloom + bloom_h) * 0.5;

    // Add glow to original
    vec3 result = color.rgb + bloom * glow_strength;

    FragColor = vec4(result, color.a);
}

#endif
