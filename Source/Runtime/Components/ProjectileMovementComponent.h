#pragma once

#include "MovementComponent.h"
#include "SceneComponent.h"

struct FHitResult;

class UProjectileMovementComponent : public UMovementComponent
{
    DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

    REFLECT_START(ClassName)
    REFLECT_END()

public:
	UProjectileMovementComponent() = default;
	virtual ~UProjectileMovementComponent() override;

    // 중력을 쓰지 않는 것이 default
    float ProjectileGravityScale = 0.f;

    virtual void TickComponent(float DeltaTime) override;

    virtual float GetMaxSpeed() const override { return MaxSpeed; }
    virtual float GetGravityZ() const override;

protected:
    FVector LimitVelocity(FVector NewVelocity) const;

    virtual FVector ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const;

private:
    float MaxSpeed = 0.f;
};