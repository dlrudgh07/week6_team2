#include "EnginePCH.h"
#include "Actor.h"
#include "World/World.h"
#include "World/Level.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Component/SceneComponent.h"

AActor::AActor()
{
}

AActor::~AActor()
{
    TArray<UActorComponent*> ToDelete = Components;
    Components.Reset();
    RootComponent = nullptr;

    for (UActorComponent* Component : ToDelete)
    {
        delete Component;
    }
}

void AActor::BeginPlay()
{
	//if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(RootComponent))
	//{
	//	World->AddPrimitive(Cast<UPrimitiveComponent>(RootComponent));
	//}

	for (UActorComponent* Component : Components) 
	{
		Component->BeginPlay();
	}
}

void AActor::Tick(float DeltaTime)
{
	for (UActorComponent* Component : Components)
	{
		Component->TickComponent(DeltaTime);
	}
}

void AActor::SetActorTickEnabled(const bool bEnabled)
{
	const bool bNewEnabled = bCanEverTick && bEnabled;
	if (bTickEnabled == bNewEnabled)
		return;

	bTickEnabled = bNewEnabled;
	if (World)
		World->RefreshActorTickRegistration(this);
}

void AActor::SetCanEverTick(const bool bEnabled)
{
	if (bCanEverTick == bEnabled)
		return;

	bCanEverTick = bEnabled;
	if (!bCanEverTick)
		bTickEnabled = false;
	if (World)
		World->RefreshActorTickRegistration(this);
}

void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
    for (uint32 i = 0; i < Components.Num(); ++i)
    {
        if (Components[i] == Component)
        {
            Components.RemoveAt(i, 1);
            break;
        }
    }

    if (RootComponent == Component)
    {
        RootComponent = nullptr;
    }
}

FVector AActor::GetActorLocation() const
{
    if (RootComponent)
    {
        return RootComponent->GetWorldLocation();
    }
    return FVector::ZeroVector;
}

//FRotator AActor::GetActorRotation() const
//{
//    if (RootComponent)
//    {
//        // USceneComponent의 GetWorldRotation() 호출
//        return RootComponent->GetWorldRotation();
//    }
//    return FRotator::ZeroRotator;
//}

FVector AActor::GetActorScale3D() const
{
    if (RootComponent)
    {
        return RootComponent->GetWorldScale3D();
    }
    return FVector::OneVector;
}

//FQuat AActor::GetActorQuat() const
//{
//    if (RootComponent)
//    {
//        return FQuat(RootComponent->GetWorldRotation());
//    }
//    return FQuat::Identity;
//}

FTransform AActor::GetActorTransform() const
{
    if (RootComponent)
    {
        return FTransform(
            RootComponent->GetWorldRotation(),
            RootComponent->GetWorldLocation(),
            RootComponent->GetWorldScale3D()
        );

        // return FTransform(RootComponent->GetWorldMatrix());
    }
    return FTransform::Identity;
}

bool AActor::Destroy()
{
    if (!World)
        return false;

    return World->DestroyActor(this);
}
