#pragma once

// GLSL 1.20 source is embedded to avoid dependence on the launch working directory.
namespace EffectShaders
{
    const char* const Fire = R"GLSL(#version 120
uniform float uTime;
uniform float uMystical;
varying vec2 uv;

float noise(vec2 p)
{
    return sin(p.x * 11.0 + sin(p.y * 7.0)) * sin(p.y * 13.0 + p.x * 3.0);
}

void main()
{
    float height = 1.0 - uv.y;
    float sway = sin(height * 9.0 - uTime * 5.0) * height * 0.10;
    float width = mix(0.21, 0.01, height);
    float turbulence = noise(vec2(uv.x * 3.0, height * 3.0 - uTime * 2.0));
    float shape = 1.0 - abs(uv.x - 0.5 - sway) / max(width, 0.015);
    float flame = smoothstep(-0.1, 0.7, shape + turbulence * 0.25);
    flame *= (1.0 - smoothstep(0.65, 1.0, height)) * smoothstep(0.0, 0.08, height);
    float glow = exp(-length((uv - vec2(0.5, 0.77)) * vec2(6.0, 4.0)) * 2.5) * 0.20;
    vec3 hot = mix(vec3(1.0, 0.82, 0.26), vec3(0.92, 0.21, 0.03), height);
    vec3 cold = mix(vec3(0.63, 0.98, 0.73), vec3(0.08, 0.52, 0.60), height);
    gl_FragColor = vec4(mix(hot, cold, uMystical), flame + glow);
}
)GLSL";

    const char* const Water = R"GLSL(#version 120
uniform float uTime;
varying vec2 uv;

void main()
{
    float waveA = sin(uv.x * 9.0 + uv.y * 6.0 + uTime * 1.4);
    float waveB = sin(uv.y * 13.0 - uv.x * 3.0 - uTime * 1.1);
    float waveC = sin(length(uv * 3.0) * 7.0 - uTime * 2.0);
    float ripple = waveA * 0.5 + waveB * 0.3 + waveC * 0.2;
    vec3 color = mix(vec3(0.035, 0.11, 0.14), vec3(0.10, 0.29, 0.28), ripple * 0.5 + 0.5);
    float glint = pow(max(0.0, waveA * waveB), 18.0);
    color += vec3(0.29, 0.49, 0.44) * glint * 0.55;
    gl_FragColor = vec4(color, 1.0);
}
)GLSL";
} // namespace EffectShaders
