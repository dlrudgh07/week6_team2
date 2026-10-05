#pragma once

#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/FireballComponent.h"

class AFireballActor : public AActor
{
    DECLARE_CLASS(AFireballActor, AActor)

public:
    AFireballActor();
    virtual ~AFireballActor() = default;

    virtual void BeginPlay() override;
    
    virtual void DuplicateSubObjects() override;
private:
    USceneComponent* SceneComponent;
	UStaticMeshComponent* StaticMeshComponent;
    UFireballComponent* FireballComponent;
};