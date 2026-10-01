#include "EnginePCH.h"
#include "MeshComponent.h"

#include "Asset/AssetManager.h"
#include "Rendering/Material.h"
#include "Rendering/Texture2D.h"
#include "Serialization/TypeSerializer.h"


UMeshComponent::UMeshComponent()
{
}

UMeshComponent::~UMeshComponent()
{
}

void UMeshComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMeshComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

UMaterial* UMeshComponent::GetMaterial(int32 SlotIndex) const
{
	if (UMaterial* Override = GetOverrideMaterial(SlotIndex))
	{
		return Override;
	}
	return GetDefaultMaterial(SlotIndex);
}

UMaterial* UMeshComponent::GetOverrideMaterial(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= OverrideMaterials.Num())
	{
		return nullptr;
	}
	return OverrideMaterials[SlotIndex];
}

void UMeshComponent::SetMaterial(int32 SlotIndex, UMaterial* InMaterial)
{
	if (SlotIndex < 0)
	{
		return;
	}

	while (OverrideMaterials.Num() <= SlotIndex)
	{
		OverrideMaterials.Add(nullptr);
	}
	if (OverrideMaterials[SlotIndex] == InMaterial)
	{
		return;
	}
	OverrideMaterials[SlotIndex] = InMaterial;
	MarkBoundsDirtyRecursive();
}

void UMeshComponent::Serialize(json& Handle, bool bIsLoading)
{
	// 리플렉션 프로퍼티 (StaticMesh 등)를 먼저 처리해서, 슬롯 수가 메시 기준으로 정해진 뒤 덮어쓰기를 다룬다
	Super::Serialize(Handle, bIsLoading);

	if (bIsLoading)
	{
		OverrideMaterials.Reset();

		if (!Handle.contains("OverrideMaterials"))
		{
			return;
		}

		int32 SlotIndex = 0;
		for (const json& SlotJson : Handle["OverrideMaterials"])
		{
			if (!SlotJson.is_null())
			{
				SetMaterial(SlotIndex, UMaterial::LoadMaterial(SlotJson));
			}
			++SlotIndex;
		}
	}
	else
	{
		json Slots = json::array();
		for (UMaterial* Override : OverrideMaterials)
		{
			Slots.push_back(Override ? UMaterial::SaveMaterial(Override) : json(nullptr));
		}
		Handle["OverrideMaterials"] = Slots;
	}
}
