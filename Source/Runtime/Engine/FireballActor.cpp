#include "EnginePCH.h"
#include "FireballActor.h"

AFireballActor::AFireballActor()
{
    SetActorTickEnabled(false);

    SceneComponent = CreateDefaultSubobject<USceneComponent>("USceneComponent");
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("UStaticMeshComponent");
    FireballComponent = CreateDefaultSubobject<UFireballComponent>("UFireballComponent");
    
    SetRootComponent(SceneComponent);
    StaticMeshComponent->SetupAttachment(SceneComponent);
}

void AFireballActor::BeginPlay()
{
    Super::BeginPlay();
}

void AFireballActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    FireballComponent = FindComponentByClass<UFireballComponent>();
}