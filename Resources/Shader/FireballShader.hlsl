#include "HeightFogCommon.hlsli"

cbuffer PerFrame : register(b0)
{
    row_major matrix ViewProj;       // 64 bytes
    row_major matrix InvViewProj;    // 64 bytes
    float2 ScreenSize;               // 8  bytes
    float2 Padding0;                  // 8  bytes
};

cbuffer PerObj : register(b1)
{
    row_major matrix World;
};

cbuffer FireballConstants : register(b2)
{
    float3 FireballPosition;
    float Radius;

    float4 Color;

    float Intensity;
    float RadiusFallOff;
    int SplitLevel;
    float Padding;
    
    float3 CameraLocation;
    float AirLightIntensity;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR;
    float2 uv       : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

Texture2D DepthTexture : register(t0);
SamplerState PointSampler : register(s0);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.Position, 1.0), World);
    output.Position = mul(worldPos, ViewProj);

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 이 픽셀의 깊이
    float2 uv = input.Position.xy / ScreenSize;
    float depth = DepthTexture.Sample(PointSampler, uv).r;

    // 깊이 -> World Position 복원
    float3 worldPos = ReconstructWorldPosition(uv, depth, InvViewProj);
    float dist = length(worldPos - FireballPosition);
    int N = SplitLevel;
    
    if (N > 1)
    {
    
    // 빚줄기 테스트 //
    /*
                               원의중심
                                / |
                               /  |
                              /   |
                             /    |
                            /     | h
          L            R   /      |
                          /       |
                         /        |
                        /         |
                       /          |
    CAMERA            /           |
    -------------------------------------------------->D
    -----------tc-----------------
    |--------t0------|----Half----|-----------| <t1 = t0 + Half + Half'
    원의중심~카메라 = L
    */
        float3 finalColor = 0;
        float3 D = worldPos - CameraLocation; // Ray
        float RayDist = length(D);
        D = D / length(D);
        float3 L = FireballPosition - CameraLocation;
        float tc = dot(L, D);
        float h_square = dot(L, L) - tc * tc;
        if (h_square > Radius * Radius)
            return 0;
    
        float Half = sqrt(Radius * Radius - h_square);
        float t0 = max(tc - Half, 0);
        float t1 = min(tc + Half, RayDist);
        if (t0 >= t1)
            return 0;
    //chord = 현
        float3 chordPos_left = CameraLocation + D * t0;
        float3 chordPos_right = CameraLocation + D * (tc + Half);
        float3 chordVector = chordPos_right - chordPos_left;
        float chord_dist = length(chordVector);
        float chord_dist_norm = chord_dist / N;
        float RayDist_unit = RayDist / N;
        float3 ParticlePos;
        float ParticleDist;
        float ParticleGlow;
        for (int i = 0; i < N; i++)
        {
            ParticlePos = chordPos_left + (i + 0.5) * D;
            ParticleDist = length(ParticlePos - FireballPosition);
            ParticleGlow = 1.0 - saturate(ParticleDist / Radius);
            ParticleGlow = pow(ParticleGlow, RadiusFallOff);
            finalColor += (Color.rgb * Intensity * ParticleGlow * chord_dist_norm);

        }
        return float4(finalColor.xyz, 1);
    // 테스트중
    
    }
    // 거리 감쇠
    else
    {
    float glow = 1.0 - saturate(dist / Radius);
    glow = pow(glow, RadiusFallOff);

    return float4(Color.rgb * Intensity * glow, 1.0);
    }
}