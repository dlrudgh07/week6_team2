#include "EnginePCH.h"
#include "Level.h"

#include "GameFramework/Actor.h"

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
