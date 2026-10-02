#pragma once

#include "GameFramework/Actor.h"
#include "ExponentialHeightFogComponent.h"

class AExponentialHeightFog : public AActor
{
    DECLARE_CLASS(AExponentialHeightFog, AActor)
public:
    AExponentialHeightFog();
    virtual ~AExponentialHeightFog() = default;
    UExponentialHeightFogComponent* GetExponentialHeightFogComponent() { return ExponentialHeightFogComponent; }

private:
    UExponentialHeightFogComponent* ExponentialHeightFogComponent;
};