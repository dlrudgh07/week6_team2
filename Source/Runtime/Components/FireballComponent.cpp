#include "EnginePCH.h"
#include "FireballComponent.h"

UFireballComponent::UFireballComponent()
{
    Intensity = 0.9f;
    Radius = 15.0f;
    RadiusFallOff = 6.0f;
    Color = FLinearColor::Red;
}

UFireballComponent::~UFireballComponent()
{

}
