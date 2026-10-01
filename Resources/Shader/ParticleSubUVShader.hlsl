// Todo: subuv
cbuffer constants : register(b0)
{
    row_major matrix MVP;
};

cbuffer subuv : register(b1)
{
    float CurrentFrame;
    float AtlasRowSize;
    float AtlasColSize;
    float ParticleAlpha;
};

Texture2D AtlasTexture : register(t0);
SamplerState AtlasSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// Todo: VS shader code duplicated
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(float4(input.position, 1.0f), MVP);
    output.uv = input.uv;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float atlasCol = fmod(CurrentFrame, AtlasColSize);
    float atlasRow = floor(CurrentFrame / AtlasColSize);

    float2 cellSize = float2(1.0f / AtlasColSize, 1.0f / AtlasRowSize);
    float2 cellUV = input.uv * cellSize;
    
    float2 cellStartOffsetUV = (float2(atlasCol, atlasRow) * cellSize);
    cellUV += cellStartOffsetUV;

    float4 color = AtlasTexture.Sample(AtlasSampler, cellUV);
    color.a *= ParticleAlpha;
    
    return color;
}
