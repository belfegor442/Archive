#ifndef COLOR_INC
#define COLOR_INC
vec3 adjustBrightness(vec3 color, float brightness) {
    return color * brightness;
}
vec3 adjustContrast(vec3 color, float contrast) {
    return (color - 0.5) * contrast + 0.5;
}
#endif