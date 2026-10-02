#pragma once

#include "MeshComponent.h"

class UStaticMeshComponent : public UMeshComponent
{
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

	REFLECT_START(UStaticMeshComponent)
		PROPERTY(StaticMesh)
	REFLECT_END()
public:
	UStaticMeshComponent();
	virtual ~UStaticMeshComponent();

	virtual void BeginPlay();
	virtual void TickComponent(float DeltaTime);

	virtual const FStaticMeshData* GetMeshData() const override { return StaticMesh ? &StaticMesh->GetMeshData() : nullptr; }

	// 메시가 바뀌면 슬롯 구성이 달라지므로 덮어쓰기를 비운다
	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh() const { return StaticMesh; }
	virtual UStaticMesh* GetRenderMesh() const override { return StaticMesh; }

	void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) override;
	virtual int32 GetNumMaterials() const override;
	virtual FString GetMaterialSlotName(int32 SlotIndex) const override;
	virtual UMaterial* GetDefaultMaterial(int32 SlotIndex) const override;

	// 수동 지정 강제 세부 단계
	int8 GetForcedLOD() const { return ForcedLOD; }
	void SetForcedLOD(int8 InLOD) { ForcedLOD = InLOD; }

private:
	UStaticMesh* StaticMesh = nullptr;
	int8 ForcedLOD = -1;

	// StaticMesh RenderPacket의 Mesh, Material 캐싱
	TArray<FRenderPacket> CachedRenderPackets;
	bool bRenderPacketCacheDirty = true;

	// 캐시가 어느 revision으로 만들어졌는지 저장
	uint64 CachedMeshRenderDataRevision = 0;

	void RebuildRenderPacketCache();

	// Mesh의 Section별 Material과 Texture를 보존해 각각의 렌더 패킷으로 제출한다.
	virtual void SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets) override;
};
