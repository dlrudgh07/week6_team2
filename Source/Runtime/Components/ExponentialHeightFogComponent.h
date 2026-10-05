#pragma once

#include "SceneComponent.h"
#include "Math/Color.h"

// Location.z값으로 밀도가 정해지는 안개 효과를 위한 Component.
class UExponentialHeightFogComponent : public USceneComponent
{
    DECLARE_CLASS(UExponentialHeightFogComponent, USceneComponent)
    
	//Property Reflection
	REFLECT_START(ClassName)
	PROPERTY(FogDensity)
	PROPERTY(FogHeightFalloff)
	PROPERTY(FogMaxOpacity)
	PROPERTY(StartDistance)
	
	PROPERTY(FogCutoffDistance)
	PROPERTY_TYPE(FogInscatteringColor, Color)
	REFLECT_END()

public:
    UExponentialHeightFogComponent();
    virtual ~UExponentialHeightFogComponent();

private:
    // 안개 전체 농도 (0 ~ 0.05)
    float FogDensity;

    // 높이에 따른 안개 농도 증가 정도. 값이 작을수록 전환폭이 커짐 (0.01 ~ 2)
    float FogHeightFalloff;

    // 안개 최대 불투명도 (0_투명 ~ 1_불투명)
    float FogMaxOpacity;

    // 카메라로부터 이 거리에서부터 안개 효과 시작 (0 ~ 5000)
    float StartDistance;

    // 카메라로부터 이 거리까지 안개 효과 끝 (0 ~ 50000)
    float EndDistance;

    // 안개를 적용하지 않는 거리 (100000 ~ 20000000)
    float FogCutoffDistance;
    // 안개 산란 색 
    FLinearColor FogInscatteringColor;
    
public:
    // 안개 파라미터값 Setter
    void SetFogDensity(float InFogDensity) { FogDensity = InFogDensity; }
    void SetFogHeightFalloff(float InFogHeightFalloff) { FogHeightFalloff = InFogHeightFalloff; }
    void SetFogMaxOpacity(float InFogMaxOpacity) { FogMaxOpacity = InFogMaxOpacity; }
    void SetStartDistance(float InStartDistance) { StartDistance = InStartDistance; }
    void SetEndDistance(float InEndDistance) { EndDistance = InEndDistance; }
    void SetFogInscatteringColor(FLinearColor InFogInscatteringColor) { FogInscatteringColor = InFogInscatteringColor; }

    // 안개 파라미터값 Getter
    float GetFogDensity() { return FogDensity; }
    float GetFogHeightFalloff() { return FogHeightFalloff; }
    float GetFogMaxOpacity() { return FogMaxOpacity; }
    float GetStartDistance() { return StartDistance; }
    float GetEndDistance() { return EndDistance; }
	float GetFogCutoffDistance(){ return FogCutoffDistance; }
	
    FLinearColor GetFogInscatteringColor() { return FogInscatteringColor; }
	
};