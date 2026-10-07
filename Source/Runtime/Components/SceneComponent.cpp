#include "EnginePCH.h"
#include "Components/SceneComponent.h"

#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"

USceneComponent::~USceneComponent()
{
	TArray<USceneComponent*> Children = AttachChildren;
	AttachChildren.Reset();
	for (USceneComponent* Child : Children)
	{
		Child->AttachParent = nullptr;
		Child->SetupAttachment(AttachParent);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		if (OwnerActor->GetRootComponent() == this)
			OwnerActor->SetRootComponent(Children.Num() > 0 ? Children[0] : nullptr);
	}

	DetachFromComponent();
}

void USceneComponent::SetupAttachment(USceneComponent* InParent)
{
	if (InParent == this || AttachParent == InParent)
		return;

	// 순환 체크
	for (USceneComponent* Parent = InParent; Parent != nullptr; Parent = Parent->AttachParent)
		if (Parent == this)
			return;

	DetachFromComponent();
	AttachParent = InParent;
	if (AttachParent)
	{
		AttachParent->AttachChildren.Add(this);
	}
	MarkTransformDirtyRecursive();
}

void USceneComponent::DetachFromComponent()
{
	if (!AttachParent)
		return;

	TArray<USceneComponent*>& Siblings = AttachParent->AttachChildren;
	for (uint32 i = 0; i < Siblings.Num(); ++i)
	{
		if (Siblings[i] == this)
		{
			Siblings.RemoveAt(i, 1);
			break;
		}
	}
	AttachParent = nullptr;
	MarkTransformDirtyRecursive();
}

void USceneComponent::MarkBoundsDirtyRecursive()
{
	bBoundsDirty = true;
	++BoundsRevision;
	OnBoundsMarkedDirty();
	// 0은 미설정 표시로 남기고 overflow 시 1로 돌린다.
	if (BoundsRevision == 0)
		BoundsRevision = 1;

	for (USceneComponent* Child : AttachChildren)
	{
		if (Child)
			Child->MarkBoundsDirtyRecursive();
	}
}

void USceneComponent::MarkTransformDirtyRecursive()
{
	bWorldMatrixDirty = true;
	bBoundsDirty = true;
	++BoundsRevision;
	OnBoundsMarkedDirty();
	if (BoundsRevision == 0)
		BoundsRevision = 1;

	for (USceneComponent* Child : AttachChildren)
	{
		if (Child)
			Child->MarkTransformDirtyRecursive();
	}
}

void USceneComponent::Serialize(json& Handle, const bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);
	if (bIsLoading)
		MarkTransformDirtyRecursive();
}

FRotator USceneComponent::GetComponentRotation() const
{
	if (AttachParent)
	{
		FQuat ParentQuat = AttachParent->GetComponentRotation().Quaternion();
		FQuat LocalQuat = Transform.GetOrientation();

		return (ParentQuat * LocalQuat).ToFRotator();
	}
	else
	{
		FQuat LocalQuat = Transform.GetOrientation();

		return LocalQuat.ToFRotator();
	}

	return Transform.Rotation;
}

FVector USceneComponent::GetComponentLocation() const
{
	FMatrix WorldMatrix = GetWorldMatrix();

	return FVector(WorldMatrix[3][0], WorldMatrix[3][1], WorldMatrix[3][2]);
}

FVector USceneComponent::GetComponentScale() const
{
	if (AttachParent)
	{
		FVector ParentScale = AttachParent->GetComponentScale();

		return FVector(Transform.Scale.X * ParentScale.X, Transform.Scale.Y * ParentScale.Y, Transform.Scale.Z * ParentScale.Z);
	}

	return Transform.Scale;
}

FMatrix USceneComponent::GetWorldMatrix() const
{
	if (bWorldMatrixDirty)
	{
		CachedWorldMatrix = Transform.GetLocalMatrix();
		if (AttachParent)
			CachedWorldMatrix = CachedWorldMatrix * AttachParent->GetWorldMatrix();
		bWorldMatrixDirty = false;
	}
	return CachedWorldMatrix;
}

void USceneComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	AttachParent = nullptr;
	AttachChildren.Reset();
}

bool USceneComponent::MoveComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	if (OutHit)
		*OutHit = FHitResult();

	// Delta와 NewRotation은 월드 공간 값이므로 부모 기준 Relative 값으로 바꿔서 저장한다.
	const FVector NewWorldLocation = GetComponentLocation() + Delta;
	FVector NewRelativeLocation = NewWorldLocation;
	FQuat NewRelativeRotation = NewRotation;

	if (AttachParent)
	{
		// World = Parent * Local 이므로 Local = Parent^-1 * World
		NewRelativeLocation = AttachParent->GetWorldMatrix().Inverse().TransformPosition(NewWorldLocation);
		NewRelativeRotation = AttachParent->GetComponentRotation().Quaternion().Inverse() * NewRotation;
	}

	Transform.Location = NewRelativeLocation;
	Transform.Rotation = NewRelativeRotation.ToFRotator();
	MarkTransformDirtyRecursive();
	return true;
}
