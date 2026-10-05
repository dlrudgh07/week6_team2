#pragma once

#include "MovementComponent.h"

struct FHitResult;

class UProjectileMovementComponent : public UMovementComponent
{
        DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

        REFLECT_START(ClassName)
        REFLECT_END()

public:
    bool bSimulationUseScopedMovement = 1;
    bool bInterpolationUseScopedMovement = 1;
    float PreviousHitTime;
    FVector PreviousHitNormal;
    float ProjectileGravityScale;
    float Buoyancy;
    float Bounciness;
    float Friction;
    float BounceVelocityStopSimulatingThreshold;
    float MinFrictionFraction;

    virtual void SetVelocityInLocalSpace(FVector NewVelocity);
    virtual void TickComponent(float DeltaTime) override;

    virtual float GetMaxSpeed() const override { return MaxSpeed; }
    virtual float GetGravityZ() const override;

protected:
    bool bThrottleInterpolation = 1;
    bool ThrottleInterpolationFramesSinceInterp = 1;
    FVector LimitVelocity(FVector NewVelocity) const;

    virtual FVector ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta);
    virtual FVector ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const;

private:
    float InitialSpeed;
    float MaxSpeed;
    bool bRotationFollowsVelocity = false;
    bool bRotationRemainsVertical = false;
    bool bInitialVelocityInLocalSpace = false;
    bool bForceSubStepping = false;
    bool bSimulationEnabled = false;
    bool bIsHomingProjectile = false;
    bool bIsSliding = false;
    bool bInterpMovement = false;

};