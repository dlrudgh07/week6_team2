#pragma once

#include "Collision/Ray.h"
#include "Component/PrimitiveComponent.h"
#include "ObjectSystem/TWeakObjectPtr.h"

#include <vector>

// World의 Primitive Bounds를 보관하는 피킹 전용 공간 인덱스다.
// View에 따라 형상이 달라지는 Primitive는 Bypass 목록에 두고 정밀 검사에서 처리한다.
class FPrimitiveBVH
{
public:
	void Update(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Primitives, uint64 TopologyRevision,
		const TArray<TWeakObjectPtr<UPrimitiveComponent>>& DirtyPrimitives);
	void Reset();
	void GatherRayCandidates(const FRay& Ray, TArray<FLineTraceCandidate>& OutCandidates) const;

	// 피킹 정밀 검사 콜백. 실제로 더 가까이 맞았을 때만 Context.BestDistance를 갱신해야 한다.
	using FRayNarrowTestFn = void (*)(FTraceContext& Context, UPrimitiveComponent* Primitive, void* UserContext);
	// 가까운 노드부터 순회하며 박스를 통과한 Primitive를 바로 정밀 검사한다.
	// 실제 교차로 갱신된 Context.BestDistance보다 박스가 먼 노드·Primitive는 건너뛴다.
	void TraceRayClosest(FTraceContext& Context, FRayNarrowTestFn NarrowTest, void* UserContext) const;

	int32 GetPrimitiveCount() const { return Entries.Num() + BypassPrimitives.Num(); }

private:
	static constexpr uint32 InvalidNodeIndex = static_cast<uint32>(-1);

	struct FEntry
	{
		UPrimitiveComponent* Primitive = nullptr;
		FBox Bounds{};
	};

	struct FNode
	{
		FBox Bounds{};
		uint32 Left = 0;
		uint32 Right = 0;
		uint32 First = 0;
		uint32 Count = 0;
	};

	// 박스를 통과한 피킹 후보와 박스 진입 거리
	// 리프마다 스택 배열로 만들어 쓰므로 기본값 초기화를 두지 않는다. 채운 개수(HitCount)만 읽는다.
	struct FRayHit
	{
		float Distance;
		UPrimitiveComponent* Primitive;
	};

	uint32 BuildNode(uint32 First, uint32 Count, uint32 Parent);
	void SortEntriesByLeaf();
	uint32 BuildNode4(uint32 NodeIndex);
	void SyncNode4Bounds(uint32 NodeIndex);
	FBox RefitNode(uint32 NodeIndex);
	void RefitFromLeaf(uint32 LeafIndex);
	void TraverseRay(const FTraceContext& Context, uint32 NodeIndex,
		TArray<FLineTraceCandidate>& OutCandidates) const;
	void TraverseRayClosest(FTraceContext& Context, uint32 Node4Index, float NodeDistance, FRayNarrowTestFn NarrowTest, void* UserContext) const;
	void TraceLeaf(FTraceContext& Context, uint32 First, uint32 Count, FRayNarrowTestFn NarrowTest, void* UserContext) const;

	TArray<FEntry> Entries;
	TArray<uint64> EntryBoundsRevisions;
	TArray<UPrimitiveComponent*> BypassPrimitives;
	TArray<uint32> PrimitiveIndices;
	TArray<uint32> EntryLeafNodes;
	TArray<FNode> Nodes;
	// 피킹 순회용 4갈래 노드. 이진 트리(Nodes)는 구축·refit에 쓰고, 박스가 바뀌면 같은 칸에 복사한다.
	TArray<FPickingBVHNode4> Nodes4;
	// Query 시 건드리지 않는 refit 메타데이터는 Node와 분리해 traversal cache 밀도를 유지한다.
	TArray<uint32> NodeParents;
	TArray<uint64> NodeRefitSerials;
	// 이진 노드가 들어간 4갈래 칸 (Node4 인덱스 * 4 + 칸). 4갈래로 합치며 빠진 노드와 루트는 InvalidNodeIndex
	TArray<uint32> NodeSlots;
	std::vector<int32> EntryIndexByObjectIndex;
	uint64 LastTopologyRevision = 0;
	uint64 RefitSerial = 0;

	// Update에서 용량을 재사용해 매 프레임 임시 할당을 피한다.
	TArray<UPrimitiveComponent*> UpdateBoundedPrimitives;
	TArray<UPrimitiveComponent*> UpdateBypassPrimitives;
	TArray<uint32> UpdateDirtyEntries;
	TArray<uint32> UpdateDirtyLeaves;
};
