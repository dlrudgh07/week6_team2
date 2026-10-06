cbuffer constants : register(b0)
{
    row_major matrix World;
};

cbuffer ViewConstants : register(b2)
{
    row_major matrix ViewProjection;
};

Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

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

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT o;
    o.position = mul(mul(float4(input.position, 1.0f), World), ViewProjection);
    o.uv = input.uv;
    return o;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 c = SpriteTexture.Sample(SpriteSampler, input.uv);
    clip(c.a - 0.1f);
    return c;
}