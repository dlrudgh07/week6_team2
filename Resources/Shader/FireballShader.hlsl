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
    float2 Padding1;
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

    // 거리 감쇠
    float dist = length(worldPos - FireballPosition);
    float glow = 1.0 - saturate(dist / Radius);
    glow = pow(glow, RadiusFallOff);

    return float4(Color.rgb * Intensity * glow, 1.0);
}