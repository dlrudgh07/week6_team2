#pragma once

#include "ActorComponent.h"

class USceneComponent;

class UMovementComponent : public UActorComponent
{
    DECLARE_CLASS(UMovementComponent, UActorComponent)

    REFLECT_START(ClassName)
    REFLECT_END()

public:
    UMovementComponent() = default;
    virtual ~UMovementComponent() override;

    // Gravity 반환
    virtual float GetGravityZ() const;
    
    // 최대 속도 반환
    virtual float GetMaxSpeed() const;

    // 과속 여부 반환
    virtual bool IsExceedingMaxSpeed(float MaxSpeed) const;
    
    // 즉시 멈추는 함수
    virtual void StopMovementImmediately();

    // Update Skip 가능 여부 반환 (if moved component is not rendered or can't move)
    virtual bool ShouldSkipUpdate(float DeltaTime) const;

    // Assign the component we move and update
    virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);

    // This needs to be called by derived classes at the end of an update whenever Velocity has changed.
    virtual void UpdateComponentVelocity();

    bool MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = nullptr, ETeleportType Teleport = ETeleportType::None);
};