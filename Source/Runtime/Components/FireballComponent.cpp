#include "EnginePCH.h"
#include "FireballComponent.h"

UFireballComponent::UFireballComponent()
{
    float Intensity = 5.0f;
    float Radius = 300.0f;
    float RadiusFallOff = 2.0f;
    FLinearColor Color = FLinearColor::Red;
}

UFireballComponent::~UFireballComponent()
{

}