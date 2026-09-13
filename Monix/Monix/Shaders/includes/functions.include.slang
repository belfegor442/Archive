#ifndef FUNCTIONS_INCLUDE_SLANG
#define FUNCTIONS_INCLUDE_SLANG

// ============================================================================
// Math Helpers
// ============================================================================

float lerp(float a, float b, float t) { return mix(a, b, t); }
vec2 lerp(vec2 a, vec2 b, float t) { return mix(a, b, t); }
vec3 lerp(vec3 a, vec3 b, float t) { return mix(a, b, t); }

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec2 saturate(vec2 x) { return clamp(x, 0.0, 1.0); }
vec3 saturate(vec3 x) { return clamp(x, 0.0, 1.0); }

float max3(vec3 v) { return max(v.x, max(v.y, v.z)); }
float min3(vec3 v) { return min(v.x, min(v.y, v.z)); }

// ============================================================================
// Color Space Conversions
// ============================================================================

vec3 rgb2yiq(vec3 rgb) { return RGB2YIQ * rgb; }
vec3 yiq2rgb(vec3 yiq) { return YIQ2RGB * yiq; }

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 kelvin2rgb(float kelvin) {
    float temp = kelvin / 100.0;
    vec3 color;
    if (temp <= 66.0) {
        color.r = 1.0;
        color.g = clamp(99.4708025861 * log(temp) - 161.1195681661, 0.0, 255.0) / 255.0;
        if (temp <= 19.0)
            color.b = 0.0;
        else
            color.b = clamp(138.5177312231 * log(temp - 10.0) - 305.0447927307, 0.0, 255.0) / 255.0;
    } else {
        color.r = clamp(329.698727446 * pow(temp - 60.0, -0.1332047592), 0.0, 255.0) / 255.0;
        color.g = clamp(288.1221695283 * pow(temp - 60.0, -0.0755148492), 0.0, 255.0) / 255.0;
        color.b = 1.0;
    }
    return color;
}

vec3 color_tools(vec3 pixel, vec3 temperature_rgb) {
    return pixel * temperature_rgb;
}

float rgb_to_gray(vec3 rgb) {
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

float saturation_get(vec3 c) {
    return max3(c) - min3(c);
}

// ============================================================================
// Coordinate / Geometry
// ============================================================================

vec2 zoom(vec2 co, float z) {
    return (co - 0.5) / z + 0.5;
}

vec2 zoomxy(vec2 co, vec2 z) {
    return (co - 0.5) / z + 0.5;
}

float zoom1D(float x, float z) {
    return (x - 0.5) / z + 0.5;
}

vec2 zoomout_coords(vec2 co, float amount) {
    return (co - 0.5) / (1.0 + amount) + 0.5;
}

vec2 tilt(vec2 co, bool isRotated, float tilt_x) {
    vec2 c = co - 0.5;
    float aspect = 1.0;
    c.y *= 1.0 + c.x * c.x * abs(tilt_x) * aspect;
    c.x *= 1.0 + c.y * c.y * abs(tilt_x) * 0.3;
    if (tilt_x < 0.0) c.y = -c.y;
    return c + 0.5;
}

vec2 tilt(vec2 co, float isRotated, float tilt_x) {
    return tilt(co, isRotated > 0.5, tilt_x);
}

vec2 tilt_bezel(vec2 co, float isrotated, float tilt_x) {
    return tilt(co, isrotated > 0.5, tilt_x);
}

vec2 Warp_koko(vec2 co, vec2 warp, float aspect) {
    vec2 cc = co - 0.5;
    vec2 warp_abs = abs(warp);
    cc *= 1.0 + pow(abs(cc.yx), vec2(2.0)) * warp_abs * sign(warp);
    return cc + 0.5;
}

float curvature_needed() {
    return 0.0;
}

float get_in_aspect() {
    return 1.0;
}

bool is_rotated() {
    return false;
}

float is_rotated_old() {
    return 0.0;
}

float need_tate(bool isrotated) {
    return isrotated ? 1.0 : 0.0;
}

bool need_NO_integer_scale() {
    return false;
}

bool need_integer_scale() {
    return false;
}

vec2 get_scaled_coords_aspect(vec2 co, vec2 viewportSize, float aspect, bool isrotated) {
    return co;
}

vec2 get_scaled_coords_aspect(vec2 co, vec4 viewportSize, float aspect, bool isrotated) {
    return co;
}

vec2 get_scaled_coords_aspect_fgbg(vec2 co, vec2 viewportSize, float aspect, bool isrotated, float bg, float fg) {
    return co;
}

vec2 get_scaled_coords_aspect_fgbg(vec2 co, vec4 viewportSize, float aspect, bool isrotated, float bg, float fg) {
    return co;
}

vec2 integer_scale(vec2 co, float aspect, bool isrotated, float doTate, float intscale) {
    return co;
}

vec2 content_geom_override_zoom(vec2 co, float aspect, float in_aspect, float vshift, float hshift, float zoom_amt) {
    co = co - vec2(hshift, vshift);
    return zoom(co, zoom_amt);
}

vec2 content_geom_override_shift(vec2 co, float aspect, float in_aspect, float vshift, float hshift, float zoom_amt) {
    co = co - vec2(hshift, vshift);
    return co;
}

vec2 content_geom_override_zoom_inv(vec2 co, float aspect, float in_aspect, float vshift, float hshift, float zoom_amt) {
    co = zoom(co, 1.0 / zoom_amt);
    co = co + vec2(hshift, vshift);
    return co;
}

vec2 overlay_image_coords(vec2 co) {
    return co;
}

float fn_border(vec2 co) {
    vec2 b = smoothstep(0.0, 0.05, co) * smoothstep(0.0, 0.05, 1.0 - co);
    return b.x * b.y;
}

bool is_first_outside_rect(vec2 co, vec4 rect) {
    return co.x < rect.x || co.x > rect.z || co.y < rect.y || co.y > rect.w;
}

bool is_first_inside_rect(vec2 co, vec4 rect) {
    return co.x >= rect.x && co.x <= rect.z && co.y >= rect.y && co.y <= rect.w;
}

vec2 antiburn(vec2 co) {
    return co;
}

vec2 antiburn_screen(vec2 co) {
    return co;
}

float antiburn_flip_even(float is_even) {
    return is_even;
}

// ============================================================================
// Blur Functions
// ============================================================================

vec2 radius_blur_adapt(vec2 radius) {
    return radius;
}

vec2 radius_blur_adapt(float radius) {
    return vec2(radius, radius);
}

vec2 radius_blur_adapt_ext_inaspect(vec2 radius, float aspect) {
    return vec2(radius.x, radius.y * aspect);
}

vec2 radius_blur_adapt_ext_inaspect(float radius, float aspect) {
    return vec2(radius, radius * aspect);
}

vec3 blur9_y(sampler2D smp, vec2 co, vec2 size, float sharpness) {
    vec2 d = vec2(0.0, size.y);
    vec3 c = texture(smp, co - d * 2.0).rgb * 0.05;
    c += texture(smp, co - d).rgb * 0.25;
    c += texture(smp, co).rgb * 0.4;
    c += texture(smp, co + d).rgb * 0.25;
    c += texture(smp, co + d * 2.0).rgb * 0.05;
    return c;
}

vec3 blur9_y_box(sampler2D smp, vec2 co, vec2 size, float sharpness, float sharp_sub) {
    return blur9_y(smp, co, size, sharpness);
}

vec3 blur9_x(sampler2D smp, vec2 co, vec2 size, float sharpness) {
    vec2 d = vec2(size.x, 0.0);
    vec3 c = texture(smp, co - d * 2.0).rgb * 0.05;
    c += texture(smp, co - d).rgb * 0.25;
    c += texture(smp, co).rgb * 0.4;
    c += texture(smp, co + d).rgb * 0.25;
    c += texture(smp, co + d * 2.0).rgb * 0.05;
    return c;
}

vec3 blur9_x_box(sampler2D smp, vec2 co, vec2 size, float sharpness, float sharp_sub) {
    return blur9_x(smp, co, size, sharpness);
}

vec3 blur_px(sampler2D smp, vec2 co, vec2 sharpness) {
    vec2 d = sharpness * vec2(0.001, 0.001);
    vec3 c = texture(smp, co - d).rgb * 0.25;
    c += texture(smp, co).rgb * 0.5;
    c += texture(smp, co + d).rgb * 0.25;
    return c;
}

vec3 blur5_x(sampler2D smp, vec2 co, vec2 size, float sharpness) {
    vec2 d = vec2(size.x, 0.0);
    vec3 c = texture(smp, co - d * 2.0).rgb * 0.1;
    c += texture(smp, co - d).rgb * 0.2;
    c += texture(smp, co).rgb * 0.4;
    c += texture(smp, co + d).rgb * 0.2;
    c += texture(smp, co + d * 2.0).rgb * 0.1;
    return c;
}

// ============================================================================
// Texture Sampling
// ============================================================================

vec4 texture_NEAREST(sampler2D smp, vec2 co, vec4 size) {
    vec2 snapped = floor(co * size.xy + 0.5) * size.zw;
    return texture(smp, snapped);
}

vec4 texture_NEAREST_lod(sampler2D smp, vec2 co, vec4 size, float lod) {
    vec2 snapped = floor(co * size.xy + 0.5) * size.zw;
    return textureLod(smp, snapped, lod);
}

vec2 coords_NEAREST(vec2 co, vec4 size) {
    return floor(co * size.xy + 0.5) * size.zw;
}

// ============================================================================
// Pixel Manipulation
// ============================================================================

vec3 pixel_push_luminance(vec3 c, float power) {
    float lum = rgb_to_gray(c);
    return mix(c, c * (lum + eps), power);
}

vec3 clip_to_white_maybe(vec3 c) {
    return clamp(c, 0.0, 1.0);
}

vec3 soft_limiter01_chroma_safe(vec3 c, float limit, float softness) {
    float lum = rgb_to_gray(c);
    vec3 chroma = c - lum;
    float limited = clamp(lum, 0.0, limit);
    float soft = mix(lum, limited, smoothstep(limit - softness, limit, lum));
    return soft + chroma;
}

vec3 adaptive_black(vec3 pixel, float avglum, float strength, bool mode) {
    float lum = rgb_to_gray(pixel);
    float factor = clamp((lum - 0.0) / (avglum + 0.001), 0.0, 1.0);
    return pixel * mix(1.0, factor, strength);
}

vec3 saturation_set_gain(vec3 c, float gain) {
    float lum = rgb_to_gray(c);
    return mix(vec3(lum), c, gain);
}

float scale_to_range(float x, float lo, float hi) {
    return (x - lo) / (hi - lo + eps);
}

vec2 scale_to_range_vec2(vec2 x, float lo, float hi) {
    return (x - lo) / (hi - lo + eps);
}

vec3 scale_to_range_vec3(vec3 x, float lo, float hi) {
    return (x - lo) / (hi - lo + eps);
}

float normalize_range(float x, float lo, float hi) {
    return (x - lo) / (hi - lo + eps);
}

float map_range(float x, float in_min, float in_max, float out_min, float out_max) {
    return out_min + (x - in_min) * (out_max - out_min) / (in_max - in_min + eps);
}

vec3 map_range(vec3 x, float in_min, float in_max, float out_min, float out_max) {
    return out_min + (x - in_min) * (out_max - out_min) / (in_max - in_min + eps);
}

float mix_step(float a, float b, float t) {
    return mix(a, b, step(0.5, t));
}

vec2 mix_step(vec2 a, vec2 b, float t) {
    return mix(a, b, step(0.5, t));
}

vec3 mix_step(vec3 a, vec3 b, float t) {
    return mix(a, b, step(0.5, t));
}

float apply_contrast_brightness(float x, float contrast, float brightness) {
    return (x - 0.5) * contrast + 0.5 + brightness;
}

vec2 smoothstep_fast(vec2 edge0, vec2 edge1, vec2 x) {
    vec2 t = clamp((x - edge0) / (edge1 - edge0 + eps), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

float smoothstep_fast(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0 + eps), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

vec2 unpack_21(float packed, float scale) {
    return unpackUnorm2x16(floatBitsToUint(packed)) * scale;
}

float pack_21(vec2 v) {
    return uintBitsToFloat(packUnorm2x16(clamp(v, 0.0, 1.0)));
}

// ============================================================================
// Delta Render
// ============================================================================

bool bdelta_render_enabled() { return false; }
float delta_render_enabled() { return 0.0; }

float delta_render_changed(sampler2D feedback, sampler2D current, vec2 co, vec2 check_area) {
    vec3 prev = texture(feedback, co).rgb;
    vec3 curr = texture(current, co).rgb;
    return distance(prev, curr);
}

vec2 delta_render_get_check_area() { return vec2(1.0); }
float is_deltarender_allowed(bool is_interlaced) { return 0.0; }
bool deltarender_preconditions_met(bool is_interlaced) { return false; }

// ============================================================================
// Frame Skip / Timing
// ============================================================================

bool is_skipped_frame() { return false; }
bool fps_halver_is_skipped_frame() { return false; }
bool skip_subframe() { return false; }
bool is_subframe() { return false; }
bool frame_is_even() { return false; }

bool line_is_even(float y, float size_y) {
    return mod(floor(y * size_y), 2.0) == 0.0;
}

float frame_subframe() { return 0.0; }

bool low_fps(sampler2D fps_tex) { return false; }
float get_fps(sampler2D fps_tex) { return 60.0; }

// ============================================================================
// Interlace / Scanline
// ============================================================================

bool is_interlaced() { return false; }
bool scanlines_will_alternate(bool is_interlaced) { return false; }
bool scanline_have_to_flicker(bool is_interlaced) { return false; }
bool pixelgrid_h_needed() { return false; }
bool need_doublescan() { return false; }
bool fake_scanlines_needed(bool very_lowres_y, bool is_interlaced) { return false; }

bool fn_very_lowres_y(vec2 outSize, vec4 oriSize, float doTate, bool isRotated,
                       bool integerScale, float aspect, float scanlinePeriod) {
    return false;
}

// ============================================================================
// Zoom Modifier / Dynamic Zoom
// ============================================================================

float get_dyn_zoom(sampler2D avglum_tex) { return 1.0; }
float get_autocrop_zoom(sampler2D avglum_tex) { return 1.0; }

vec2 get_zooms_modifier(float doTate, bool needNOIntegerScale, bool isRotated,
                        float aspect, vec2 outputSize) {
    return vec2(1.0);
}

// ============================================================================
// Pixelgrid
// ============================================================================

vec4 PG_get_hmask_preset() { return vec4(1.0, 0.0, 0.0, 0.0); }

float get_dotmat_auto_sharpness(float doTate, bool needNOIntScale, bool isRotated, float aspect) {
    return 1.0;
}

// ============================================================================
// Edge / Distance
// ============================================================================

float distance_from_edge_linear(vec2 co, vec2 area_inv) {
    vec2 d = min(co, 1.0 - co) * area_inv;
    return min(d.x, d.y);
}

float corners_shade(vec2 co, float amount) {
    vec2 d = (co - 0.5) * 2.0;
    return 1.0 - dot(d, d) * amount;
}

// ============================================================================
// Noise / Random
// ============================================================================

float get_rf_noise(vec2 seed) {
    return (fract(sin(dot(seed, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * RF_NOISE_STRENGTH;
}

float random_fast(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

// ============================================================================
// Halo
// ============================================================================

float get_halo_ungain_factor() { return 1.0; }

// ============================================================================
// Warped Glow
// ============================================================================

float warpedglow_x_allowed(float sharpness) { return sharpness; }

// ============================================================================
// Adaptive Strobe
// ============================================================================

vec3 adaptive_strobe(vec3 pixel, float oscillator, sampler2D feedback, vec2 co) {
    return pixel;
}

// ============================================================================
// Image
// ============================================================================

vec4 pixel_overlay_image_stretch(sampler2D smp, vec2 co) {
    return texture(smp, co);
}

vec3 pixel_backdrop_image(sampler2D smp, vec2 co, vec2 viewportSize) {
    return texture(smp, co).rgb;
}

vec3 pixel_backdrop_image(sampler2D smp, vec2 co, vec4 viewportSize) {
    return texture(smp, co).rgb;
}

// ============================================================================
// Screen Uniformity
// ============================================================================

vec2 coords_QULEZ(vec2 co, vec2 size) { return co; }

// ============================================================================
// Debug
// ============================================================================

vec3 PrintValueVec3(vec2 uv, vec2 fpos, float value, float maxdigits, float decimalplaces) {
    return vec3(0.0);
}

// ============================================================================
// Bloom Helper Functions (included from blooms.include.slang)
// ============================================================================

#ifndef BLOOMS_INCLUDE_SLANG
#include "includes/blooms.include.slang"
#endif

#endif // FUNCTIONS_INCLUDE_SLANG
