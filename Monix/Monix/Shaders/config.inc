#ifndef CONFIG_INC
#define CONFIG_INC

// Mathematical constants
const float eps = 0.0001;
const float pi = 3.14159265358979323846;
const float TAU = 6.28318530717958647692;

// Colorspace matrices (column-major for GLSL)
const mat3 RGB2YIQ = mat3(
    0.299, 0.587, 0.114,
    0.595716, -0.274453, -0.321263,
    0.211456, -0.522591, 0.311135
);
const mat3 YIQ2RGB = mat3(
    1.0, 1.0, 1.0,
    0.9563, 0.2721, -1.1070,
    0.6210, -0.6474, -1.7030
);

const mat3 RGB2YUV = mat3(
    0.299, -0.14713, 0.615,
    0.587, -0.28886, -0.51499,
    0.114, 0.436, -0.10001
);
const mat3 YUV2RGB = mat3(
    1.0, 1.0, 1.0,
    0.0, -0.34413, 1.772,
    1.402, -0.71414, 0.0
);

// Sampling points on the avglum texture (packed into alpha channel)
#define TSMOOTH_NOCC_SAMPLING_POINT    vec2(0.25, 0.25)
#define DYN_ZOOM_SAMPLING_POINT        vec2(0.75, 0.25)
#define AUTOCROP_SAMPLING_POINT        vec2(0.25, 0.75)
#define RESSWITCH_SAMPLING_POINT       vec2(0.75, 0.75)
#define FULL_SCENE_LUM_SAMPLING_POINT  vec2(0.5, 0.5)

// Feature enable/disable flags (DO_*) - defaults to enabled where standard
#define DO_BLOOM 1.0
#define DO_HALO 0.0
#define DO_BEZEL 1.0
#define DO_AMBILIGHT 0.0
#define DO_IN_GLOW 0.0
#define DO_CCORRECTION 1.0
#define DO_PIXELGRID 0.0
#define DO_PIXELGRID_H 0.0
#define DO_PIXELGRID_W 0.0
#define DO_DOT_MATRIX 0.0
#define DO_FXAA 0.0
#define DO_TILT 0.0
#define DO_VIGNETTE 0.0
#define DO_SPOT 0.0
#define DO_CURVATURE 0.0
#define DO_CVBS_BLEED 0.0
#define DO_SHIFT_RGB 0.0
#define DO_GAME_GEOM_OVERRIDE 0.0
#define DO_GLOBAL_SHZO 0.0
#define DO_BG_IMAGE 0.0
#define BG_IMAGE_OVER 0.0
#define DO_BACKDROP 0.0
#define DO_NTSC_ARTIFACTS 0.0
#define DO_DEDITHER 0.0
#define DO_TEST 0.0
#define DO_DYNZOOM 0.0
#define DO_PPERSISTENCE 0.0
#define DO_WTFL 0.0
#define DO_RF_NOISE 0.0

// Bloom parameters
#define BLOOM_SIZE 1.0
#define BLOOM_QUALITY 5.0
#define BLOOM_GAMMA 2.0
#define BLOOM_GAMMA_OUT 1.0
#define BLOOM_POWER 1.0
#define BLOOM_MIX 1.0
#define BLOOM_EYE_ADPT_SRT 0.0
#define BLOOM_SOFT_LIMITER 0.0
#define BLOOM_HALATION_MIX 0.0
#define BLOOM_OVER_WHITE 0.0
#define BLOOM_BYPASS 0.0

// Halo parameters
#define HALO_SHARPNESS 1.0
#define HALO_POWER 1.0
#define HALO_GAMMA 1.0
#define HALO_GAMMA_OUT 1.0
#define HALO_NO_PREGAIN 0.0
#define HALO_VS_SCAN 0.0
#define GLOW_SHARP_MAX 1.0

// Glow parameters
#define IN_GLOW_W 1.0
#define IN_GLOW_H 1.0
#define IN_GLOW_BIAS 0.0
#define IN_GLOW_BIAS_MAX 1.0
#define IN_GLOW_POWER 1.0
#define IN_GLOW_GAMMA 1.0
#define IN_GLOW_SPREAD 1.0
#define IN_GLOW_SPREAD_SHARPNESS 1.0
#define MIN_IN_GLOW_SHARP 0.0
#define IN_GLOW_WARPSHARP_X 0.0
#define IN_GLOW_WARPSHARP_Y 0.0
#define IN_GLOW_WARPSHARP_GAMMA 1.0

// Pixelgrid parameters
#define PIXELGRID_SIZE_W 1.0
#define PIXELGRID_TVL_X 1.0
#define PIXELGRID_MUL_X 1.0
#define PIXELGRID_MIN_H 0.0
#define PIXELGRID_MAX_H 1.0
#define PIXELGRID_MIN_W 0.0
#define PIXELGRID_MAX_W 1.0
#define PIXELGRID_GAMMA_H 1.0
#define PIXELGRID_GAMMA_W 1.0
#define PIXELGRID_NO_INTERBLEED_H 0.0
#define PIXELGRID_OVERMASK 0.0
#define PIXELGRID_BASAL_GRID 0.0
#define PIXELGRID_Y_MASK 0.0
#define PIXELGRID_Y_MASK_HEIGHT 1.0
#define PIXELGRID_Y_MASK_STEEP 1.0
#define PIXELGRID_Y_MASK_OFFSET 0.0
#define PIXELGRID_Y_MASK_SHIFT 0.0
#define PIXELGRID_Y_MASK_COORDS 0.0
#define PIXELGRID_Y_MASK_ON_WHITE 0.0
#define PIXELGRID_Y_MASK_OFFSET_DEHIVE 0.0
#define PIXELGRID_Y_SPARK 0.0
#define PIXELGRID_DO_SHADOWMASK 0.0
#define PIXELGRID_SHADOWMASK_H 1.0
#define PIXELGRID_SHADOWMASK_SHIFT 0.0
#define PIXELGRID_OFFSET_CORE 0.0
#define PIXELGRID_OFFSET_CORE_W 0.0
#define PIXELGRID_INTR_FLICK_POWR 0.0
#define PIXELGRID_INTR_DISABLE_Y 0.0
#define PIXELGRID_INTR_GAMMA_ADJ 1.0
#define PIXELGRID_INTR_PAST_FIELD 0.0
#define PIXELGRID_H_DEDOT 0.0

// Dot matrix parameters
#define DOT_M_SHADOW_OFF 0.0
#define DOT_M_SHADOW_STR 1.0
#define DOT_M_G_SHARP 1.0
#define DOT_M_G_BRT 1.0
#define DOT_M_G_STR 1.0
#define DOT_M_G_TRESH 0.5
#define DOT_M_SHARP_AUTO_MIN 0.0
#define DOT_M_SHARP_AUTO_MAX 1.0
#define DOT_M_MBLUR_STR 0.0
#define DOT_M_MBLUR_MODE 0.0
#define DOT_C 0.0
#define DOT_C_SPEED 1.0

// Color correction / NTSC parameters
#define CONTRAST 1.0
#define BRIGHTNESS 0.0
#define TEMPERATURE 6500.0
#define GAMMA_OUT 1.0
#define NTSC_FILTER_FC 0.0
#define NTSC_FILTER_WIDTH 5.0
#define NTSC_FILTER_SCF 1.0
#define NTSC_PHASE_SHIFT 0.0
#define NTSC_MIX 0.0
#define NTSC_ARTF_TRSH 0.5
#define NTSC_ARTF_ZAP 0.0
#define NTSC_SHOW_ARTF_MASK 0.0

// Deconvergence parameters
#define DECON_RX 0.0
#define DECON_RY 0.0
#define DECON_GX 0.0
#define DECON_GY 0.0
#define DECON_BX 0.0
#define DECON_BY 0.0
#define EDGE_DEFOCUS 0.0
#define EDGE_DEFOCUS_DO_BLUR 0.0
#define EDGE_DEFOCUS_AREA_INV 1.0

// Geometry / Scaling parameters
#define GEOM_WARP_X 0.0
#define GEOM_WARP_Y 0.0
#define GAME_GEOM_ASPECT 1.0
#define GAME_GEOM_VSHIFT 0.0
#define GAME_GEOM_HSHIFT 0.0
#define GAME_GEOM_ZOOM 1.0
#define GAME_GEOM_INT_SCALE 0.0
#define NEW_SCALEMOD_X 1.0
#define NEW_SCALEMOD_Y 1.0
#define AUTOCROP_MAX 1.0
#define AUTOCROP_MIN 0.0
#define AUTOCROP_STEADINESS 0.0
#define AUTOCROP_SAMPLES 1.0
#define AUTOCROP_SAMPLE_SIZE 1.0
#define AUTOCROP_TRANSITION_SPEED 1.0

// Bezel / Reflection parameters
#define BEZEL_RFL_ZOOM 1.0
#define BEZEL_REFL_SKIP_SIZE 0.0
#define BEZEL_REFL_BLR_SHD 1.0
#define BEZEL_REFL_ZOOMOUT_ROOM 1.0
#define BEZEL_REFL_FADEOUT_START 0.0
#define BEZEL_REFL_STRENGTH 1.0
#define BEZEL_REFL_CONTRAST 1.0
#define BEZEL_RFL_CONTRAST 1.0
#define BEZEL_RFL_BLR_SHD 0.0
#define BEZEL_LIGHT_NO_FADEOUT 0.0
#define BEZEL_CORNER_DARK 0.0
#define BEZEL_DIFFUSION_STR 0.0
#define BEZEL_FRAME_ZOOM 1.0
#define BEZEL_USE_STRAIGHT 0.0
#define BEZEL_SPCL_STRENGTH 0.0
#define BEZEL_ROUGHNESS 0.0
#define BEZEL_TUBE_BLANK_SIZE 0.0
#define BEZEL_FRAME_SURROUND_MARGIN 0.0
#define BEZEL_R 0.5
#define BEZEL_G 0.5
#define BEZEL_B 0.5
#define BEZEL_CON 1.0

// CVBS parameters
#define CVBS_BLEED_SAMPLES 5.0
#define CVBS_BLEED_PAL 0.0
#define CVBS_BLEED_ADJUST 1.0
#define CVBS_BLEED_COARSE 0.0
#define CVBS_UNSHARP_COMPENSATION 0.0

// Vignette / Spot / Uniformity parameters
#define V_SHAPE 1.0
#define V_SIZE 1.0
#define UNIFORMITY 1.0
#define S_POSITION_X 0.5
#define S_POSITION_Y 0.5
#define S_SIZE 0.5
#define S_POWER 1.0

// Tilt
#define TILT_X 0.0

// Ambient light parameters
#define AMBI_SMP_INT_OFFSET 0.0
#define AMBI_STRETCH 1.0
#define AMBI_STRETCH_VERTICAL 0.0
#define AMBI_GAMMA 1.0
#define AMBI_POWER 1.0
#define AMBI_FALLOFF 1.0
#define AMBI_FAST_STEP 0.0
#define AMBI_INT_OFFSET 0.0
#define AMBI_SCENE_CHG_THRSHLD 0.0
#define AMBI_STEPS 1.0
#define AMBI_OVER_BEZEL 0.0
#define AMBI_OVER_BEZEL_SIZE 1.0
#define AMBI_OVER_BEZEL_AR_CORRECTION 0.0
#define AMBI_BG_IMAGE_BLEND_MODE 0.0
#define AMBI_BG_IMAGE_FORCE 0.0
#define AMBI_ADD_ON_BLACK 0.0

// Background image parameters
#define BG_IMAGE_ROTATION 0.0
#define BG_IMAGE_OFFX 0.0
#define BG_IMAGE_OFFY 0.0
#define BG_IMAGE_ZOOM 1.0
#define BG_IMAGE_NIGHTIFY 0.0
#define BG_IMAGE_WRAP_MODE 0.0
#define BACKDROP_JUST_TUBE 0.0
#define BACKDROP_BLEND_MODE 0.0

// Persistence / Motion parameters
#define PPERSISTENCE_START 0.0
#define PPERSISTENCE_END 1.0
#define PPERSISTENCE_MUL_R 1.0
#define PPERSISTENCE_MUL_G 1.0

// Resolution / Output parameters
#define RESSWITCH_SYNC_SPEED 1.0
#define RESSWITCH_GLITCH_TRESHOLD 0.5
#define SCANLINES_FREQUENCY_MULTIPLIER_OVERRIDE 1.0
#define SERVICE1 0.0

// Dithering / Dedither parameters
#define DEDITHER_TRESHMAX 1.0
#define DEDITHER_STRENGTH 1.0
#define DEDITHER_STRENGTH2 1.0

// Waterfall parameters
#define WTFL_SHARPNESS_X 1.0
#define WTFL_SHARPNESS_Y 1.0

// Noise / RF parameters
#define RF_NOISE_STRENGTH 0.0
#define NOISEPOWER_AMBI 0.0

// Adaptive Strobe / LCD parameters
#define ADPT_LCD_RETENTION_FIX 0.0
#define LCD_ANTIGHOSTING 0.0
#define LCD_ANTIGHOSTING_CEIL 1.0

// Debug / FPS parameters
#define FPS_MEAN 60.0
#define ADAPTIVE_BLACK 0.0

// Luminance weight
#define LUMINANCE 0.3

// Global offset/zoom
#define GLOBAL_OFFX 0.0
#define GLOBAL_OFFY 0.0
#define GLOBAL_ZOOM 1.0

// Compile-time flags (comment out to disable)
// #define D3D_WORKAROUND
// #define _HAS_ROTATION_UNIFORM
// #define _HAS_FRAMETIME_UNIFORMS
// #define SKIP_WARPEDGLOW
// #define HQ_DECON
// #define FLICKER_IN_MOTION
// #define ORIGINAL_FPS_UNTRUSTED
// #define SKIP_RANDOM
// #define RENDER_BOGUS_FRAMES
// #define ANTIBURN_COMPLETE
// #define ANTIBURN_X
// #define USE_LED1
// #define USE_LED2
// #define USE_LED3
// #define _USE_DENISE_EMU
// #define _DEBUG_DENISE_EMU
// #define DO_ADPT_STROBE_PLUGIN
// #define DO_LCD_ANTIGHOSTING_PLUGIN
// #define HALVE_BORDER_UPDATE
// #define SUBFRAMES_OPTIMIZATIONS
// #define DISALLOW_BG_IMAGE_TEXTURE_WRAP_IN_SHADER
#define BEZEL_RES vec2(1920.0, 1080.0)

// Mega Bezel framework uniform structures
struct GlobalData {
    mat4 MVP;
    vec4 FinalViewportSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    vec4 SourceSize;
    float FrameTimeDelta;
    float OriginalFPS;
    float TotalSubFrames;
    float LedDriveState;
    float LED1_X;
    float LED1_Y;
    float LED1_SIZE;
    float LED1_SHAPE;
    float LED1_ASPECT;
    float LED1_EXPOSURE;
    float LED1_HUE;
    float LED2_X;
    float LED2_Y;
    float LED2_SIZE;
    float LED2_SHAPE;
    float LED2_ASPECT;
    float LED2_EXPOSURE;
    float LED2_HUE;
    float LED3_X;
    float LED3_Y;
    float LED3_SIZE;
    float LED3_SHAPE;
    float LED3_ASPECT;
    float LED3_EXPOSURE;
    float LED3_HUE;
    vec4 colortools_and_ntsc_passSize;
    vec4 colortools_and_ntsc_passFeedbackSize;
    vec4 flick_and_noise_passSize;
    vec4 upscale_passSize;
};

uniform GlobalData global;

struct ParamsData {
    vec4 OriginalSize;
    vec4 SourceSize;
    vec4 OutputSize;
    float FrameCount;
    float Rotation;
    float STATIC_SHARPNESS;
    float DYNAMIC_SHARPNESS_MIN;
    float DYNAMIC_SHARPNESS_MAX;
    float USE_DYNAMIC_SHARPNESS;
    float USE_SHARPENING_BIAS;
    float DDT_THRESHOLD;
    vec4 avglum_passFeedbackSize;
};

uniform ParamsData params;

// Additional identifiers not already defined above
#define BEZEL_ASPECT_CORRECTION 0.0
#define BLOOM_PUPIL_EXPOSURE_CURVE_SHOULDER 0.0
#define BLOOM_PUPIL_LUM_ZONE_W 0.0
#define PIXELGRID_H_PRST 5.0

#endif // CONFIG_INC

