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

void ULevel::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	TArray<AActor*> Temp;
	Temp.Reserve(Actors.Num());

	for (auto* Actor : Actors)
	{
		AActor* NewActor = FObjectFactory::DuplicateObject(Actor, this);

		if (!NewActor)
		{
			continue;
		}

		NewActor->SetWorld(OwningWorld);
		NewActor->SetLevel(this);
		NewActor->DuplicateSubObjects();
		Temp.Add(NewActor);
	}

	Actors = std::move(Temp);
}