#include "EnginePCH.h"
#include "StaticMeshComponent.h"
#include "Asset/AssetManager.h"
#include "Rendering/RenderCommand.h"


// StaticMesh 컴포넌트를 초기화한다.
UStaticMeshComponent::UStaticMeshComponent()
{
	StaticMesh = UAssetManager::GetAssetByKey<UStaticMesh>("Cube");
}

// StaticMesh 컴포넌트의 소멸을 처리한다.
UStaticMeshComponent::~UStaticMeshComponent()
{
}

// 부모 컴포넌트의 시작 처리를 호출한다.
void UStaticMeshComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 부모 컴포넌트의 프레임 갱신을 호출한다.
void UStaticMeshComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
    if (StaticMesh == InStaticMesh)
        return;

    StaticMesh = InStaticMesh;
    ClearOverrideMaterials();
    MarkBoundsDirtyRecursive();
	bRenderPacketCacheDirty = true;
}

int32 UStaticMeshComponent::GetNumMaterials() const
{
    return StaticMesh ? static_cast<int32>(StaticMesh->GetMeshData().MaterialSlots.Num()) : 0;
}

FString UStaticMeshComponent::GetMaterialSlotName(int32 SlotIndex) const
{
    if (!StaticMesh || SlotIndex < 0 || SlotIndex >= GetNumMaterials())
        return FString();
    return StaticMesh->GetMeshData().MaterialSlots[SlotIndex].Name;
}

UMaterial* UStaticMeshComponent::GetDefaultMaterial(int32 SlotIndex) const
{
    return StaticMesh ? StaticMesh->GetMaterial(static_cast<uint32>(SlotIndex)) : nullptr;
}

void UStaticMeshComponent::RebuildRenderPacketCache()
{
	CachedRenderPackets.Reset();

	if (!StaticMesh)
		return;

	const FStaticMeshData& MeshData = StaticMesh->GetMeshData();

	for (const FStaticMeshSection& Section : MeshData.Sections)
	{
		UMaterial* SectionMaterial = GetMaterial(static_cast<int32>(Section.MaterialSlotIndex));
		if (!SectionMaterial)
			continue;

		FRenderPacket rp;
		rp.mesh = StaticMesh;
		rp.material = SectionMaterial;
		rp.StartIndex = Section.StartIndex;
		rp.IndexCount = Section.IndexCount;
		CachedRenderPackets.Add(rp);
	}
	
	CachedMeshRenderDataRevision = StaticMesh->GetRenderDataRevision();
	bRenderPacketCacheDirty = false;
}

void UStaticMeshComponent::SetMaterial(int32 SlotIndex, UMaterial* InMaterial)
{
	UMaterial* PreviousMaterial = GetMaterial(SlotIndex);

	UMeshComponent::SetMaterial(SlotIndex, InMaterial);

	if (PreviousMaterial != GetMaterial(SlotIndex))
		bRenderPacketCacheDirty = true;
}

// TArray 기반 고속 패킷 제출
void UStaticMeshComponent::SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets)
{
    if (!StaticMesh)
        return;

	if (bRenderPacketCacheDirty || CachedMeshRenderDataRevision != StaticMesh->GetRenderDataRevision())
	{
		RebuildRenderPacketCache();
	}

    const FMatrix WorldMatrix = GetWorldMatrix();

	for (FRenderPacket RenderPacket : CachedRenderPackets)
	{
		RenderPacket.model = WorldMatrix;
		OutPackets.Add(RenderPacket);
	}
}
