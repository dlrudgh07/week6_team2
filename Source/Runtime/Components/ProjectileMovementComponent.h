#pragma once

#include "MovementComponent.h"
#include "SceneComponent.h"

struct FHitResult;

class UProjectileMovementComponent : public UMovementComponent
{
    DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

    REFLECT_START(ClassName)
	PROPERTY(ProjectileGravityScale)
    PROPERTY(Bounciness)
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
    float Bounciness = 1.0f;        // 1이면 같은 속도로 반사함.

    virtual FVector ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const;

private:
    float MaxSpeed = 0.f;
    void ResolveAABBCollision();
};