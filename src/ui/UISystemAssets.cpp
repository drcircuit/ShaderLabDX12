#include "ShaderLab/UI/UISystemAssets.h"

namespace ShaderLab {

const int kPostFxHistoryCount = 4;
const int kMaxPostFxChain = 32;
const int kAboutSrvIndex = 120;

const char* kAboutLogoShader = R"(
// Inputs assumed available: float iTime; float2 iResolution;

static const float3 VERTICES[20] =
{
    float3( 0.0,  0.0,  1.0),
    float3( 0.58, 0.33, 0.75),
    float3( 0.0, -0.67, 0.75),
    float3(-0.58, 0.33, 0.75),
    float3( 0.36, 0.87, 0.33),
    float3( 0.93,-0.13, 0.33),
    float3( 0.58,-0.75, 0.33),
    float3(-0.58,-0.75, 0.33),
    float3(-0.93,-0.13, 0.33),
    float3(-0.36, 0.87, 0.33),
    float3( 0.58, 0.75,-0.33),
    float3( 0.93, 0.13,-0.33),
    float3( 0.36,-0.87,-0.33),
    float3(-0.36,-0.87,-0.33),
    float3(-0.93, 0.13,-0.33),
    float3(-0.58, 0.75,-0.33),
    float3( 0.0,  0.67,-0.75),
    float3( 0.58,-0.33,-0.75),
    float3(-0.58,-0.33,-0.75),
    float3( 0.0,  0.0,-1.0)
};

static const int SEG[60] =
{
     0, 2, 6, 5, 1,
     0, 3, 8, 7, 2,
     0, 1, 4, 9, 3,
     2, 7,13,12, 6,
     8,14,18,13, 7,
     6,12,17,11, 5,
     3, 9,15,14, 8,
     1, 5,11,10, 4,
     4,10,16,15, 9,
    19,18,14,15,16,
    19,17,12,13,18,
    19,16,10,11,17
};

float2x2 Rot2(float a)
{
    float s = sin(a), c = cos(a);
    return float2x2(c, -s, s, c);
}

float SegmentDistance(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float  h  = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-8));
    return length(pa - ba * h);
}

float2 Project(float3 p, float cameraZ)
{
    float denom = p.z - cameraZ;
    denom = (abs(denom) < 1e-4) ? (denom < 0 ? -1e-4 : 1e-4) : denom;
    return p.xy / denom;
}

// Rotate around TWO axes: X (yz) and Y (zx) with independent speeds
float3 Spin2Axis(float3 p, float ax, float ay)
{
    // X axis rotation (affects yz)
    float2 yz = mul(Rot2(ax), p.yz);
    p.y = yz.x; p.z = yz.y;

    // Y axis rotation (affects zx)
    float2 zx = mul(Rot2(ay), p.zx);
    p.z = zx.x; p.x = zx.y;

    return p;
}

float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    // Normalize pixel coordinates (0 to 1)
    float2 uv = fragCoord / iResolution;
    // HLSL port: projective edge tracing wireframe
    float2 R = iResolution.xy;

    // Shadertoy-style normalized coords (aspect-correct)
    float2 U = (2.0 * fragCoord - R) / R.y;

    // Controls (tweakable constants)
    const float cameraZ      = 1.7;
    const float lineWidthPx  = 1.5;
    const float speedX       = 0.9;  // radians/sec-ish (scaled by iTime)
    const float speedY       = 1.5;

    float ax = iTime * speedX;
    float ay = iTime * speedY;

    float px = 1.0 / R.y;
    float halfW = max(lineWidthPx * px, 0.5 * px);

    float3 col = 0.0;

    float2 prev2 = 0.0;
    float2 first2 = 0.0;

    // Walk face-loops: every 5 indices closes the loop
    [unroll]
    for (int k = 0; k < 60; k++)
    {
        int idx = SEG[k];
        float3 p = VERTICES[idx];

        p = Spin2Axis(p, ax, ay);

        float2 p2 = Project(p, cameraZ);

        if (k % 5 == 0)
        {
            first2 = p2;
            prev2 = p2;
            continue;
        }

        float2 a = prev2;
        float2 b = (k % 5 == 4) ? first2 : p2;

        float d = SegmentDistance(U, a, b);

        // AA line
        float l = smoothstep(halfW, 0.0, d);

        // Simple time-varying color along faces/edges
        float faceId = floor(k / 5.0);
        float edgeId = (k % 5);
        float t = frac(faceId * 0.13 + edgeId * 0.21 + iTime * 0.05);

        float3 base = float3(0.42, 1.0, 1.00);
        float3 hot  = float3(0, 0.25, 0.8);
        float3 edgeColor = lerp(base, hot, t);

        col += edgeColor * l;

        prev2 = p2;
    }

    col = saturate(col);
    float alpha = saturate(dot(col, float3(0.333, 0.333, 0.333)) * 1.5);
    return float4(col, alpha);
}
)";

const char* kAboutGlitchPostFx = R"(
float rand(float2 co) {
    return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}

float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    float band = floor(uv.y * 40.0);
    float shift = (rand(float2(band, iTime)) - 0.5) * 0.02;
    float2 uv2 = uv + float2(shift, 0.0);
    return iChannel0.Sample(iSampler0, uv2);
}
)";

const PostFxPreset kPostFxPresets[] = {
    {
        "Simple Blur",
        R"(
float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    float2 texel = 1.0 / iResolution;
    float4 col = float4(0,0,0,0);
    col += iChannel0.Sample(iSampler0, uv + texel * float2(-1, -1));
    col += iChannel0.Sample(iSampler0, uv + texel * float2( 0, -1));
    col += iChannel0.Sample(iSampler0, uv + texel * float2( 1, -1));
    col += iChannel0.Sample(iSampler0, uv + texel * float2(-1,  0));
    col += iChannel0.Sample(iSampler0, uv + texel * float2( 0,  0));
    col += iChannel0.Sample(iSampler0, uv + texel * float2( 1,  0));
    col += iChannel0.Sample(iSampler0, uv + texel * float2(-1,  1));
    col += iChannel0.Sample(iSampler0, uv + texel * float2( 0,  1));
    col += iChannel0.Sample(iSampler0, uv + texel * float2( 1,  1));
    return col / 9.0;
}
)"
    },
    {
        "Grain",
        R"(
float rand(float2 co) {
    return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}

float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    float4 col = iChannel0.Sample(iSampler0, uv);
    float n = rand(uv * iResolution + iTime * 50.0);
    col.rgb += (n - 0.5) * 0.08;
    return col;
}
)"
    },
    {
        "Vignette",
        R"(
float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    float2 p = uv - 0.5;
    float v = 1.0 - smoothstep(0.2, 0.7, dot(p, p));
    float4 col = iChannel0.Sample(iSampler0, uv);
    col.rgb *= v;
    return col;
}
)"
    },
    {
        "Chromatic Aberration",
        R"(
float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    float2 dir = uv - 0.5;
    float2 offset = dir * 0.003;
    float r = iChannel0.Sample(iSampler0, uv + offset).r;
    float g = iChannel0.Sample(iSampler0, uv).g;
    float b = iChannel0.Sample(iSampler0, uv - offset).b;
    return float4(r, g, b, 1.0);
}
)"
    },
    {
        "Scanlines",
        R"(
float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    float4 col = iChannel0.Sample(iSampler0, uv);
    float l = sin(uv.y * iResolution.y * 3.14159);
    col.rgb *= 0.9 + 0.1 * l;
    return col;
}
)"
    },
    {
        "Glitch",
        R"(
float rand(float2 co) {
    return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}

float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    float band = floor(uv.y * 40.0);
    float shift = (rand(float2(band, iTime)) - 0.5) * 0.02;
    float2 uv2 = uv + float2(shift, 0.0);
    return iChannel0.Sample(iSampler0, uv2);
}
)"
    }
};

const size_t kPostFxPresetCount = sizeof(kPostFxPresets) / sizeof(kPostFxPresets[0]);

} // namespace ShaderLab
