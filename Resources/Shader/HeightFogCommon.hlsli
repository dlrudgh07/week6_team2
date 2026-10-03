#pragma once

static const float FLT_EPSILON = 1e-4f;

struct FogParams
{
    float4 FogInscatteringColor;
	float FogDensity;
    float FogHeightFalloff;
    float FogMaxOpacity;
    float StartDistance;
    float EndDistance;
    float FogCutoffDistance;

};

float3 ReconstructWorldPosition(float2 UV, float Depth, matrix InvProj, matrix InvView)
{
    // NDC
    float4 ndc;

    ndc.x = UV.x * 2.0 - 1.0;
    ndc.y = 1.0 - UV.y * 2.0;
    ndc.z = Depth;
    ndc.w = 1.0;

    float4 ViewPos = mul(ndc, InvProj);
    ViewPos /= ViewPos.w;

    float4 WorldPos = mul(ViewPos, InvView);
    WorldPos /= WorldPos.w;

	return WorldPos.xyz;    // return float
}

// Fog Factor 계산 (FogColor와 곱해지는 값)
float CalculateFogFactor(float WorldPos, float3 CameraPos, FogParams Fog)
{
    float3 Ray = WorldPos - CameraPos;
    float Distance = length(Ray);
    if (Distance < FLT_EPSILON || Distance > Fog.FogCutoffDistance) return 0.0f;

    float3 Dir = Ray / Distance;

    // 1. 거리 최적화
    float EffectiveDistance = max(0.0, Distance - Fog.StartDistance);

    // 2. 높이 안개 농도 적분
    // d(z) = Density * exp(-Falloff * (z - Height))
    float Falloff = max(Fog.FogHeightFalloff, FLT_EPSILON);
    float StartHeight = CameraPosition.z + Dir.z * min(Distance, Fog.StartDistance);
    float RelativeHeight = StartHeight - FogHeight;

    // Ratio = (1 - exp(-x)) / x
    float x = Fog.Falloff * Dir.z * EffectiveDistance;
    float Ratio = abs(x) > 0.01 ? (1 - exp(-x)) / x : 1.0 - x * 0.5;

    float FogInt =  Fog.FogDensity * exp(-Fog.Falloff * RelativeHeight) * EffectiveDistance * Ratio;

    // 3. 투과율 계산 및 Max Opacity 적용
    float FogFactor = 1.0 - exp(-FogInt);
    return min(FogFactor, Fog.FogMaxOpacity);
}