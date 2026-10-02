#include "EnginePCH.h"
#include "Level.h"

#include "GameFramework/Actor.h"

UWorld* ULevel::GetWorld() const
{
	return OwningWorld;
}

const TArray<AActor*>& ULevel::GetActors() const
{
	return Actors;
}

uint32 ULevel::GetActorCount() const
{
	return Actors.Num();
}

void ULevel::AddActor(AActor* Actor)
{
	if (!Actor)
		return;

	Actors.Add(Actor);
}

void ULevel::ClearActors()
{
	for (AActor* Actor : Actors)
	{
		if (!Actor)
			continue;

		delete Actor;
	}

	Actors.Reset();
}
