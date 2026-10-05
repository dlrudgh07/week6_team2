#pragma once

static const float FLT_EPSILON = 1e-4f;

struct FogParams
{
    // 16-bytes
    float4 FogInscatteringColor;    
	
    // 16-bytes
    float FogDensity;               
    float FogHeightFalloff;         
    float FogHeight;
    float FogMaxOpacity;

    // 16-bytes
    float StartDistance;            
    float FogCutoffDistance;
    float Padding1;
    float Padding2;
};

float3 ReconstructWorldPosition(float2 UV, float Depth, matrix InvViewProj)
{
    // NDC
    float4 ndc;

    ndc.x = UV.x * 2.0 - 1.0;
    ndc.y = 1.0 - UV.y * 2.0;
    ndc.z = Depth;
    ndc.w = 1.0;

    // float4 ViewPos = mul(ndc, InvProj);
    // ViewPos /= ViewPos.w;

    float4 WorldPos = mul(ndc, InvViewProj);
    WorldPos /= WorldPos.w;

	return WorldPos.xyz;    // return float3
}

// Fog Factor 계산 (FogColor와 곱해지는 값)
float CalculateFogFactor(float3 WorldPos, float3 CameraPos, FogParams Fog)
{
    float3 Ray = WorldPos - CameraPos;
    float Distance = length(Ray);
    if (Distance < FLT_EPSILON || Distance > Fog.FogCutoffDistance) return 0.0f;

    
    //정규화
    float3 Dir = Ray / Distance;

    // 1. 거리 최적화
    float EffectiveDistance = Distance - Fog.StartDistance;
    //max(0.0, Distance - Fog.StartDistance);

    // 2. 높이 안개 농도 적분
    // d(z) = Density * exp(-Falloff * (z - Height))
    float Falloff = max(Fog.FogHeightFalloff, FLT_EPSILON);
    float StartHeight = CameraPos.z + Dir.z * min(Distance, Fog.StartDistance);
    float RelativeHeight = StartHeight - Fog.FogHeight;

    // Ratio = (1 - exp(-x)) / x
    float x = Falloff * Dir.z * EffectiveDistance;
    float Ratio = abs(x) > 0.01 ? (1 - exp(-x)) / x : 1.0 - x * 0.5;

    float FogInt =  Fog.FogDensity * exp(-Falloff * RelativeHeight) * EffectiveDistance * Ratio;

    // 3. 투과율 계산 및 Max Opacity 적용
    float FogFactor = 1.0 - exp(-FogInt);
    return min(FogFactor, Fog.FogMaxOpacity);
}