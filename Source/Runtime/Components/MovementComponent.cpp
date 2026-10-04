#include "MovementComponent.h"

bool UMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
    if (!UpdatedComponent.IsEmpty())
    {
        const FVector NewDelta = ConstrainDirectionToPlane(Delta);
        return UpdatedComponent->MoveComponent(NewDelta, NewRotation, bSweep, OutHit, MoveComponentFlags, Teleport);
    }        
    return false;
}