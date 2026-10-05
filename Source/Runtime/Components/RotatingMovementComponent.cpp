#include "EnginePCH.h"
#include "RotatingMovementComponent.h"

#include "SceneComponent.h"

void URotatingMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	USceneComponent* Target = GetUpdatedComponent();

	if (!Target)
	{
		return;
	}

	const FQuat OldRotation = Target->GetComponentRotation().Quaternion();
	const FQuat DeltaRotation = (RotationRate * DeltaTime).Quaternion();
	const FQuat NewRotation = (bRotationInLocalSpace ? (OldRotation * DeltaRotation) : (DeltaRotation * OldRotation)).Normalized();
	
	FVector DeltaLocation = FVector::ZeroVector;
	if (PivotTranslation != FVector::ZeroVector)
	{
		const FVector OldPivot = OldRotation.RotateVector(PivotTranslation);
		const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
		DeltaLocation = OldPivot - NewPivot;
	}

	// MoveUpdatedComponent(DeltaLocation, NewRotation, );
}
