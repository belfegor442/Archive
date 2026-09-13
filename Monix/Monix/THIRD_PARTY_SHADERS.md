# Third-Party Shader Acknowledgments

This document lists external shaders and algorithms used in the MONIX CRT presets.

## CRT Shader Sources

| Shader | Source | Repository | License | Files Used | Modifications |
|--------|--------|------------|---------|------------|---------------|
| CRT-Royale (Scanlines) | TroggleMonkey | [libretro/slang-shaders](https://github.com/libretro/slang-shaders) | GPL-2.0 | `GLSL/crt/scanlines.glsl` | Simplified single-pass Gaussian beam model. Extracted from multi-pass pipeline. |
| CRT-Royale (Phosphor) | TroggleMonkey | [libretro/slang-shaders](https://github.com/libretro/slang-shaders) | GPL-2.0 | `GLSL/crt/phosphor.glsl` | Simplified aperture grille/slot/shadow mask. Removed LUT texture dependency. |
| CRT-Royale (Glow) | TroggleMonkey | [libretro/slang-shaders](https://github.com/libretro/slang-shaders) | GPL-2.0 | `GLSL/crt/glow.glsl` | Simplified bloom. Combined H+V blur into single pass. |
| CRT-Geom (Curvature) | cgwg, Themaister, DOLLS | [libretro/slang-shaders](https://github.com/libretro/slang-shaders) | GPL-2.0 | `GLSL/crt/curvature.glsl` | Barrel distortion algorithm. Simplified from multi-pass geometry correction. |
| CRT Flicker | Monix (Original) | N/A | MIT | `GLSL/crt/flicker.glsl` | Custom temporal phosphor flicker simulation. |

## Algorithm Details

### Scanlines (from CRT-Royale)
- **Algorithm**: Gaussian beam profile scanline model
- **Reference**: `crt-royale-scanlines-vertical-interlacing.slang`
- **Key concept**: Each scanline is modeled as a Gaussian distribution centered on the electron beam position
- **Modifications**: Removed interlacing support, combined horizontal/vertical beam profiles into single pass

### Phosphor Mask (from CRT-Royale)
- **Algorithm**: Procedural RGB phosphor pattern generation
- **Reference**: `crt-royale-scanlines-horizontal-apply-mask.slang`
- **Key concept**: Three mask types (aperture grille, slot mask, shadow mask) generated procedurally
- **Modifications**: Removed LUT texture dependency, simplified mask generation. Original uses Lanczos-resized PNG textures.

### Glow/Bloom (from CRT-Royale)
- **Algorithm**: Separable Gaussian blur of bright areas
- **Reference**: `crt-royale-bloom-vertical.slang`, `crt-royale-bloom-horizontal-reconstitute.slang`
- **Key concept**: Extract bright areas (brightpass), apply Gaussian blur, add back to original
- **Modifications**: Combined H+V blur into single pass for simplicity. Original uses 2-pass separable blur.

### Curvature (from CRT-Geom)
- **Algorithm**: Barrel distortion transformation
- **Reference**: `crt-geom.slang` geometry correction
- **Key concept**: Transform screen coordinates through barrel distortion function
- **Modifications**: Simplified from full geometry correction to basic barrel distortion. Removed tangential distortion and overscan.

### Flicker (Original)
- **Algorithm**: Temporal brightness modulation
- **Key concept**: Simulates CRT phosphor decay and refresh rate flicker
- **Modifications**: Original implementation. Based on CRT phosphor persistence characteristics.

## License Compliance

All CRT-Royale derived shaders are licensed under GPL-2.0. The MONIX CRT presets are distributed under GPL-2.0 to comply with this license.

The flicker shader (`GLSL/crt/flicker.glsl`) is an original Monix implementation licensed under MIT.

## References

1. **CRT-Royale**: https://github.com/libretro/slang-shaders/tree/master/crt/shaders/crt-royale
2. **CRT-Geom**: https://github.com/libretro/slang-shaders/tree/master/crt/shaders/crt-geom
3. **Libretro Slang Shaders**: https://github.com/libretro/slang-shaders
4. **CRT-Royale Documentation**: See `crt-royale/README.TXT` in the libretro repository
