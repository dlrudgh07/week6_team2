#include "EnginePCH.h"
#include "PrimitiveBVH.h"

#include "Component/StaticMeshComponent.h"

#include <algorithm>
#include <array>
#include <limits>

namespace
{
	constexpr uint32 PrimitiveBVHLeafSize = 16;
	constexpr int32 SAHBinCount = 16;
	constexpr float SplitEpsilon = 1.0e-6f;

	FBox UnionBounds(const FBox& A, const FBox& B)
	{
		return {
			FVector(
				(std::min)(A.Min.X, B.Min.X),
				(std::min)(A.Min.Y, B.Min.Y),
				(std::min)(A.Min.Z, B.Min.Z)),
			FVector(
				(std::max)(A.Max.X, B.Max.X),
				(std::max)(A.Max.Y, B.Max.Y),
				(std::max)(A.Max.Z, B.Max.Z))};
	}

	FVector BoundsCenter(const FBox& Bounds)
	{
		return (Bounds.Min + Bounds.Max) * 0.5f;
	}

	float SurfaceArea(const FBox& Bounds)
	{
		const FVector Size = Bounds.Max - Bounds.Min;
		return 2.0f * (Size.X * Size.Y + Size.Y * Size.Z + Size.Z * Size.X);
	}

	float AxisValue(const FVector& Value, const int32 Axis)
	{
		return Axis == 0 ? Value.X : Axis == 1 ? Value.Y : Value.Z;
	}

	bool CanUseBoundsForPicking(UPrimitiveComponent* Primitive)
	{
		return Cast<UStaticMeshComponent>(Primitive) != nullptr;
	}
}

void FPrimitiveBVH::Update(
	const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Primitives, const uint64 TopologyRevision,
	const TArray<TWeakObjectPtr<UPrimitiveComponent>>& DirtyPrimitives)
{
	if (LastTopologyRevision != TopologyRevision)
	{
		LastTopologyRevision = TopologyRevision;
		UpdateBoundedPrimitives.Reset();
		UpdateBypassPrimitives.Reset();
		UpdateBoundedPrimitives.Reserve(Primitives.Num());
		UpdateBypassPrimitives.Reserve(Primitives.Num());

		for (const TWeakObjectPtr<UPrimitiveComponent>& WeakPrimitive : Primitives)
		{
			UPrimitiveComponent* Primitive = WeakPrimitive.Get();
			if (!Primitive)
				continue;

			if (CanUseBoundsForPicking(Primitive))
				UpdateBoundedPrimitives.Add(Primitive);
			else
				UpdateBypassPrimitives.Add(Primitive);
		}

		Entries.Reset();
		EntryBoundsRevisions.Reset();
		BypassPrimitives.Reset();
		PrimitiveIndices.Reset();
		EntryLeafNodes.Reset();
		Nodes.Reset();
		Nodes4.Reset();
		NodeParents.Reset();
		NodeRefitSerials.Reset();
		NodeSlots.Reset();
		EntryIndexByObjectIndex.clear();

		Entries.Reserve(UpdateBoundedPrimitives.Num());
		EntryBoundsRevisions.Reserve(UpdateBoundedPrimitives.Num());
		PrimitiveIndices.Reserve(UpdateBoundedPrimitives.Num());
		Nodes.Reserve((std::max)(1, UpdateBoundedPrimitives.Num() * 2));
		NodeParents.Reserve((std::max)(1, UpdateBoundedPrimitives.Num() * 2));
		NodeRefitSerials.Reserve((std::max)(1, UpdateBoundedPrimitives.Num() * 2));
		for (UPrimitiveComponent* Primitive : UpdateBoundedPrimitives)
		{
			FEntry Entry{};
			Entry.Primitive = Primitive;
			Entry.Bounds = Primitive->GetWorldBounds();
			const uint32 EntryIndex = Entries.Add(std::move(Entry));
			EntryBoundsRevisions.Add(Primitive->GetBoundsRevision());
			PrimitiveIndices.Add(EntryIndex);
			const uint32 ObjectIndex = Primitive->GetInternalIndex();
			if (EntryIndexByObjectIndex.size() <= ObjectIndex)
				EntryIndexByObjectIndex.resize(static_cast<size_t>(ObjectIndex) + 1, -1);
			EntryIndexByObjectIndex[ObjectIndex] = static_cast<int32>(EntryIndex);
		}
		EntryLeafNodes.SetNum(Entries.Num(), false);

		BypassPrimitives.Reserve(UpdateBypassPrimitives.Num());
		for (UPrimitiveComponent* Primitive : UpdateBypassPrimitives)
			BypassPrimitives.Add(Primitive);

		if (!PrimitiveIndices.IsEmpty())
		{
			BuildNode(0, static_cast<uint32>(PrimitiveIndices.Num()), InvalidNodeIndex);
			SortEntriesByLeaf();
			NodeSlots.Init(InvalidNodeIndex, Nodes.Num());
			// 루트가 리프면(Primitive가 리프 크기 이하) 4갈래 노드 없이 리프 하나로 검사한다
			if (Nodes[0].Count == 0)
			{
				Nodes4.Reserve(Nodes.Num() / 2);
				BuildNode4(0);
			}
		}
		return;
	}

	UpdateDirtyEntries.Reset();
	for (const TWeakObjectPtr<UPrimitiveComponent>& WeakPrimitive : DirtyPrimitives)
	{
		UPrimitiveComponent* Primitive = WeakPrimitive.Get();
		if (!Primitive)
			continue;

		const uint32 ObjectIndex = Primitive->GetInternalIndex();
		if (ObjectIndex >= EntryIndexByObjectIndex.size())
			continue;

		const int32 EntryIndex = EntryIndexByObjectIndex[ObjectIndex];
		if (EntryIndex < 0 || Entries[EntryIndex].Primitive != Primitive)
			continue;

		FEntry& Entry = Entries[EntryIndex];
		if (EntryBoundsRevisions[EntryIndex] != Primitive->GetBoundsRevision())
		{
			Entry.Bounds = Primitive->GetWorldBounds();
			EntryBoundsRevisions[EntryIndex] = Primitive->GetBoundsRevision();
			UpdateDirtyEntries.Add(static_cast<uint32>(EntryIndex));
		}
	}

	if (UpdateDirtyEntries.IsEmpty() || Nodes.IsEmpty())
		return;

	const int32 FullRefitThreshold = (std::max)(64, Entries.Num() / 128);
	if (UpdateDirtyEntries.Num() >= FullRefitThreshold)
	{
		RefitNode(0);
		return;
	}

	++RefitSerial;
	UpdateDirtyLeaves.Reset();
	for (const uint32 EntryIndex : UpdateDirtyEntries)
	{
		const uint32 LeafIndex = EntryLeafNodes[EntryIndex];
		if (NodeRefitSerials[LeafIndex] != RefitSerial)
		{
			NodeRefitSerials[LeafIndex] = RefitSerial;
			UpdateDirtyLeaves.Add(LeafIndex);
		}
	}
	for (const uint32 LeafIndex : UpdateDirtyLeaves)
		RefitFromLeaf(LeafIndex);
}

void FPrimitiveBVH::Reset()
{
	Entries.Reset();
	EntryBoundsRevisions.Reset();
	BypassPrimitives.Reset();
	PrimitiveIndices.Reset();
	EntryLeafNodes.Reset();
	Nodes.Reset();
	Nodes4.Reset();
	NodeParents.Reset();
	NodeRefitSerials.Reset();
	NodeSlots.Reset();
	EntryIndexByObjectIndex.clear();
	UpdateBoundedPrimitives.Reset();
	UpdateBypassPrimitives.Reset();
	UpdateDirtyEntries.Reset();
	UpdateDirtyLeaves.Reset();
	LastTopologyRevision = 0;
	RefitSerial = 0;
}

void FPrimitiveBVH::GatherRayCandidates(const FRay& Ray,
	TArray<FLineTraceCandidate>& OutCandidates) const
{
	OutCandidates.Reset();
	const FTraceContext Context = MakeTraceContext(Ray, nullptr, nullptr);
	if (!Nodes.IsEmpty())
	{
		float RootDistance = 0.0f;
		if (RayIntersectsAABB(
			Context, Nodes[0].Bounds.Min, Nodes[0].Bounds.Max, RootDistance))
		{
			TraverseRay(Context, 0, OutCandidates);
		}
	}

	for (UPrimitiveComponent* Primitive : BypassPrimitives)
	{
		if (Primitive)
			OutCandidates.Add({Primitive, 0.0f, false});
	}

	std::sort(OutCandidates.begin(), OutCandidates.end(),
		[](const FLineTraceCandidate& A, const FLineTraceCandidate& B)
		{
			if (A.bHasBoundsDistance != B.bHasBoundsDistance)
				return A.bHasBoundsDistance;
			return A.bHasBoundsDistance && A.BoundsDistance < B.BoundsDistance;
		});
}

void FPrimitiveBVH::TraceRayClosest(FTraceContext& Context, const FRayNarrowTestFn NarrowTest, void* UserContext) const
{
	// View에 따라 형상이 달라지는 Primitive는 Bounds를 신뢰할 수 없으므로 거리와 무관하게 먼저 정밀 검사한다
	for (UPrimitiveComponent* Primitive : BypassPrimitives)
	{
		if (Primitive)
			NarrowTest(Context, Primitive, UserContext);
	}

	// 4갈래 루트는 자식 박스 4개 검사가 루트 박스 검사를 대신한다
	if (!Nodes4.IsEmpty())
	{
		TraverseRayClosest(Context, 0, 0.0f, NarrowTest, UserContext);
	}
	else if (!Nodes.IsEmpty())
	{
		float RootDistance = 0.0f;
		if (RayIntersectsAABB(Context, Nodes[0].Bounds.Min, Nodes[0].Bounds.Max, RootDistance))
			TraceLeaf(Context, Nodes[0].First, Nodes[0].Count, NarrowTest, UserContext);
	}
}

uint32 FPrimitiveBVH::BuildNode(const uint32 First, const uint32 Count, const uint32 Parent)
{
	FBox NodeBounds = Entries[PrimitiveIndices[First]].Bounds;
	FVector CentroidMin = BoundsCenter(NodeBounds);
	FVector CentroidMax = CentroidMin;
	for (uint32 Offset = 1; Offset < Count; ++Offset)
	{
		const FBox& Bounds = Entries[PrimitiveIndices[First + Offset]].Bounds;
		const FVector Center = BoundsCenter(Bounds);
		NodeBounds = UnionBounds(NodeBounds, Bounds);
		CentroidMin.X = (std::min)(CentroidMin.X, Center.X);
		CentroidMin.Y = (std::min)(CentroidMin.Y, Center.Y);
		CentroidMin.Z = (std::min)(CentroidMin.Z, Center.Z);
		CentroidMax.X = (std::max)(CentroidMax.X, Center.X);
		CentroidMax.Y = (std::max)(CentroidMax.Y, Center.Y);
		CentroidMax.Z = (std::max)(CentroidMax.Z, Center.Z);
	}

	FNode Node{};
	Node.Bounds = NodeBounds;
	const uint32 NodeIndex = Nodes.Add(Node);
	NodeParents.Add(Parent);
	NodeRefitSerials.Add(0);
	if (Count <= PrimitiveBVHLeafSize)
	{
		Nodes[NodeIndex].First = First;
		Nodes[NodeIndex].Count = Count;
		for (uint32 Offset = 0; Offset < Count; ++Offset)
			EntryLeafNodes[PrimitiveIndices[First + Offset]] = NodeIndex;
		return NodeIndex;
	}

	struct FBin
	{
		FBox Bounds{};
		uint32 Count = 0;
		bool bValid = false;
	};

	float BestCost = (std::numeric_limits<float>::max)();
	int32 BestAxis = -1;
	int32 BestSplit = -1;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float Minimum = AxisValue(CentroidMin, Axis);
		const float Extent = AxisValue(CentroidMax, Axis) - Minimum;
		if (Extent <= SplitEpsilon)
			continue;

		std::array<FBin, SAHBinCount> Bins{};
		for (uint32 Offset = 0; Offset < Count; ++Offset)
		{
			const FBox& Bounds = Entries[PrimitiveIndices[First + Offset]].Bounds;
			const int32 BinIndex = std::clamp(
				static_cast<int32>((AxisValue(BoundsCenter(Bounds), Axis) - Minimum) / Extent * SAHBinCount),
				0, SAHBinCount - 1);
			FBin& Bin = Bins[BinIndex];
			Bin.Bounds = Bin.bValid ? UnionBounds(Bin.Bounds, Bounds) : Bounds;
			Bin.bValid = true;
			++Bin.Count;
		}

		std::array<FBox, SAHBinCount> LeftBounds{};
		std::array<FBox, SAHBinCount> RightBounds{};
		std::array<uint32, SAHBinCount> LeftCounts{};
		std::array<uint32, SAHBinCount> RightCounts{};
		bool bLeftValid = false;
		bool bRightValid = false;
		for (int32 BinIndex = 0; BinIndex < SAHBinCount; ++BinIndex)
		{
			if (Bins[BinIndex].bValid)
			{
				LeftBounds[BinIndex] = bLeftValid
					? UnionBounds(LeftBounds[BinIndex - 1], Bins[BinIndex].Bounds)
					: Bins[BinIndex].Bounds;
				bLeftValid = true;
			}
			else if (BinIndex > 0)
			{
				LeftBounds[BinIndex] = LeftBounds[BinIndex - 1];
			}
			LeftCounts[BinIndex] = Bins[BinIndex].Count +
				(BinIndex > 0 ? LeftCounts[BinIndex - 1] : 0);
		}
		for (int32 BinIndex = SAHBinCount - 1; BinIndex >= 0; --BinIndex)
		{
			if (Bins[BinIndex].bValid)
			{
				RightBounds[BinIndex] = bRightValid
					? UnionBounds(RightBounds[BinIndex + 1], Bins[BinIndex].Bounds)
					: Bins[BinIndex].Bounds;
				bRightValid = true;
			}
			else if (BinIndex + 1 < SAHBinCount)
			{
				RightBounds[BinIndex] = RightBounds[BinIndex + 1];
			}
			RightCounts[BinIndex] = Bins[BinIndex].Count +
				(BinIndex + 1 < SAHBinCount ? RightCounts[BinIndex + 1] : 0);
		}

		for (int32 Split = 0; Split < SAHBinCount - 1; ++Split)
		{
			if (LeftCounts[Split] == 0 || RightCounts[Split + 1] == 0)
				continue;
			const float Cost = SurfaceArea(LeftBounds[Split]) * static_cast<float>(LeftCounts[Split]) +
				SurfaceArea(RightBounds[Split + 1]) * static_cast<float>(RightCounts[Split + 1]);
			if (Cost < BestCost)
			{
				BestCost = Cost;
				BestAxis = Axis;
				BestSplit = Split;
			}
		}
	}

	auto Begin = PrimitiveIndices.begin() + First;
	auto End = Begin + Count;
	uint32 LeftCount = 0;
	if (BestAxis >= 0)
	{
		const float Minimum = AxisValue(CentroidMin, BestAxis);
		const float Extent = AxisValue(CentroidMax, BestAxis) - Minimum;
		const auto Middle = std::partition(Begin, End,
			[&](const uint32 EntryIndex)
			{
				const float Center = AxisValue(BoundsCenter(Entries[EntryIndex].Bounds), BestAxis);
				const int32 Bin = std::clamp(
					static_cast<int32>((Center - Minimum) / Extent * SAHBinCount), 0, SAHBinCount - 1);
				return Bin <= BestSplit;
			});
		LeftCount = static_cast<uint32>(Middle - Begin);
	}

	if (LeftCount == 0 || LeftCount == Count)
	{
		const FVector Range = CentroidMax - CentroidMin;
		const int32 Axis = Range.X >= Range.Y && Range.X >= Range.Z ? 0 : Range.Y >= Range.Z ? 1 : 2;
		LeftCount = Count / 2;
		std::nth_element(Begin, Begin + LeftCount, End,
			[&](const uint32 A, const uint32 B)
			{
				return AxisValue(BoundsCenter(Entries[A].Bounds), Axis) <
					AxisValue(BoundsCenter(Entries[B].Bounds), Axis);
			});
	}

	const uint32 Left = BuildNode(First, LeftCount, NodeIndex);
	const uint32 Right = BuildNode(First + LeftCount, Count - LeftCount, NodeIndex);
	Nodes[NodeIndex].Left = Left;
	Nodes[NodeIndex].Right = Right;
	return NodeIndex;
}

// 리프 순서대로 Entry를 다시 놓는다. 리프 검사가 흩어진 Entry 대신 연속된 메모리를 읽고, PrimitiveIndices는 0, 1, 2...가 된다.
void FPrimitiveBVH::SortEntriesByLeaf()
{
	TArray<FEntry> SortedEntries;
	TArray<uint64> SortedRevisions;
	TArray<uint32> SortedLeafNodes;
	SortedEntries.Reserve(Entries.Num());
	SortedRevisions.Reserve(Entries.Num());
	SortedLeafNodes.Reserve(Entries.Num());
	for (const uint32 EntryIndex : PrimitiveIndices)
	{
		SortedEntries.Add(Entries[EntryIndex]);
		SortedRevisions.Add(EntryBoundsRevisions[EntryIndex]);
		SortedLeafNodes.Add(EntryLeafNodes[EntryIndex]);
	}

	for (uint32 Index = 0; Index < static_cast<uint32>(SortedEntries.Num()); ++Index)
	{
		PrimitiveIndices[Index] = Index;
		EntryIndexByObjectIndex[SortedEntries[Index].Primitive->GetInternalIndex()] = static_cast<int32>(Index);
	}
	Entries = std::move(SortedEntries);
	EntryBoundsRevisions = std::move(SortedRevisions);
	EntryLeafNodes = std::move(SortedLeafNodes);
}

// 이진 트리를 4갈래로 합친다. 자식이 4개가 될 때까지 면적이 가장 큰 안쪽 자식을 그 자식 둘로 펼친다.
uint32 FPrimitiveBVH::BuildNode4(const uint32 NodeIndex)
{
	uint32 Children[4] = {Nodes[NodeIndex].Left, Nodes[NodeIndex].Right, 0, 0};
	uint32 ChildCount = 2;
	while (ChildCount < 4)
	{
		int32 Widest = -1;
		for (uint32 Index = 0; Index < ChildCount; ++Index)
		{
			if (Nodes[Children[Index]].Count == 0 && (Widest < 0 || SurfaceArea(Nodes[Children[Index]].Bounds) > SurfaceArea(Nodes[Children[Widest]].Bounds)))
				Widest = static_cast<int32>(Index);
		}
		if (Widest < 0)
			break;
		const FNode& Expanded = Nodes[Children[Widest]];
		Children[Widest] = Expanded.Left;
		Children[ChildCount++] = Expanded.Right;
	}

	FPickingBVHNode4 Node4{};
	const uint32 Node4Index = Nodes4.Add(Node4);
	for (uint32 Slot = 0; Slot < 4; ++Slot)
	{
		if (Slot >= ChildCount)
		{
			Node4.Count[Slot] = FPickingBVHNode4::EmptySlot;
			continue;
		}
		const FNode& Child = Nodes[Children[Slot]];
		Node4.SetBounds(Slot, Child.Bounds);
		Node4.Child[Slot] = Child.Count > 0 ? Child.First : BuildNode4(Children[Slot]);
		Node4.Count[Slot] = Child.Count;
		NodeSlots[Children[Slot]] = Node4Index * 4 + Slot;
	}
	Nodes4[Node4Index] = Node4;
	return Node4Index;
}

// refit으로 바뀐 이진 노드 박스를 그 노드가 들어간 4갈래 칸에 옮긴다 (칸이 없는 노드는 순회에 쓰이지 않는다)
void FPrimitiveBVH::SyncNode4Bounds(const uint32 NodeIndex)
{
	const uint32 SlotIndex = NodeSlots[NodeIndex];
	if (SlotIndex != InvalidNodeIndex)
		Nodes4[SlotIndex / 4].SetBounds(SlotIndex % 4, Nodes[NodeIndex].Bounds);
}

FBox FPrimitiveBVH::RefitNode(const uint32 NodeIndex)
{
	FNode& Node = Nodes[NodeIndex];
	if (Node.Count > 0)
	{
		FBox Bounds = Entries[PrimitiveIndices[Node.First]].Bounds;
		for (uint32 Offset = 1; Offset < Node.Count; ++Offset)
			Bounds = UnionBounds(Bounds, Entries[PrimitiveIndices[Node.First + Offset]].Bounds);
		Node.Bounds = Bounds;
		SyncNode4Bounds(NodeIndex);
		return Bounds;
	}

	Node.Bounds = UnionBounds(RefitNode(Node.Left), RefitNode(Node.Right));
	SyncNode4Bounds(NodeIndex);
	return Node.Bounds;
}

void FPrimitiveBVH::RefitFromLeaf(const uint32 LeafIndex)
{
	FNode& Leaf = Nodes[LeafIndex];
	FBox Bounds = Entries[PrimitiveIndices[Leaf.First]].Bounds;
	for (uint32 Offset = 1; Offset < Leaf.Count; ++Offset)
		Bounds = UnionBounds(Bounds, Entries[PrimitiveIndices[Leaf.First + Offset]].Bounds);
	Leaf.Bounds = Bounds;
	SyncNode4Bounds(LeafIndex);

	uint32 ParentIndex = NodeParents[LeafIndex];
	while (ParentIndex != InvalidNodeIndex)
	{
		FNode& Parent = Nodes[ParentIndex];
		Parent.Bounds = UnionBounds(Nodes[Parent.Left].Bounds, Nodes[Parent.Right].Bounds);
		SyncNode4Bounds(ParentIndex);
		ParentIndex = NodeParents[ParentIndex];
	}
}

void FPrimitiveBVH::TraverseRay(const FTraceContext& Context, const uint32 NodeIndex,
	TArray<FLineTraceCandidate>& OutCandidates) const
{
	const FNode& Node = Nodes[NodeIndex];
	if (Node.Count > 0)
	{
		for (uint32 Offset = 0; Offset < Node.Count; ++Offset)
		{
			const FEntry& Entry = Entries[PrimitiveIndices[Node.First + Offset]];
			UPrimitiveComponent* Primitive = Entry.Primitive;
			float BoundsDistance = 0.0f;
			if (Primitive && RayIntersectsAABB(
				Context, Entry.Bounds.Min, Entry.Bounds.Max, BoundsDistance))
			{
				OutCandidates.Add({Primitive, BoundsDistance, true});
			}
		}
		return;
	}

	float LeftDistance = 0.0f;
	float RightDistance = 0.0f;
	const FNode& LeftNode = Nodes[Node.Left];
	const FNode& RightNode = Nodes[Node.Right];
	const bool bHitLeft = RayIntersectsAABB(
		Context, LeftNode.Bounds.Min, LeftNode.Bounds.Max, LeftDistance);
	const bool bHitRight = RayIntersectsAABB(
		Context, RightNode.Bounds.Min, RightNode.Bounds.Max, RightDistance);

	if (bHitLeft && bHitRight)
	{
		const uint32 NearNode = LeftDistance <= RightDistance ? Node.Left : Node.Right;
		const uint32 FarNode = LeftDistance <= RightDistance ? Node.Right : Node.Left;
		TraverseRay(Context, NearNode, OutCandidates);
		TraverseRay(Context, FarNode, OutCandidates);
	}
	else if (bHitLeft)
	{
		TraverseRay(Context, Node.Left, OutCandidates);
	}
	else if (bHitRight)
	{
		TraverseRay(Context, Node.Right, OutCandidates);
	}
}

void FPrimitiveBVH::TraverseRayClosest(FTraceContext& Context, const uint32 Node4Index, const float NodeDistance, const FRayNarrowTestFn NarrowTest, void* UserContext) const
{
	// 노드 박스가 이미 실제로 맞은 거리보다 멀면 안의 Primitive는 더 가까울 수 없다
	// (박스 진입 거리로 갱신하면 박스만 스치는 앞 객체 때문에 뒤 객체를 놓치므로 실제 교차 거리만 사용한다)
	if (NodeDistance >= Context.BestDistance)
		return;

	// 자식 박스 4개를 한 번에 검사하고, 통과한 자식을 가까운 순서로 받는다
	const FPickingBVHNode4& Node = Nodes4[Node4Index];
	float Distances[4];
	uint32 Slots[4];
	const uint32 HitCount = RayIntersectsNode4(Context, Node, Distances, Slots);

	// 가까운 자식 먼저. 먼 자식은 앞 자식에서 갱신된 최근접 거리로 다시 판정한다
	for (uint32 Index = 0; Index < HitCount; ++Index)
	{
		const uint32 Slot = Slots[Index];
		if (Distances[Slot] >= Context.BestDistance)
			break;
		if (Node.Count[Slot] > 0)
			TraceLeaf(Context, Node.Child[Slot], Node.Count[Slot], NarrowTest, UserContext);
		else
			TraverseRayClosest(Context, Node.Child[Slot], Distances[Slot], NarrowTest, UserContext);
	}
}

// 리프 안에서 박스를 통과한 Primitive만 모아 박스 진입 거리순으로 정밀 검사한다
void FPrimitiveBVH::TraceLeaf(FTraceContext& Context, const uint32 First, const uint32 Count, const FRayNarrowTestFn NarrowTest, void* UserContext) const
{
	FRayHit Hits[PrimitiveBVHLeafSize];
	uint32 HitCount = 0;
	for (uint32 Offset = 0; Offset < Count && HitCount < PrimitiveBVHLeafSize; ++Offset)
	{
		const FEntry& Entry = Entries[First + Offset];
		float BoundsDistance = 0.0f;
		if (Entry.Primitive && RayIntersectsAABB(Context, Entry.Bounds.Min, Entry.Bounds.Max, BoundsDistance) && BoundsDistance < Context.BestDistance)
			Hits[HitCount++] = {BoundsDistance, Entry.Primitive};
	}

	std::sort(Hits, Hits + HitCount, [](const FRayHit& A, const FRayHit& B) { return A.Distance < B.Distance; });
	for (uint32 Index = 0; Index < HitCount && Hits[Index].Distance < Context.BestDistance; ++Index)
		NarrowTest(Context, Hits[Index].Primitive, UserContext);
}
