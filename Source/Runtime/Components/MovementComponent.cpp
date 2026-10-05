#include "EnginePCH.h"
#include "MovementComponent.h"
#include "SceneComponent.h"
#include "GameFramework/Actor.h"

UMovementComponent::~UMovementComponent() = default;

void UMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	AutoRegisterUpdatedComponent();
}

void UMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	// 에디터에서 Tick되는 액터는 BeginPlay 없이 Tick이 들어오므로 여기서도 확인
	AutoRegisterUpdatedComponent();
}

void UMovementComponent::AutoRegisterUpdatedComponent()
{
	if (UpdatedComponent)
		return;

	if (AActor* OwnerActor = GetOwner())
	{
		SetUpdatedComponent(OwnerActor->GetRootComponent());
	}
}

float UMovementComponent::GetGravityZ() const
{
	// UE는 World->GetGravityZ(). 월드 중력 설정이 생기면 연결
	return -980.f;
}

bool UMovementComponent::IsExceedingMaxSpeed(float MaxSpeed) const
{
	MaxSpeed = (MaxSpeed > 0.f) ? MaxSpeed : 0.f;

	// 수치 오차를 고려해 1% 허용
	const float OverVelocityPercent = 1.01f;
	return Velocity.Dot(Velocity) > MaxSpeed * MaxSpeed * OverVelocityPercent;
}

bool UMovementComponent::ShouldSkipUpdate(float DeltaTime) const
{
	return UpdatedComponent == nullptr;
}

void UMovementComponent::UpdateComponentVelocity()
{
	// UE: UpdatedComponent->ComponentVelocity = Velocity;
	// USceneComponent에 ComponentVelocity가 생기면 연결
}

bool UMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	if (UpdatedComponent)
	{
		// 2D 처리
		// const FVector NewDelta = ConstrainDirectionToPlane(Delta);

		return UpdatedComponent->MoveComponent(/*New*/Delta, NewRotation, bSweep, OutHit, /*MoveComponentFlags,*/ Teleport);
	}
	return false;
}

float UMovementComponent::GetMaxSpeed() const
{
	return 0.f;
}

void UMovementComponent::StopMovementImmediately()
{
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();
}

USceneComponent* UMovementComponent::GetUpdatedComponent() const
{
	return UpdatedComponent;
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* SceneComponent)
{
	UpdatedComponent = SceneComponent;
}

UPrimitiveComponent* UMovementComponent::GetUpdatedPrimitive() const
{
	return UpdatedPrimitive;
}

void UMovementComponent::SetUpdatedPrimitive(UPrimitiveComponent* PrimitiveComponent)
{
	UpdatedPrimitive = PrimitiveComponent;
}

FVector UMovementComponent::GetVelocity() const
{
	return Velocity;
}

void UMovementComponent::SetVelocity(FVector InVelocity)
{
	Velocity = InVelocity;
}
