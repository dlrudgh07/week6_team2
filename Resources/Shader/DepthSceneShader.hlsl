#include "HeightFogCommon.hlsli"

cbuffer FogConstants : register(b0)
{
    row_major matrix InvProj;

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
    Output.UV = float2((ID << 1) & 2, (ID & 2));

    // (-1, 1), (3, 1), (-1, -3) -> 화면 전체를 가리는 큰 삼각형 하나
    Output.Position = float4(Output.UV * float2(2, -2) + float2(-1, 1), 0, 1);

    return Output;
}

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Scene Color Read
    // float4 Color = Texture.Sample(LinearSampler, input.uv);

    // Depth Read
    float Depth = DepthTexture.Sample(PointSampler, input.UV).r;
    
    // View Pos 계산
    float2 UV = input.UV;
    float4 ndc;
    ndc.x = UV.x * 2.0 - 1.0;
    ndc.y = 1.0 - UV.y * 2.0;
    ndc.z = Depth;
    ndc.w = 1.0;

    float4 ViewPos = mul(ndc, InvProj);
    ViewPos /= ViewPos.w; 
    float DepthColor = exp(-ViewPos.x * 0.05); //0.05=가까이 있는것에 더 진하게 해주는 보정. 일단 하드코딩

    return float4(DepthColor, DepthColor, DepthColor,1);
}
