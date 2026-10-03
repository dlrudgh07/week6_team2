#include "HeightFogCommon.hslsi"

// Camera Constant Buffer
cbuffer CameraParams : register(b0)
{
    float3 CameraPos;
    row_major matrix InvView;
    row_major matrix InvProj;
};

// Fog Constant Buffer
cbuffer FogParams : register(b1)
{
    FogParams Fog;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

Texture2D DepthTexture : register(t0);
// Texture2D Texture : register(t1);
SamplerState PointSampler : register(s0);
// SamplerState LinearSampler : register(s1);

// Vertex Shader
// Vertex Buffer, Index Buffer 불필요 (Ex. Draw(3, 0))
PS_INPUT mainVS(uint ID : SV_VertexID)
{
    PS_INPUT Output;
    // (0, 0), (2, 0), (0, 2)
    Output.UV = float2((ID << 1) & 2, (ID & 2), 0, 1);

    // (-1, 1), (3, 1), (-1, -3) -> 화면 전체를 가리는 큰 삼각형 하나
    Output.Position = float4(Output.UV * float2(2, -2) + float2(-1, 1), 1, 0);

    return Output;
}

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Scene Color Read
    // float4 Color = Texture.Sample(LinearSampler, input.uv);

    // Depth Read
    float Depth = DepthTexture.Sample(PointSampler, input.UV).r;

    // World Pos 계산
    float3 WorldPos = ReconstructWorldPosition(input.UV, Depth, InvProj, InvView);

    // Fog Factor 계산
    float FogFactor = CalculateFogFactor(WorldPos, CameraPos, Fog);

    return float4(Fog.FogInscatteringColor.rgb, FogFactor);

    // Lerp(선형보간) : Scene Color + Fog Color 
    // float3 RGB = Color.rgb * ( 1 - FogFactor ) + FogInscatteringColor.rgb * FogFactor;

    // return float4(RGB, Color.a);
}
