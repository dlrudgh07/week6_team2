#pragma once

#include "ProjectileMovementComponent.h"
#include "Math/Color.h"

class UFireballComponent : public UProjectileMovementComponent
{
    DECLARE_CLASS(UFireballComponent, UProjectileMovementComponent)

    REFLECT_START(ClassName)
    PROPERTY(Intensity)
    PROPERTY(Radius)
    PROPERTY(RadiusFallOff)
    PROPERTY_TYPE(Color, Color)
    REFLECT_END()

public:
    UFireballComponent();
    virtual ~UFireballComponent();

private:
    float Intensity;            // 불빛 밝기
    float Radius;               // 불빛이 영향을 끼치는 범위
    float RadiusFallOff;        // 밝기가 감소하는 정도
    FLinearColor Color;         // 불빛 색상

public:
    // FireBall 파라미터값 Setter
    void SetIntensity(float InIntensity) { Intensity = InIntensity; }
    void SetRadius(float InRadius) { Radius = InRadius; }
    void SetRadiusFallOff(float InRadiusFallOff) { RadiusFallOff = InRadiusFallOff; }
    void SetColor(FLinearColor InColor) { Color = InColor; }

    // FireBall 파라미터값 Getter
    float GetIntensity() { return Intensity; }
    float GetRadius() { return Radius; }
    float GetRadiusFallOff() { return RadiusFallOff; }
    FLinearColor GetColor() { return Color; }
};