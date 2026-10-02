#pragma once

#include "SceneComponent.h"

class FLineBatcher;

class USpotLightComponent : public USceneComponent
{
	DECLARE_CLASS(USpotLightComponent, USceneComponent)

	REFLECT_START(ClassName)
		PROPERTY(InnerConeAngle)
		PROPERTY(OuterConeAngle)
		PROPERTY(AttenuationRadius)
		PROPERTY(Falloff)
		PROPERTY(Intensity)
		PROPERTY_TYPE(LightColor, Color)
	REFLECT_END()

public:
	USpotLightComponent() = default;
	virtual ~USpotLightComponent() override = default;

	void DrawDebug(FLineBatcher* LineBatcher) const;

	float GetInnerConeAngle() const { return InnerConeAngle; }
	void SetInnerConeAngle(float InAngle) { InnerConeAngle = InAngle; }

	float GetOuterConeAngle() const { return OuterConeAngle; }
	void SetOuterConeAngle(float InAngle) { OuterConeAngle = InAngle; }

	float GetFalloff() const { return Falloff; }
	void SetFalloff(float InFalloff) { Falloff = InFalloff; }

	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }

	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; }

private:
	// 원뿔 중심축 기준 반각(도). 
	float InnerConeAngle = 15.0f;
	float OuterConeAngle = 30.0f;

	// 빛이 닿는 거리
	float AttenuationRadius = 5.0f;

	// Inner에서 Outer로 가며 밝기가 떨어지는 정도.
	float Falloff = 1.0f;

	float Intensity = 1.0f;

	FVector4 LightColor = FVector4(1.0f, 1.0f, 0.6f, 1.0f);
};
