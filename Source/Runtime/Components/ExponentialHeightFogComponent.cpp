#include "EnginePCH.h"
#include "ExponentialHeightFogComponent.h"
#include "ExponentialHeightFog.h"

UExponentialHeightFogComponent::UExponentialHeightFogComponent()
{
    FogDensity = 0.02f;
    FogHeightFalloff = 0.2f;
    
    FogMaxOpacity = 1.0f;
    StartDistance = 0.0f;
    EndDistance = 10000.0f;

    FogCutoffDistance = 10000.0f;

}

UExponentialHeightFogComponent::~UExponentialHeightFogComponent()
{

}

// AExponentialHeightFog (Actor)
// FogComponent를 월드에 배치하기 위한 Wrapper Actor 이므로 Component 파일과 함께 정의
AExponentialHeightFog::AExponentialHeightFog()
{
    ExponentialHeightFogComponent = 
        CreateDefaultSubobject<UExponentialHeightFogComponent>("UExponentialHeightFogComponent");
	SetRootComponent(ExponentialHeightFogComponent);
}
