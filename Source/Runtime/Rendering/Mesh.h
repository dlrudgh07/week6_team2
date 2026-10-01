#pragma once

#include "Asset/RenderAsset.h"
#include "Rendering/Buffer.h"
#include "Rendering/StaticMeshData.h"
#include "Asset/ObjImporter/ObjImportSettings.h"

struct FStaticMeshLOD
{
	TUniquePtr<FIndexBuffer> IndexBuffer;
	uint32 IndexCount = 0;
	float ScreenSizeThreshold = 0.0f;
};

class UStaticMesh : public URenderAsset
{
	DECLARE_CLASS(UStaticMesh, URenderAsset)
public:
	virtual ~UStaticMesh() override;

	FStaticMeshData MeshData;

	// Renderer가 실제로 Bind할 런타임 재질 객체
	TArray<UMaterial*> Materials;

	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;

	// 단계별 인덱스 버퍼 목록
	TArray<FStaticMeshLOD> LODs;

	const FStaticMeshData& GetMeshData() const { return MeshData; }
	UMaterial* GetMaterial(uint32 SlotIndex) const;

	// 단계별 인덱스 버퍼 반환
	FIndexBuffer* GetIndexBuffer(uint8 LODIndex = 0) const;
	uint32 GetIndexCount(uint8 LODIndex = 0) const;

	uint64 RenderDataRevision = 1;
	uint64 GetRenderDataRevision() const { return RenderDataRevision; }

	// 메시 간소화 라이브러리를 통한 자동 생성
	bool GenerateLODs();
	bool RebuildFromMeshData(FStaticMeshData&& InData);

	EObjAxisPreset ImportAxisPreset = EObjAxisPreset::Default;
	EObjAxisPreset AppliedAxisPreset = EObjAxisPreset::Default;
};
