#include "EnginePCH.h"
#include "MovementComponent.h"

bool UMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	if (UpdatedComponent)
	{
		// const FVector NewDelta = ConstrainDirectionToPlane(Delta);
		// return UpdatedComponent->MoveComponent(NewDelta, NewRotation, bSweep, OutHit, MoveComponentFlags, Teleport);
	}
	return false;
}

// Inlines
inline float UMovementComponent::GetMaxSpeed() const
{
	return 0.f;
}

inline void UMovementComponent::StopMovementImmediately()
{
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();
}

USceneComponent* UMovementComponent::GetUpdatedComponent() const
{
	return UpdatedComponent;
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* SceneComponent)
{
	UpdatedComponent = SceneComponent;
}

UPrimitiveComponent* UMovementComponent::GetUpdatedPrimitive() const
{
	return UpdatedPrimitive;
}

void UMovementComponent::SetUpdatedPrimitive(UPrimitiveComponent* PrimitiveComponent)
{
	UpdatedPrimitive = PrimitiveComponent;
}

FVector UMovementComponent::GetVelocity() const
{
	return Velocity;
}

void UMovementComponent::SetVelocity(FVector InVelocity)
{
	Velocity = InVelocity;
}
