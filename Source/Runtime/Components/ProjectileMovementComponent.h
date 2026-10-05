#pragma once

#include "MovementComponent.h"

struct FHitResult;

class UProjectileMovementComponent : public UMovementComponent
{
        DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

        REFLECT_START(ClassName)
        REFLECT_END()

public:
    uint8 bSimulationUseScopedMovement:1;
    uint8 bInterpolationUseScopedMovement:1;

private:
    float InitialSpeed;
    float MaxSpeed;
    uint8 bRotationFollowsVelocity:1;
    uint8 bRotationRemainsVertical:1;
    uint8 bInitialVelocityInLocalSpace:1;
    uint8 bForceSubStepping:1;
    uint8 bSimulationEnabled:1;
    uint8 bIsHomingProjectile:1;
    uint8 bIsSliding:1;
    uint8 bInterpMovement:1;

protected:
    uint8 bThrottleInterpolation:1;
    uint8 ThrottleInterpolationFramesSinceInterp;

};