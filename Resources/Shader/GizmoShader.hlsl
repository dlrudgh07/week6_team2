cbuffer constants : register(b0)
{
    row_major matrix World;
    row_major matrix ViewProjection;
	float4 Color;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
	float4 color : COLOR;
};

PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;
    
    output.position = mul(float4(input.position, 1.0f), World);
    output.position = mul(output.position, ViewProjection);
    //output.position = float4(input.position, 1.0f);
	output.color = Color;
	return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
	return input.color;
	//return float4(1.0f, 1.0f, 1.0f, 1.0f);
}