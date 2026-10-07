#include "HeightFogCommon.hlsli"

// Camera + Fog Constant Buffer
cbuffer FogConstants : register(b0)
{
    // 64-bytes
    row_major matrix InvViewProj;
    
    // 16-bytes
    float3 CameraPos;
    float Padding0;

    // 48-bytes
    FogParams Fog;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

Texture2D DepthTexture : register(t0);
Texture2D Panorama : register(t1);
SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);

// Vertex Shader
// Vertex Buffer, Index Buffer 불필요 (Ex. Draw(3, 0))
PS_INPUT mainVS(uint ID : SV_VertexID)
{
    PS_INPUT Output;
    // (0, 0), (2, 0), (0, 2)
    Output.UV = float2((ID << 1) & 2, (ID & 2));

    // (-1, 1), (3, 1), (-1, -3) -> 화면 전체를 가리는 큰 삼각형 하나
    Output.Position = float4(Output.UV * float2(2, -2) + float2(-1, 1), 0, 1);

    return Output;
}

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float Depth = DepthTexture.Sample(PointSampler, input.UV).r;

    float3 WorldPos =
        ReconstructWorldPosition(input.UV, Depth, InvViewProj);

    float FogFactor = CalculateFogFactor(WorldPos, CameraPos, Fog);

    if (FogFactor <= 0.0f)
        return float4(0, 0, 0, 0);

    // 카메라에서 해당 픽셀을 바라보는 월드 방향
    float3 Direction = normalize(WorldPos - CameraPos);

    // 기존 Skybox와 동일한 Z-up 파노라마 좌표
    const float PI = 3.14159265f;

    float2 PanoUV;
    PanoUV.x = atan2(Direction.y, Direction.x) / (2.0f * PI) + 0.5f;
    PanoUV.y = acos(clamp(Direction.z, -1.0f, 1.0f)) / PI;

    float3 FogColor =
        Panorama.SampleLevel(LinearSampler, PanoUV, 0).rgb;
    FogColor *= Fog.FogInscatteringColor.rgb;

    return float4(FogColor, FogFactor);
}