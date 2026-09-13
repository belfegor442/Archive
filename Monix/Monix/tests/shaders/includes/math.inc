#ifndef MATH_INC
#define MATH_INC
float clamp01(float x) { return clamp(x, 0.0, 1.0); }
vec2 clamp01(vec2 v) { return clamp(v, vec2(0.0), vec2(1.0)); }
#endif