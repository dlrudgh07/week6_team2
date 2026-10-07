#include "EnginePCH.h"
#include "PlayerStart.h"

APlayerStart::APlayerStart()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>("USceneComponent"));
}

void APlayerStart::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
