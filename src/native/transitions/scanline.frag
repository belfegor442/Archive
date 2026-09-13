#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D oldFrame;
layout(set = 0, binding = 1) uniform sampler2D newFrame;

layout(push_constant) uniform PushConstants {
  float progress;
  float intensity;
  float scanlineWidth;
  float distortion;
  float noiseAmount;
  float flickerAmount;
  float time;
  float _pad;
} pc;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
  vec2 uv = vTexCoord;

  float scanY = pc.progress;

  float distToScan = abs(uv.y - scanY);

  float edge = smoothstep(scanY - pc.scanlineWidth, scanY + pc.scanlineWidth, uv.y);

  vec2 distortedUV = uv;
  float distortionStrength = smoothstep(pc.scanlineWidth * 3.0, 0.0, distToScan);
  distortedUV.x += distortionStrength * pc.distortion * sin(uv.y * 80.0 + pc.time * 10.0);

  vec4 oldColor = texture(oldFrame, distortedUV);
  vec4 newColor = texture(newFrame, distortedUV);

  vec4 color = mix(newColor, oldColor, edge);

  float scanlineBrightness = exp(-distToScan * 80.0) * pc.intensity;
  float flicker = 1.0 + sin(pc.time * 120.0) * pc.flickerAmount;
  color.rgb += vec3(scanlineBrightness * flicker * 0.5);

  float noise = (hash(uv * 1000.0 + pc.time) - 0.5) * pc.noiseAmount;
  color.rgb += vec3(noise);

  float edgeGlow = exp(-distToScan * 40.0) * pc.intensity * 0.3;
  color.rgb += vec3(0.7, 0.85, 1.0) * edgeGlow * flicker;

  color.rgb = clamp(color.rgb, 0.0, 1.0);

  FragColor = color;
}
