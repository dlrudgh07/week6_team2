#include "EnginePCH.h"
#include "Component/SceneComponent.h"

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

	DetachFromParent();
}

void USceneComponent::SetupAttachment(USceneComponent* InParent)
{
	if (InParent == this || AttachParent == InParent)
		return;

	// 순환 체크
	for (USceneComponent* Parent = InParent; Parent != nullptr; Parent = Parent->AttachParent)
		if (Parent == this)
			return;

	DetachFromParent();
	AttachParent = InParent;
	if (AttachParent)
	{
		AttachParent->AttachChildren.Add(this);
	}
	MarkTransformDirtyRecursive();
}

void USceneComponent::DetachFromParent()
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

FRotator USceneComponent::GetWorldRotation() const
{
	if (AttachParent)
	{
		FQuat ParentQuat = AttachParent->GetWorldRotation().Quaternion();
		FQuat LocalQuat = Transform.GetOrientation();

		return (ParentQuat * LocalQuat).ToFRotator();
	}

	return Transform.Rotation;
}

FVector USceneComponent::GetWorldLocation() const
{
	FMatrix WorldMatrix = GetWorldMatrix();

	return FVector(WorldMatrix[3][0], WorldMatrix[3][1], WorldMatrix[3][2]);
}

FVector USceneComponent::GetWorldScale3D() const
{
	if (AttachParent)
	{
		FVector ParentScale = AttachParent->GetWorldScale3D();

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
