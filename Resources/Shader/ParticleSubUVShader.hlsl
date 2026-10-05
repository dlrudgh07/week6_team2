// 렌더러의 오브젝트별 슬롯(b0) = World 행렬 + 패킷별 재질 파라미터(FSubUVConstants).
// 슬롯은 VS/PS 모두에 바인딩된다. (FRenderer::WritePerObjectSlot 참고)
cbuffer constants : register(b0)
{
    row_major matrix World;

    // FSubUVConstants와 같은 순서
    float CurrentFrame;
    float AtlasRowSize;
    float AtlasColSize;
    float ParticleAlpha;
};

cbuffer ViewConstants : register(b2)
{
    row_major matrix ViewProjection;
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
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.position = mul(worldPos, ViewProjection);
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
