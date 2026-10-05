#include "MovementComponent.h"

// Inlines
inline float UMovementComponent::GetMaxSpeed() const { return 0.f; }
inline void UMovementComponent::StopMovementImmediately() { Velocity = FVector::ZeroVector; UpdateComponentVelocity(); }

bool UMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
    if (UpdatedComponent)
    {
        // const FVector NewDelta = ConstrainDirectionToPlane(Delta);
        // return UpdatedComponent->MoveComponent(NewDelta, NewRotation, bSweep, OutHit, MoveComponentFlags, Teleport);
    }        
    return false;
}