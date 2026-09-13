/*
    MONIX CRT Phosphor Mask Shader
    Based on CRT-Royale's aperture grille / slot mask model
    License: GPL-2.0 (based on crt-royale by TroggleMonkey)
    Reference: https://github.com/libretro/slang-shaders
*/

#pragma parameter phosphor_mask_type "Mask Type (0=Grille 1=Slot 2=Shadow)" 0.0 0.0 2.0 1.0
#pragma parameter phosphor_strength "Phosphor Strength" 0.18 0.0 1.0 0.02
#pragma parameter phosphor_triad_size "Triad Size" 3.0 1.0 8.0 1.0

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
uniform COMPAT_PRECISION float phosphor_mask_type;
uniform COMPAT_PRECISION float phosphor_strength;
uniform COMPAT_PRECISION float phosphor_triad_size;
#else
#define phosphor_mask_type 0.0
#define phosphor_strength 0.18
#define phosphor_triad_size 3.0
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
uniform COMPAT_PRECISION float phosphor_mask_type;
uniform COMPAT_PRECISION float phosphor_strength;
uniform COMPAT_PRECISION float phosphor_triad_size;
#else
#define phosphor_mask_type 0.0
#define phosphor_strength 0.18
#define phosphor_triad_size 3.0
#endif

// Aperture grille: symmetric RGB grid (both H and V)
vec3 apertureGrille(vec2 coord, float triadWidth) {
    float px = mod(coord.x * triadWidth, 3.0);
    float py = mod(coord.y * triadWidth, 3.0);

    vec3 maskX = vec3(0.0);
    if (px < 1.0) maskX.r = 1.0;
    else if (px < 2.0) maskX.g = 1.0;
    else maskX.b = 1.0;

    vec3 maskY = vec3(0.0);
    if (py < 1.0) maskY.r = 1.0;
    else if (py < 2.0) maskY.g = 1.0;
    else maskY.b = 1.0;

    // Symmetric intersection: both H and V must match for a channel to be bright
    return maskX * maskY;
}

// Slot mask: symmetric offset grid
vec3 slotMask(vec2 coord, float triadWidth) {
    float px = mod(coord.x * triadWidth, 3.0);
    float py = mod(coord.y * triadWidth, 3.0);

    // Offset every other row and column for slot pattern
    float offsetX = mod(floor(coord.y * triadWidth), 2.0) * 1.5;
    float offsetY = mod(floor(coord.x * triadWidth), 2.0) * 1.5;

    float posX = mod(px + offsetX, 3.0);
    float posY = mod(py + offsetY, 3.0);

    vec3 maskX = vec3(0.0);
    if (posX < 1.0) maskX.r = 1.0;
    else if (posX < 2.0) maskX.g = 1.0;
    else maskX.b = 1.0;

    vec3 maskY = vec3(0.0);
    if (posY < 1.0) maskY.r = 1.0;
    else if (posY < 2.0) maskY.g = 1.0;
    else maskY.b = 1.0;

    return maskX * maskY;
}

// Shadow mask: symmetric triad dot pattern
vec3 shadowMask(vec2 coord, float triadWidth) {
    float px = mod(coord.x * triadWidth, 3.0);
    float py = mod(coord.y * triadWidth, 3.0);

    float offsetX = mod(floor(coord.y * triadWidth), 2.0) * 1.5;
    float offsetY = mod(floor(coord.x * triadWidth), 2.0) * 1.5;

    float posX = mod(px + offsetX, 3.0);
    float posY = mod(py + offsetY, 3.0);

    vec3 mask = vec3(0.0);
    float distX = mod(posX, 1.0) - 0.5;
    float distY = mod(posY, 1.0) - 0.5;
    float dist = length(vec2(distX, distY));

    if (dist < 0.4) {
        float channel = floor(min(posX, posY));
        if (channel < 1.0) mask.r = 1.0;
        else if (channel < 2.0) mask.g = 1.0;
        else mask.b = 1.0;
    }

    return mask;
}

void main()
{
    vec4 color = COMPAT_TEXTURE(Texture, vTexCoord);

    float triadWidth = OutputSize.x / phosphor_triad_size;
    vec3 mask = vec3(1.0);

    if (phosphor_mask_type < 0.5) {
        mask = apertureGrille(vTexCoord, triadWidth);
    } else if (phosphor_mask_type < 1.5) {
        mask = slotMask(vTexCoord, triadWidth);
    } else {
        mask = shadowMask(vTexCoord, triadWidth);
    }

    vec3 masked = color.rgb * mix(vec3(1.0), mask, phosphor_strength);

    FragColor = vec4(masked, color.a);
}

#endif
