#ifndef BLOOMS_INCLUDE_SLANG
#define BLOOMS_INCLUDE_SLANG

// ============================================================================
// Fuzzy Main Pass - gamma/brightness pre-processing for bloom pipeline
// ============================================================================

// Returns (mul, pow) parameters for adaptive brightness/gamma
vec2 apply_fuzzy_main_pass_stage_1() {
    return vec2(1.0, 1.0);
}

vec2 apply_fuzzy_main_pass_stage_1_no_gamma_in() {
    return vec2(1.0, 1.0);
}

// Apply fuzzy brightness/gamma to a color
vec3 apply_fuzzy_main_pass_stage_2(vec3 color, vec2 fuzzy_params) {
    return color * fuzzy_params.x;
}

// ============================================================================
// Core Bloom Function - multi-tap polar directional blur
// ============================================================================

// bloom() performs a multi-sample directional blur at a given angle (offset).
// Used across bloom_pass_2, bloom_pass_3, bloom_pass_4, reflection_blur, reflection_blur_pre.
//
// Parameters:
//   smp              - input texture
//   uv               - texture coordinate
//   srcsize          - vec4(width, height, 1/width, 1/height)
//   blur_radius_adpt - adapted blur radius from radius_blur_adapt()
//   quality          - number of directional samples
//   offset           - angular offset (radians)
//   pass_num         - pass index (0 for standard bloom)
vec3 bloom(sampler2D smp, vec2 uv, vec4 srcsize, vec2 blur_radius_adpt,
           float quality, float offset, float pass_num) {

    vec3 sum = vec3(0.0);
    float total_weight = 0.0;

    float samples = max(quality, 1.0);
    float TwoPI = 6.283185307179586;

    // Generate directional samples around the pixel
    // Each tap is offset along a different angle for a smooth radial blur
    for (float i = 0.0; i < samples; i++) {
        // Angle for this sample: evenly spaced + offset
        float angle = (i / samples) * TwoPI + offset;

        // Distance from center increases with sample index for softer spread
        float dist = (i + 1.0) / samples;

        // Compute UV offset along the angle
        vec2 sample_offset = vec2(cos(angle), sin(angle)) * blur_radius_adpt * dist;

        // Sample weight: center samples are heavier
        float weight = 1.0 / (1.0 + i * 0.5);
        weight *= weight; // square for smoother falloff

        sum += textureLod(smp, uv + sample_offset, 0.0).rgb * weight;
        total_weight += weight;
    }

    return sum / max(total_weight, eps);
}

#endif // BLOOMS_INCLUDE_SLANG
