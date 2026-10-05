#include "EnginePCH.h"
#include "ProjectileMovementComponent.h"


void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (ShouldSkipUpdate(DeltaTime)) return;

	USceneComponent* Target = GetUpdatedComponent();

	if (!Target) return;

    // 속도 갱신
    FVector NewVelocity = GetVelocity();
    NewVelocity.Z += GetGravityZ() * DeltaTime;
    SetVelocity(LimitVelocity(NewVelocity));

    const FVector Delta = ComputeMoveDelta(GetVelocity(), DeltaTime);
    MoveUpdatedComponent(Delta, Target->GetComponentRotation().Quaternion(), false);
}

float UProjectileMovementComponent::GetGravityZ() const
{
    return Super::GetGravityZ() * ProjectileGravityScale;
}

FVector UProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const
{
    return InVelocity * DeltaTime;
}

FVector UProjectileMovementComponent::LimitVelocity(FVector NewVelocity) const
{
    const float Max = GetMaxSpeed();
    if (Max > 0.f && NewVelocity.Dot(NewVelocity) > Max * Max)
        NewVelocity = NewVelocity.Normalized() * Max;

    return NewVelocity;
}