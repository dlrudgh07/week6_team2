#pragma once

#include "ActorComponent.h"

class USceneComponent;

struct FHitResult;

// Move Update 처리하는 방법
enum class ETeleportType : uint8
{
	None,             // 일반 이동
	TeleoportPhysics, // 순간 이동 (속도 유지)
	ResetPhysics,     // 순간 이동 (물리 상태 초기화)
};

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

	// bool MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = nullptr, ETeleportType Teleport = ETeleportType::None);

	virtual void TickComponent(float DeltaTime) override;

	USceneComponent* GetUpdatedComponent() const;

	UPrimitiveComponent* GetUpdatedPrimitive() const;
	void SetUpdatedPrimitive(UPrimitiveComponent* PrimitiveComponent);

	FVector GetVelocity() const;
	void SetVelocity(FVector InVelocity);

  protected:
	// 실제 이동 처리, 충돌 검사, 위치 변경
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = nullptr, ETeleportType Teleport = ETeleportType::None);

	// Quaternion NewRotation
	// 공통 전처리, 파라미터 검증, 이동 가능 여부 확인 (호출 Interface)
	inline bool MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
	{
		return MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, OutHit, Teleport);
	}

  private:
	// SRT를 가진 Component
	USceneComponent* UpdatedComponent;

	// Collision, Physics, Bounds, Hit, Overlap 등을 처리하는 Component
	UPrimitiveComponent* UpdatedPrimitive;

	FVector Velocity;
};