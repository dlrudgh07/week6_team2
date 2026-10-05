#pragma once
#include "Math/Color.h"

struct FFireballSceneData
{
    FVector Position;            // 구 위치
    float Intensity;            // 불빛 밝기
    float Radius;               // 불빛이 영향을 끼치는 범위
    float RadiusFallOff;        // 밝기가 감소하는 정도
    FLinearColor Color;         // 불빛 색상
};