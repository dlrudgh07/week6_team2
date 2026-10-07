#include "EnginePCH.h"
#include "ProjectileMovementComponent.h"
#include "PrimitiveComponent.h"
#include "Engine/World.h"

UProjectileMovementComponent::~UProjectileMovementComponent() = default;

void UProjectileMovementComponent::ResolveAABBCollision()
{
	USceneComponent* Target = GetUpdatedComponent();
	UPrimitiveComponent* Self = Cast<UPrimitiveComponent>(Target);
	AActor* Owner = GetOwner();
	if (!Self || !Owner)
		return;

	UWorld* World = Owner->GetWorld();
	if (!World)
		return;

	for (const TWeakObjectPtr<UPrimitiveComponent>& WeakOther : World->GetWorldPrimitiveComponents())
	{
		UPrimitiveComponent* Other = WeakOther.Get();
		if (!Other || Other->GetOwner() == GetOwner())
			continue; // 자기 자신과의 충돌검사는 하지 않음.

		const FBox A = Self->GetWorldBounds();
		const FBox B = Other->GetWorldBounds();

		// x, y, z 축별 겹친 길이
		const float OverlapX = FMath::Min(A.Max.X, B.Max.X) - FMath::Max(A.Min.X, B.Min.X);
		const float OverlapY = FMath::Min(A.Max.Y, B.Max.Y) - FMath::Max(A.Min.Y, B.Min.Y);
		const float OverlapZ = FMath::Min(A.Max.Z, B.Max.Z) - FMath::Max(A.Min.Z, B.Min.Z);

		// 안 겹침
		if (OverlapX <= 0.f || OverlapY <= 0.f || OverlapZ <= 0.f)
			continue;

		const FVector CenterA = (A.Min + A.Max) * 0.5f;
		const FVector CenterB = (B.Min + B.Max) * 0.5f;

		FVector Normal;
		float Depth;

		// 가장 적게 겹친 축이 접촉면이라고 가정
		if (OverlapX <= OverlapY && OverlapX <= OverlapZ)
		{
			// (1.f, 0.f, 0.f) or (-1.f, 0.f, 0.f)
			Depth = OverlapX;
			Normal = FVector(CenterA.X >= CenterB.X ? 1.f : -1.f, 0.f, 0.f);
		}
		else if (OverlapY <= OverlapZ)
		{
			Depth = OverlapY;
			// (0.f, 1.f, 0.f) or (0.f, -1.f, 0.f)
			Normal = FVector(0.f, CenterA.Y >= CenterB.Y ? 1.f : -1.f, 0.f);
		}
		else
		{
			Depth = OverlapZ;
			// (0.f, 0.f, 1.f) or (0.f, 0.f, -1.f)
			Normal = FVector(0.f, 0.f, CenterA.Z >= CenterB.Z ? 1.f : -1.f);
		}

		// 겹친 만큼 밀어내기
		//MoveUpdatedComponent(Normal * Depth, Target->GetComponentRotation().Quaternion(), false);

		// V  = Vn (수직 성분) + Vt (수평성분)
		// V' = Vt - Vn (Vn을 2배 해서 빼주면 됨)
		const FVector V = GetVelocity();
		const float VN = V.Dot(Normal);
		// 면 쪽으로 향하고 있을 때만 반사
		if (VN < 0.f)
			SetVelocity((V - Normal * (2.f * VN)) * Bounciness);
	}
}

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (ShouldSkipUpdate(DeltaTime))
		return;

	USceneComponent* Target = GetUpdatedComponent();

	if (!Target)
		return;

	// 속도 갱신
	FVector NewVelocity = GetVelocity();
	NewVelocity.Z += GetGravityZ() * DeltaTime;
	SetVelocity(LimitVelocity(NewVelocity));

	// 충돌 검사
	ResolveAABBCollision();

	const FVector Delta = ComputeMoveDelta(GetVelocity(), DeltaTime);
	MoveUpdatedComponent(Delta, Target->GetComponentRotation().Quaternion(), false);
}

float UProjectileMovementComponent::GetGravityZ() const
{
	return Super::GetGravityZ() * ProjectileGravityScale;
}

FVector UProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const
{
	return InVelocity * DeltaTime + FVector(0, 0, GetGravityZ()) * 0.5f * DeltaTime * DeltaTime;
}

FVector UProjectileMovementComponent::LimitVelocity(FVector NewVelocity) const
{
	const float Max = GetMaxSpeed();
	if (Max > 0.f && NewVelocity.Dot(NewVelocity) > Max * Max)
		NewVelocity = NewVelocity.Normalized() * Max;

	return NewVelocity;
}