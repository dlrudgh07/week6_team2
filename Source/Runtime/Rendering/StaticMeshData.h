#pragma once

#include "Vertex.h"
#include <immintrin.h>

struct FBox
{
	FVector Min;
	FVector Max;

	FBox GetWorldAABB(const FMatrix& M) const
	{
		// 중심 및 범위 계산
		const __m128 vHalf = _mm_set1_ps(0.5f);
		const __m128 vMin = _mm_setr_ps(Min.X, Min.Y, Min.Z, 0.0f);
		const __m128 vMax = _mm_setr_ps(Max.X, Max.Y, Max.Z, 0.0f);

		const __m128 vCenter = _mm_mul_ps(_mm_add_ps(vMin, vMax), vHalf);
		const __m128 vExtent = _mm_mul_ps(_mm_sub_ps(vMax, vMin), vHalf);

		// 행렬 로드
		const __m128 Row0 = _mm_loadu_ps(M.M[0]);
		const __m128 Row1 = _mm_loadu_ps(M.M[1]);
		const __m128 Row2 = _mm_loadu_ps(M.M[2]);
		const __m128 Row3 = _mm_loadu_ps(M.M[3]);

		// 중심 변환
		const __m128 CX = _mm_shuffle_ps(vCenter, vCenter, _MM_SHUFFLE(0, 0, 0, 0));
		const __m128 CY = _mm_shuffle_ps(vCenter, vCenter, _MM_SHUFFLE(1, 1, 1, 1));
		const __m128 CZ = _mm_shuffle_ps(vCenter, vCenter, _MM_SHUFFLE(2, 2, 2, 2));

		__m128 WorldCenter = _mm_add_ps(_mm_mul_ps(CX, Row0), _mm_mul_ps(CY, Row1));
		WorldCenter = _mm_add_ps(WorldCenter, _mm_add_ps(_mm_mul_ps(CZ, Row2), Row3));

		// 절댓값 연산
		const __m128 AbsMask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
		const __m128 AbsRow0 = _mm_and_ps(Row0, AbsMask);
		const __m128 AbsRow1 = _mm_and_ps(Row1, AbsMask);
		const __m128 AbsRow2 = _mm_and_ps(Row2, AbsMask);

		// 범위 변환
		const __m128 EX = _mm_shuffle_ps(vExtent, vExtent, _MM_SHUFFLE(0, 0, 0, 0));
		const __m128 EY = _mm_shuffle_ps(vExtent, vExtent, _MM_SHUFFLE(1, 1, 1, 1));
		const __m128 EZ = _mm_shuffle_ps(vExtent, vExtent, _MM_SHUFFLE(2, 2, 2, 2));

		__m128 WorldExtent = _mm_add_ps(_mm_mul_ps(EX, AbsRow0), _mm_mul_ps(EY, AbsRow1));
		WorldExtent = _mm_add_ps(WorldExtent, _mm_mul_ps(EZ, AbsRow2));

		// 경계 계산
		const __m128 NewMin = _mm_sub_ps(WorldCenter, WorldExtent);
		const __m128 NewMax = _mm_add_ps(WorldCenter, WorldExtent);

		alignas(16) float OutMin[4];
		alignas(16) float OutMax[4];
		_mm_store_ps(OutMin, NewMin);
		_mm_store_ps(OutMax, NewMax);

		return FBox{
			FVector(OutMin[0], OutMin[1], OutMin[2]),
			FVector(OutMax[0], OutMax[1], OutMax[2])
		};
	}
};

// Cooked obj Data의 Section 구조체
struct FStaticMeshSection
{
	uint32 StartIndex = 0;
	uint32 IndexCount = 0;
	uint32 MaterialSlotIndex = 0;
};

struct FStaticMaterialSlot
{
	FString Name;
	FVector4 BaseColor = FVector4(1, 1, 1, 1);
	FString DiffuseTexturePath;
};

// 메시 로컬 공간에서 피킹 Ray가 통과할 삼각형만 찾기 위한 가속 노드다.
struct FMeshPickingBVHNode
{
	FBox Bounds{};
	uint32 Left = 0;
	uint32 Right = 0;
	uint32 First = 0;
	uint32 Count = 0;
	bool bLeaf = false;
};

// 피킹 BVH의 4갈래 노드 (World·Mesh 공용). 이진 트리의 두 층을 한 층으로 합쳐 내려가는 단계를 절반으로 줄인다.
// 자식 박스 4개를 축별 배열로 모아 SIMD로 한 번에 검사한다. 128B = 캐시 라인 2개.
struct alignas(64) FPickingBVHNode4
{
	static constexpr uint32 EmptySlot = static_cast<uint32>(-1);

	float MinX[4];
	float MinY[4];
	float MinZ[4];
	float MaxX[4];
	float MaxY[4];
	float MaxZ[4];
	uint32 Child[4]; // Count > 0: 리프의 첫 원소 인덱스, Count == 0: 자식 4갈래 노드 인덱스
	uint32 Count[4]; // 리프 원소 수. 0은 안쪽 노드, EmptySlot은 빈 칸

	void SetBounds(const uint32 Slot, const FBox& Bounds)
	{
		MinX[Slot] = Bounds.Min.X;
		MinY[Slot] = Bounds.Min.Y;
		MinZ[Slot] = Bounds.Min.Z;
		MaxX[Slot] = Bounds.Max.X;
		MaxY[Slot] = Bounds.Max.Y;
		MaxZ[Slot] = Bounds.Max.Z;
	}
};

// 피킹 전용 삼각형. 세 꼭짓점 위치만 BVH 리프 순서로 연속 저장해 정밀 검사 때 인덱스·정점 간접 참조를 없앤다.
struct FPickingTriangle
{
	FVector V0;
	FVector V1;
	FVector V2;
};

struct FStaticMeshData
{
	TArray<FVertexPNCT> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
	TArray<FStaticMaterialSlot> MaterialSlots;
	FBox AABB;

	// 동일 Mesh를 사용하는 모든 Component가 공유한다. 첫 정밀 피킹 때 한 번만 구축한다.
	mutable TArray<uint32> PickingTriangleIndices;
	mutable TArray<FMeshPickingBVHNode> PickingBVHNodes;
	// 피킹 순회용 4갈래 노드. 이진 노드(PickingBVHNodes)를 구축한 뒤 두 층씩 합쳐 만든다.
	mutable TArray<FPickingBVHNode4> PickingBVHNodes4;
	// PickingTriangleIndices와 같은 순서(BVH 리프 순서)의 삼각형 위치. BVH 리프 검사는 이 배열만 읽는다.
	mutable TArray<FPickingTriangle> PickingTriangles;
	mutable bool bPickingBVHBuilt = false;

	void InvalidatePickingBVH()
	{
		PickingTriangleIndices.Reset();
		PickingBVHNodes.Reset();
		PickingBVHNodes4.Reset();
		PickingTriangles.Reset();
		bPickingBVHBuilt = false;
	}

	// TODO: 나중에 Sections, MaterialSlots 도 Append 해줘야 함.
	void Append(const FStaticMeshData& Other)
	{
		InvalidatePickingBVH();
		uint32 Base = (uint32)Vertices.Num();

		Vertices.Append(Other.Vertices);

		for (uint32 i : Other.Indices)
			Indices.Add(Base + i);
	}

	void Translate(const FVector& Offset)
	{
		InvalidatePickingBVH();
		for (auto& V : Vertices)
			V.Position += Offset;
	}

	// Vertices, Indices, Sections, MaterialSlots가 서로 맞는지 검사한다.
	// 실패하면 이유를 OutError에 담는다. 로그는 호출한 쪽이 경로와 함께 남긴다.
	bool Validate(FString& OutError) const;
};
