```glsl
/*
    CRT-interlaced

    Copyright (C) 2010-2012 cgwg, Themaister and DOLLS

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    (cgwg gave their consent to have the original version of this shader
    distributed under the GPL in this message:

        http://board.byuu.org/viewtopic.php?p=26075#p26075

        "Feel free to distribute my shaders under the GPL. After all, the
        barrel distortion code was taken from the Curvature shader, which is
        under the GPL."
    
    This shader variant is pre-configured with screen curvature
*/

#pragma parameter CRTgamma "CRT Gamma" 2.4 0.1 5.0 0.1
#pragma parameter INV "Inverse Gamma" 0.0 0.0 1.0 1.0
#pragma parameter monitorgamma "Monitor Gamma" 2.2 0.1 5.0 0.1

#pragma parameter d "CRT Distance" 1.5 0.1 3.0 0.1
#pragma parameter CURVATURE "Curvature" 1.0 0.0 1.0 1.0
#pragma parameter R "Curvature Radius" 3.3 0.1 10.0 0.1

#pragma parameter cornersize "Corner Size" 0.015 0.001 1.0 0.005
#pragma parameter cornersmooth "Corner Smoothness" 400.0 80.0 2000.0 100.0

#pragma parameter x_tilt "Horizontal Tilt" 0.0 -0.5 0.5 0.05
#pragma parameter y_tilt "Vertical Tilt" 0.0 -0.5 0.5 0.05

#pragma parameter overscan_x "Horizontal Overscan" 100.0 -125.0 125.0 1.0
#pragma parameter overscan_y "Vertical Overscan" 100.0 -125.0 125.0 1.0

#pragma parameter DOTMASK "Dot Mask Strength" 0.15 0.0 1.0 0.05
#pragma parameter SHARPER "Sharpness" 1.0 1.0 3.0 1.0

#pragma parameter scanline_weight "Scanline Weight" 0.35 0.1 0.5 0.05
#pragma parameter lum "Luminance" 0.05 0.0 1.0 0.01

#pragma parameter interlace_detect "Interlacing" 1.0 0.0 1.0 1.0
#pragma parameter SATURATION "Saturation" 1.05 0.0 2.0 0.05


#ifndef PARAMETER_UNIFORM

#define CRTgamma 2.4
#define monitorgamma 2.2

#define d 1.5
#define CURVATURE 1.0
#define R 3.3

#define cornersize 0.015
#define cornersmooth 400.0

#define x_tilt 0.0
#define y_tilt 0.0

#define overscan_x 100.0
#define overscan_y 100.0

#define DOTMASK 0.15
#define SHARPER 1.0

#define scanline_weight 0.35
#define lum 0.05

#define interlace_detect 1.0
#define SATURATION 1.05

#define INV 0.0

#endif


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

#define COMPAT_PRECISION mediump

#else

#define COMPAT_PRECISION

#endif


COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;

COMPAT_VARYING vec4 COL0;
COMPAT_VARYING vec4 TEX0;


vec4 _oPosition1;

uniform mat4 MVPMatrix;

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;

uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;


COMPAT_VARYING vec2 overscan;
COMPAT_VARYING vec2 aspect;
COMPAT_VARYING vec3 stretch;
COMPAT_VARYING vec2 sinangle;
COMPAT_VARYING vec2 cosangle;
COMPAT_VARYING vec2 one;

COMPAT_VARYING float mod_factor;

COMPAT_VARYING vec2 ilfac;


#ifdef PARAMETER_UNIFORM

uniform COMPAT_PRECISION float CRTgamma;
uniform COMPAT_PRECISION float monitorgamma;

uniform COMPAT_PRECISION float d;
uniform COMPAT_PRECISION float CURVATURE;
uniform COMPAT_PRECISION float R;

uniform COMPAT_PRECISION float cornersize;
uniform COMPAT_PRECISION float cornersmooth;

uniform COMPAT_PRECISION float x_tilt;
uniform COMPAT_PRECISION float y_tilt;

uniform COMPAT_PRECISION float overscan_x;
uniform COMPAT_PRECISION float overscan_y;

uniform COMPAT_PRECISION float DOTMASK;
uniform COMPAT_PRECISION float SHARPER;

uniform COMPAT_PRECISION float scanline_weight;
uniform COMPAT_PRECISION float lum;

uniform COMPAT_PRECISION float interlace_detect;
uniform COMPAT_PRECISION float SATURATION;

#endif


#define FIX(c) max(abs(c), 1e-5);


float intersect(vec2 xy)
{
    float A =
        dot(xy, xy)
        + d * d;

    float B =
        2.0 *
        (
            R *
            (
                dot(xy, sinangle)
                - d *
                cosangle.x *
                cosangle.y
            )
            - d * d
        );

    float C =
        d * d
        + 2.0 *
        R *
        d *
        cosangle.x *
        cosangle.y;

    return
        (
            -B
            - sqrt(
                max(
                    B * B - 4.0 * A * C,
                    0.0
                )
            )
        )
        /
        (2.0 * A);
}


vec2 bkwtrans(vec2 xy)
{
    float c =
        intersect(xy);

    vec2 point =
        vec2(c) * xy;

    point -=
        vec2(-R) * sinangle;

    point /=
        vec2(R);

    vec2 tang =
        sinangle /
        cosangle;

    vec2 poc =
        point /
        cosangle;

    float A =
        dot(tang, tang)
        + 1.0;

    float B =
        -2.0 *
        dot(poc, tang);

    float C =
        dot(poc, poc)
        - 1.0;

    float a =
        (
            -B
            + sqrt(
                max(
                    B * B - 4.0 * A * C,
                    0.0
                )
            )
        )
        /
        (2.0 * A);

    vec2 uv =
        (
            point
            - a * sinangle
        )
        /
        cosangle;

    float r =
        R *
        acos(a);

    return
        uv *
        r /
        sin(r / R);
}


vec2 fwtrans(vec2 uv)
{
    float r =
        FIX(
            sqrt(
                dot(uv, uv)
            )
        );

    uv *=
        sin(r / R) /
        r;

    float x =
        1.0 -
        cos(r / R);

    float D =
        d / R
        + x *
        cosangle.x *
        cosangle.y
        + dot(uv, sinangle);

    return
        d *
        (
            uv *
            cosangle
            - x *
            sinangle
        )
        /
        D;
}


vec3 maxscale()
{
    vec2 c =
        bkwtrans(
            -R *
            sinangle
            /
            (
                1.0
                + R / d *
                cosangle.x *
                cosangle.y
            )
        );

    vec2 a =
        vec2(0.5, 0.5) *
        aspect;

    vec2 lo =
        vec2(
            fwtrans(
                vec2(-a.x, c.y)
            ).x,

            fwtrans(
                vec2(c.x, -a.y)
            ).y
        )
        /
        aspect;

    vec2 hi =
        vec2(
            fwtrans(
                vec2(+a.x, c.y)
            ).x,

            fwtrans(
                vec2(c.x, +a.y)
            ).y
        )
        /
        aspect;

    return vec3(
        (hi + lo) *
        aspect *
        0.5,

        max(
            hi.x - lo.x,
            hi.y - lo.y
        )
    );
}


void main()
{
    // ============================================================
    // MONIX CRT CONFIGURATION
    // ============================================================

    // CRT target gamma
    // Typical CRT phosphor / rendering target.
    //
    // Monix:
    // 2.4
    //

    // Monitor gamma
    // Standard modern display compensation.
    //
    // Monix:
    // 2.2
    //

    // Overscan
    //
    // 1.00 means no image cropping.
    //
    overscan =
        vec2(
            1.00,
            1.00
        );


    // Aspect ratio
    //
    // Preserve source aspect ratio.
    //
    aspect =
        vec2(
            1.0,
            1.0
        );


    // Simulated CRT distance.
    //
    // d = 1.5
    //
    // A relatively close simulated display.
    // This works well for a UI / terminal style CRT.


    // Curvature radius.
    //
    // R = 2.5
    //
    // Much less aggressive than R = 1.0.
    // Gives Monix a visible but controlled CRT curve.


    // Tilt.
    //
    // Keep the virtual display perfectly aligned.
    //
    const vec2 angle =
        vec2(
            0.0,
            0.0
        );


    // ============================================================
    // POSITION
    // ============================================================

    vec4 _oColor;

    vec2 _otexCoord;

    gl_Position =
        VertexCoord.x *
        MVPMatrix[0]
        +
        VertexCoord.y *
        MVPMatrix[1]
        +
        VertexCoord.z *
        MVPMatrix[2]
        +
        VertexCoord.w *
        MVPMatrix[3];

    gl_Position.x *= 4.0 * OutputSize.y / (3.0 * OutputSize.x);

    _oPosition1 =
        gl_Position;

    _oColor =
        COLOR;

    _otexCoord =
        TexCoord.xy *
        1.0001;

    COL0 =
        COLOR;

    TEX0.xy =
        TexCoord.xy *
        1.0001;


    // ============================================================
    // CURVATURE GEOMETRY
    // ============================================================

    sinangle =
        sin(
            vec2(
                x_tilt,
                y_tilt
            )
        )
        +
        vec2(
            0.001
        );

    cosangle =
        cos(
            vec2(
                x_tilt,
                y_tilt
            )
        )
        +
        vec2(
            0.001
        );


    stretch =
        maxscale();


    // ============================================================
    // INTERLACING
    // ============================================================

    ilfac =
        vec2(
            1.0,
            clamp(
                floor(
                    InputSize.y /
                    200.0
                ),
                1.0,
                2.0
            )
        );


    // ============================================================
    // SOURCE TEXEL SIZE
    // ============================================================

    vec2 sharpTextureSize =
        vec2(
            SHARPER *
            TextureSize.x,

            TextureSize.y
        );


    one =
        ilfac /
        sharpTextureSize;


    // ============================================================
    // HORIZONTAL MODULATION
    // ============================================================

    mod_factor =
        TexCoord.x *
        TextureSize.x *
        OutputSize.x /
        InputSize.x;
}


#elif defined(FRAGMENT)


#if __VERSION__ >= 130

#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture

out vec4 FragColor;

#else

#define COMPAT_VARYING varying

#define FragColor gl_FragColor

#define COMPAT_TEXTURE texture2D

#endif


#ifdef GL_ES

#ifdef GL_FRAGMENT_PRECISION_HIGH

precision highp float;

#else

precision mediump float;

#endif

#define COMPAT_PRECISION mediump

#else

#define COMPAT_PRECISION

#endif


struct output_dummy
{
    vec4 _color;
};


uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;

uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;


uniform sampler2D Texture;


COMPAT_VARYING vec4 TEX0;


// ============================================================
// LINEAR PROCESSING
// ============================================================
//
// Keep enabled.
//
// This gives more physically sensible gamma handling.
//
// ============================================================

#define LINEAR_PROCESSING


// ============================================================
// OVERSAMPLING
// ============================================================
//
// 3x beam oversampling.
//
// Important for keeping scanlines smooth at different
// resolutions and scaling factors.
//
// ============================================================

#define OVERSAMPLE


// Older Gaussian beam profile disabled.
//
// #define USEGAUSSIAN


// ============================================================
// MACROS
// ============================================================

#define FIX(c) max(abs(c), 1e-5);

#define PI 3.141592653589


#ifdef LINEAR_PROCESSING

#define TEX2D(c) \
    pow( \
        COMPAT_TEXTURE(Texture, (c)), \
        vec4(CRTgamma) \
    )

#else

#define TEX2D(c) \
    COMPAT_TEXTURE(Texture, (c))

#endif


COMPAT_VARYING vec2 one;

COMPAT_VARYING float mod_factor;

COMPAT_VARYING vec2 ilfac;

COMPAT_VARYING vec2 overscan;

COMPAT_VARYING vec2 aspect;

COMPAT_VARYING vec3 stretch;

COMPAT_VARYING vec2 sinangle;

COMPAT_VARYING vec2 cosangle;


#ifdef PARAMETER_UNIFORM

uniform COMPAT_PRECISION float CRTgamma;
uniform COMPAT_PRECISION float monitorgamma;

uniform COMPAT_PRECISION float d;
uniform COMPAT_PRECISION float CURVATURE;
uniform COMPAT_PRECISION float R;

uniform COMPAT_PRECISION float cornersize;
uniform COMPAT_PRECISION float cornersmooth;

uniform COMPAT_PRECISION float x_tilt;
uniform COMPAT_PRECISION float y_tilt;

uniform COMPAT_PRECISION float overscan_x;
uniform COMPAT_PRECISION float overscan_y;

uniform COMPAT_PRECISION float DOTMASK;
uniform COMPAT_PRECISION float SHARPER;

uniform COMPAT_PRECISION float scanline_weight;
uniform COMPAT_PRECISION float lum;

uniform COMPAT_PRECISION float interlace_detect;
uniform COMPAT_PRECISION float SATURATION;

uniform COMPAT_PRECISION float INV;

#endif


// ============================================================
// CURVATURE
// ============================================================

float intersect(vec2 xy)
{
    float A =
        dot(xy, xy)
        + d * d;

    float B =
        2.0 *
        (
            R *
            (
                dot(xy, sinangle)
                - d *
                cosangle.x *
                cosangle.y
            )
            - d * d
        );

    float C =
        d * d
        + 2.0 *
        R *
        d *
        cosangle.x *
        cosangle.y;

    return
        (
            -B
            -
            sqrt(
                max(
                    B * B -
                    4.0 * A * C,
                    0.0
                )
            )
        )
        /
        (2.0 * A);
}


vec2 bkwtrans(vec2 xy)
{
    float c =
        intersect(xy);

    vec2 point =
        vec2(c) *
        xy;

    point -=
        vec2(-R) *
        sinangle;

    point /=
        vec2(R);

    vec2 tang =
        sinangle /
        cosangle;

    vec2 poc =
        point /
        cosangle;

    float A =
        dot(tang, tang)
        + 1.0;

    float B =
        -2.0 *
        dot(poc, tang);

    float C =
        dot(poc, poc)
        - 1.0;

    float a =
        (
            -B
            +
            sqrt(
                max(
                    B * B -
                    4.0 * A * C,
                    0.0
                )
            )
        )
        /
        (2.0 * A);

    vec2 uv =
        (
            point
            - a *
            sinangle
        )
        /
        cosangle;

    float r =
        FIX(
            R *
            acos(a)
        );

    return
        uv *
        r /
        sin(r / R);
}


vec2 transform(vec2 coord)
{
    coord *=
        TextureSize /
        InputSize;

    coord =
        (
            coord -
            vec2(0.5)
        )
        *
        aspect *
        stretch.z
        +
        stretch.xy;

    return
        (
            bkwtrans(coord)
            /
            vec2(
                overscan_x / 100.0,
                overscan_y / 100.0
            )
            /
            aspect
            +
            vec2(0.5)
        )
        *
        InputSize /
        TextureSize;
}


// ============================================================
// CORNER MASK
// ============================================================

float corner(vec2 coord)
{
    coord *=
        TextureSize /
        InputSize;

    coord =
        (
            coord -
            vec2(0.5)
        )
        *
        vec2(
            overscan_x / 100.0,
            overscan_y / 100.0
        )
        +
        vec2(0.5);

    coord =
        min(
            coord,
            vec2(1.0) -
            coord
        )
        *
        aspect;

    vec2 cdist =
        vec2(
            cornersize
        );

    coord =
        cdist -
        min(
            coord,
            cdist
        );

    float dist =
        sqrt(
            dot(
                coord,
                coord
            )
        );

    return
        clamp(
            (
                cdist.x -
                dist
            )
            *
            cornersmooth,

            0.0,
            1.0
        )
        *
        1.0001;
}


// ============================================================
// SCANLINE PROFILE
// ============================================================

vec4 scanlineWeights(
    float distance,
    vec4 color)
{
    vec4 wid =
        2.0 +
        2.0 *
        pow(
            color,
            vec4(4.0)
        );

    vec4 weights =
        vec4(distance) /
        scanline_weight;

    return
        (
            lum +
            1.4
        )
        *
        exp(
            -pow(
                weights *
                inversesqrt(
                    0.5 *
                    wid
                ),
                wid
            )
        )
        /
        (
            0.6 +
            0.2 *
            wid
        );
}


// ============================================================
// SATURATION
// ============================================================

vec3 saturation(
    vec3 textureColor)
{
    float lumValue =
        length(
            textureColor
        )
        *
        0.5775;

    vec3 luminanceWeighting =
        vec3(
            0.3,
            0.6,
            0.1
        );

    if (lumValue < 0.5)
    {
        luminanceWeighting.rgb =
            (
                luminanceWeighting.rgb *
                luminanceWeighting.rgb
            )
            +
            (
                luminanceWeighting.rgb *
                luminanceWeighting.rgb
            );
    }

    float luminance =
        dot(
            textureColor,
            luminanceWeighting
        );

    vec3 greyScaleColor =
        vec3(
            luminance
        );

    return
        vec3(
            mix(
                greyScaleColor,
                textureColor,
                SATURATION
            )
        );
}


// ============================================================
// GAMMA
// ============================================================

#define pwr \
vec3( \
    1.0 / \
    ( \
        (-0.7 * \
        (1.0 - scanline_weight) + 1.0) \
        * \
        (-0.5 * DOTMASK + 1.0) \
    ) \
    - 1.25 \
)


// ============================================================
// INVERSE GAMMA
// ============================================================

vec3 inv_gamma(
    vec3 col,
    vec3 power)
{
    vec3 cir =
        col -
        1.0;

    cir *=
        cir;

    col =
        mix(
            sqrt(col),
            sqrt(
                1.0 -
                cir
            ),
            power
        );

    return col;
}


// ============================================================
// MAIN
// ============================================================

void main()
{
    // ============================================================
    // COORDINATES
    // ============================================================

    vec2 xy =
        (
            CURVATURE > 0.5
        )
        ?
        transform(TEX0.xy)
        :
        TEX0.xy;


    // ============================================================
    // CRT CORNERS
    // ============================================================

    float cval =
        corner(xy);


    // ============================================================
    // INTERLACING
    // ============================================================

    vec2 ilvec =
        vec2(
            0.0,

            ilfac.y *
            interlace_detect
            > 1.5
            ?
            mod(
                float(FrameCount),
                2.0
            )
            :
            0.0
        );


    vec2 ratio_scale =
        (
            xy *
            TextureSize
            -
            vec2(0.5)
            +
            ilvec
        )
        /
        ilfac;


#ifdef OVERSAMPLE

    float filter_ =
        InputSize.y /
        OutputSize.y;

#endif


    vec2 uv_ratio =
        fract(
            ratio_scale
        );


    // ============================================================
    // SNAP TO SOURCE TEXEL
    // ============================================================

    xy =
        (
            floor(
                ratio_scale
            )
            *
            ilfac
            +
            vec2(0.5)
            -
            ilvec
        )
        /
        TextureSize;


    // ============================================================
    // LANCZOS2
    // ============================================================

    vec4 coeffs =
        PI *
        vec4(
            1.0 + uv_ratio.x,
            uv_ratio.x,
            1.0 - uv_ratio.x,
            2.0 - uv_ratio.x
        );


    coeffs =
        FIX(
            coeffs
        );


    coeffs =
        2.0 *
        sin(coeffs) *
        sin(coeffs / 2.0)
        /
        (
            coeffs *
            coeffs
        );


    coeffs /=
        dot(
            coeffs,
            vec4(1.0)
        );


    // ============================================================
    // CURRENT SCANLINE
    // ============================================================

    vec4 col =
        clamp(
            mat4(
                TEX2D(
                    xy -
                    vec2(
                        one.x,
                        0.0
                    )
                ),

                TEX2D(
                    xy
                ),

                TEX2D(
                    xy +
                    vec2(
                        one.x,
                        0.0
                    )
                ),

                TEX2D(
                    xy +
                    vec2(
                        2.0 *
                        one.x,
                        0.0
                    )
                )
            )
            *
            coeffs,

            0.0,
            1.0
        );


    // ============================================================
    // NEXT SCANLINE
    // ============================================================

    vec4 col2 =
        clamp(
            mat4(
                TEX2D(
                    xy +
                    vec2(
                        -one.x,
                        one.y
                    )
                ),

                TEX2D(
                    xy +
                    vec2(
                        0.0,
                        one.y
                    )
                ),

                TEX2D(
                    xy +
                    one
                ),

                TEX2D(
                    xy +
                    vec2(
                        2.0 *
                        one.x,
                        one.y
                    )
                )
            )
            *
            coeffs,

            0.0,
            1.0
        );


#ifndef LINEAR_PROCESSING

    col =
        pow(
            col,
            vec4(CRTgamma)
        );

    col2 =
        pow(
            col2,
            vec4(CRTgamma)
        );

#endif


    // ============================================================
    // SCANLINE WEIGHTS
    // ============================================================

    vec4 weights =
        scanlineWeights(
            uv_ratio.y,
            col
        );

    vec4 weights2 =
        scanlineWeights(
            1.0 -
            uv_ratio.y,
            col2
        );


#ifdef OVERSAMPLE

    uv_ratio.y =
        uv_ratio.y
        +
        1.0 / 3.0 *
        filter_;


    weights =
        (
            weights
            +
            scanlineWeights(
                uv_ratio.y,
                col
            )
        )
        /
        3.0;


    weights2 =
        (
            weights2
            +
            scanlineWeights(
                abs(
                    1.0 -
                    uv_ratio.y
                ),
                col2
            )
        )
        /
        3.0;


    uv_ratio.y =
        uv_ratio.y
        -
        2.0 / 3.0 *
        filter_;


    weights +=
        scanlineWeights(
            abs(
                uv_ratio.y
            ),
            col
        )
        /
        3.0;


    weights2 +=
        scanlineWeights(
            abs(
                1.0 -
                uv_ratio.y
            ),
            col2
        )
        /
        3.0;

#endif


    // ============================================================
    // COMBINE SCANLINES
    // ============================================================

    vec3 mul_res =
        (
            col *
            weights
            +
            col2 *
            weights2
        )
        .rgb
        *
        vec3(
            cval
        );


    // ============================================================
    // DOT MASK
    //
    // Reduced from 0.30 -> 0.15 for Monix.
    //
    // This keeps the RGB structure visible without making
    // text/logs look like they're behind a colored grid.
    // ============================================================

    vec3 dotMaskWeights =
        mix(
            vec3(
                1.0,
                1.0 - DOTMASK,
                1.0
            ),

            vec3(
                1.0 - DOTMASK,
                1.0,
                1.0 - DOTMASK
            ),

            floor(
                mod(
                    mod_factor,
                    2.0
                )
            )
        );


    mul_res *=
        dotMaskWeights;


    // ============================================================
    // DISPLAY GAMMA
    // ============================================================

    if (INV == 1.0)
    {
        mul_res =
            inv_gamma(
                mul_res,
                pwr
            );
    }
    else
    {
        mul_res =
            pow(
                mul_res,
                vec3(
                    1.0 /
                    monitorgamma
                )
            );
    }


    // ============================================================
    // SATURATION
    // ============================================================

    mul_res =
        saturation(
            mul_res
        );


    // ============================================================
    // OUTPUT
    // ============================================================

    FragColor =
        vec4(
            mul_res,
            1.0
        );
}


#endif
```
