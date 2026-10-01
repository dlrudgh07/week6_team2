#include "EnginePCH.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include "RenderCommand.h"
#include "Asset/AssetManager.h"
#include "Collision/Ray.h"
#include <meshoptimizer.h>
#include <algorithm>

UStaticMesh::~UStaticMesh()
{
	VertexBuffer = nullptr;
	IndexBuffer = nullptr;
	LODs.Reset();
}

UMaterial* UStaticMesh::GetMaterial(uint32 SlotIndex) const
{
	if (SlotIndex < Materials.size() && Materials[SlotIndex]) {
		return Materials[SlotIndex];
	}
	// 기본 재질 캐시
	static UMaterial* CachedDefaultMaterial = nullptr;
	if (!CachedDefaultMaterial)
	{
		CachedDefaultMaterial = UAssetManager::GetAssetByKey<UMaterial>("DefaultMaterial");
	}
	return CachedDefaultMaterial;
}

FIndexBuffer* UStaticMesh::GetIndexBuffer(uint8 LODIndex) const
{
	if (LODIndex > 0 && !LODs.IsEmpty())
	{
		const uint32 ClampedIndex = (std::min)(static_cast<uint32>(LODIndex), static_cast<uint32>(LODs.Num() - 1));
		if (LODs[ClampedIndex].IndexBuffer)
		{
			return LODs[ClampedIndex].IndexBuffer.get();
		}
	}
	return IndexBuffer.get();
}

uint32 UStaticMesh::GetIndexCount(uint8 LODIndex) const
{
	if (LODIndex > 0 && !LODs.IsEmpty())
	{
		const uint32 ClampedIndex = (std::min)(static_cast<uint32>(LODIndex), static_cast<uint32>(LODs.Num() - 1));
		if (LODs[ClampedIndex].IndexCount > 0)
		{
			return LODs[ClampedIndex].IndexCount;
		}
	}
	return IndexBuffer ? IndexBuffer->GetIndexCount() : static_cast<uint32>(MeshData.Indices.Num());
}

bool UStaticMesh::GenerateLODs()
{
	const uint32 TotalIndices = static_cast<uint32>(MeshData.Indices.Num());
	const uint32 TotalVertices = static_cast<uint32>(MeshData.Vertices.Num());
	if (TotalIndices == 0 || TotalVertices == 0)
	{
		return false;
	}

	LODs.Reset();

	// 기본 원본 인덱스 버퍼 보존
	if (!IndexBuffer)
	{
		IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
			MeshData.Indices.GetData(),
			TotalIndices);
	}

	FStaticMeshLOD LODZero;
	LODZero.IndexCount = IndexBuffer ? IndexBuffer->GetIndexCount() : TotalIndices;
	LODZero.ScreenSizeThreshold = 0.15f;
	LODs.Add(std::move(LODZero));

	// 단순 도형의 경우 간소화 생략
	const uint32 TriangleCount = TotalIndices / 3;
	if (TriangleCount < 64)
	{
		return true;
	}

	const float* Positions = reinterpret_cast<const float*>(&MeshData.Vertices[0].Position);
	const size_t PositionStride = sizeof(FVertexPNCT);

	// 중간 단계 간소화 인덱스 생성 (원본의 25% 수준으로 대폭 축소)
	{
		const uint32 Desired1 = (TotalIndices / 4 / 3) * 3;
		const size_t TargetIndexCount = (Desired1 > 36u) ? Desired1 : 36u;
		TArray<uint32> LOD1Indices;
		LOD1Indices.SetNum(TotalIndices);

		float ResultError = 0.0f;
		const size_t ReducedCount = meshopt_simplify(
			LOD1Indices.GetData(),
			MeshData.Indices.GetData(),
			TotalIndices,
			Positions,
			TotalVertices,
			PositionStride,
			TargetIndexCount,
			0.05f,
			0,
			&ResultError);

		if (ReducedCount > 0 && ReducedCount < TotalIndices)
		{
			// 버텍스 캐시 및 오버드로우 최적화
			meshopt_optimizeVertexCache(
				LOD1Indices.GetData(),
				LOD1Indices.GetData(),
				ReducedCount,
				TotalVertices
			);

			meshopt_optimizeOverdraw(
				LOD1Indices.GetData(),
				LOD1Indices.GetData(),
				ReducedCount,
				Positions,
				TotalVertices,
				PositionStride,
				1.05f
			);

			FStaticMeshLOD LOD1;
			LOD1.IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
				LOD1Indices.GetData(),
				static_cast<uint32>(ReducedCount));
			LOD1.IndexCount = static_cast<uint32>(ReducedCount);
			LOD1.ScreenSizeThreshold = 0.05f;
			LODs.Add(std::move(LOD1));
		}
	}

	// 원거리 저해상도 단계 간소화 인덱스 생성 (원본의 5% 수준으로 극단적 축소)
	{
		const uint32 Desired2 = (TotalIndices / 20 / 3) * 3;
		const size_t TargetIndexCount = (Desired2 > 12u) ? Desired2 : 12u;
		TArray<uint32> LOD2Indices;
		LOD2Indices.SetNum(TotalIndices);

		float ResultError = 0.0f;
		size_t ReducedCount =  meshopt_simplify(
			LOD2Indices.GetData(),
			MeshData.Indices.GetData(),
			TotalIndices,
			Positions,
			TotalVertices,
			PositionStride,
			TargetIndexCount,
			0.15f,
			0,
			&ResultError);

		// 일반 간소화가 기하 제약으로 목표에 도달하지 못할 경우 초저해상도 전용 함수 적용
		const uint32 PrevIndexCount = LODs[LODs.Num() - 1].IndexCount;
		if (ReducedCount == 0 || ReducedCount > PrevIndexCount * 0.7f)
		{
			ReducedCount = meshopt_simplifySloppy(
				LOD2Indices.GetData(),
				MeshData.Indices.GetData(),
				TotalIndices,
				Positions,
				TotalVertices,
				PositionStride,
				TargetIndexCount,
				0.25f,
				&ResultError);
		}

		if (ReducedCount > 0 && ReducedCount < PrevIndexCount)
		{
			// 버텍스 캐시 및 오버드로우 최적화
			meshopt_optimizeVertexCache(
				LOD2Indices.GetData(),
				LOD2Indices.GetData(),
				ReducedCount,
				TotalVertices
			);

			meshopt_optimizeOverdraw(
				LOD2Indices.GetData(),
				LOD2Indices.GetData(),
				ReducedCount,
				Positions,
				TotalVertices,
				PositionStride,
				1.05f
			);

			FStaticMeshLOD LOD2;
			LOD2.IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
				LOD2Indices.GetData(),
				static_cast<uint32>(ReducedCount));
			LOD2.IndexCount = static_cast<uint32>(ReducedCount);
			LOD2.ScreenSizeThreshold = 0.01f;
			LODs.Add(std::move(LOD2));
		}
	}

	return true;
}

bool UStaticMesh::RebuildFromMeshData(FStaticMeshData&& InData)
{
	MeshData = std::move(InData);

	VertexBuffer = RenderCommand::CreateStaticVertexBuffer(MeshData.Vertices.GetData(), sizeof(FVertexPNCT) * static_cast<uint32>(MeshData.Vertices.Num()), sizeof(FVertexPNCT));
	IndexBuffer = RenderCommand::CreateStaticIndexBuffer(MeshData.Indices.GetData(), static_cast<uint32>(MeshData.Indices.Num()));
	if (!VertexBuffer || !IndexBuffer)
	{
		return false;
	}

	PrepareMeshPickingBVH(MeshData);

	if (!GenerateLODs())
	{
		return false;
	}

	++RenderDataRevision;
	if (RenderDataRevision == 0)
		RenderDataRevision = 1;

	return true;
}