#include "EnginePCH.h"
#include "FireballComponent.h"

UFireballComponent::UFireballComponent()
{
    Intensity = 5.0f;
    Radius = 300.0f;
    RadiusFallOff = 2.0f;
    Color = FLinearColor::Red;
}

UFireballComponent::~UFireballComponent()
{

}
