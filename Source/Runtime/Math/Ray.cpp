#include "EnginePCH.h"
#include "Ray.h"
#include "Rendering/StaticMeshData.h"
#include "Math/EngineMath.h"

#include <algorithm>
#include <array>
#include <vector>

namespace
{
constexpr uint32 PickingBVHLeafTriangles = 8;

FVector MinVector(const FVector& A, const FVector& B)
{
	return {std::min(A.X, B.X), std::min(A.Y, B.Y), std::min(A.Z, B.Z)};
}

FVector MaxVector(const FVector& A, const FVector& B)
{
	return {std::max(A.X, B.X), std::max(A.Y, B.Y), std::max(A.Z, B.Z)};
}

FBox GetTriangleBounds(const FStaticMeshData& Mesh, const uint32 TriangleIndex)
{
	const uint32 FirstIndex = TriangleIndex * 3;
	const FVector& A = Mesh.Vertices[Mesh.Indices[FirstIndex]].Position;
	const FVector& B = Mesh.Vertices[Mesh.Indices[FirstIndex + 1]].Position;
	const FVector& C = Mesh.Vertices[Mesh.Indices[FirstIndex + 2]].Position;
	return {MinVector(A, MinVector(B, C)), MaxVector(A, MaxVector(B, C))};
}

FVector GetTriangleCentroid(const FStaticMeshData& Mesh, const uint32 TriangleIndex)
{
	const uint32 FirstIndex = TriangleIndex * 3;
	return (Mesh.Vertices[Mesh.Indices[FirstIndex]].Position + Mesh.Vertices[Mesh.Indices[FirstIndex + 1]].Position + Mesh.Vertices[Mesh.Indices[FirstIndex + 2]].Position) / 3.0f;
}

float AxisValue(const FVector& Value, const int32 Axis)
{
	if (Axis == 0)
		return Value.X;
	if (Axis == 1)
		return Value.Y;
	return Value.Z;
}

constexpr int32 PickingSAHBinCount = 16;
constexpr float PickingSplitEpsilon = 1.0e-6f;

// 빌드 중 삼각형마다 Indices/Vertices를 다시 따라가지 않도록 경계와 중심을 한 번만 계산해 둔다.
struct FPickingBuildData
{
	std::vector<FBox> TriangleBounds;
	std::vector<FVector> Centroids;
};

float SurfaceArea(const FBox& Bounds)
{
	const FVector Size = Bounds.Max - Bounds.Min;
	return 2.0f * (Size.X * Size.Y + Size.Y * Size.Z + Size.Z * Size.X);
}

int32 SAHBinIndex(const float Centroid, const float Minimum, const float Scale)
{
	return std::clamp(static_cast<int32>((Centroid - Minimum) * Scale), 0, PickingSAHBinCount - 1);
}

uint32 BuildPickingBVHNode(const FStaticMeshData& Mesh, const FPickingBuildData& Data, const uint32 First, const uint32 Count)
{
	const uint32 NodeIndex = Mesh.PickingBVHNodes.Add(FMeshPickingBVHNode{});
	FBox Bounds = Data.TriangleBounds[Mesh.PickingTriangleIndices[First]];
	FVector CentroidMin = Data.Centroids[Mesh.PickingTriangleIndices[First]];
	FVector CentroidMax = CentroidMin;

	for (uint32 Offset = 1; Offset < Count; ++Offset)
	{
		const uint32 TriangleIndex = Mesh.PickingTriangleIndices[First + Offset];
		const FBox& TriangleBounds = Data.TriangleBounds[TriangleIndex];
		Bounds.Min = MinVector(Bounds.Min, TriangleBounds.Min);
		Bounds.Max = MaxVector(Bounds.Max, TriangleBounds.Max);
		const FVector& Centroid = Data.Centroids[TriangleIndex];
		CentroidMin = MinVector(CentroidMin, Centroid);
		CentroidMax = MaxVector(CentroidMax, Centroid);
	}

	if (Count <= PickingBVHLeafTriangles)
	{
		FMeshPickingBVHNode& Node = Mesh.PickingBVHNodes[NodeIndex];
		Node.Bounds = Bounds;
		Node.First = First;
		Node.Count = Count;
		Node.bLeaf = true;
		return NodeIndex;
	}

	// Binned SAH: 세 축을 각각 빈으로 나눠 SA(왼쪽)*N(왼쪽) + SA(오른쪽)*N(오른쪽)이 가장 작은 분할을 고른다.
	struct FBin
	{
		FBox Bounds{};
		uint32 Count = 0;
	};

	auto Begin = Mesh.PickingTriangleIndices.begin() + First;
	auto End = Begin + Count;

	float BestCost = FLT_MAX;
	int32 BestAxis = -1;
	int32 BestSplit = -1;
	float BestMinimum = 0.0f;
	float BestScale = 0.0f;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float Minimum = AxisValue(CentroidMin, Axis);
		const float Extent = AxisValue(CentroidMax, Axis) - Minimum;
		if (Extent <= PickingSplitEpsilon)
			continue;

		const float Scale = static_cast<float>(PickingSAHBinCount) / Extent;
		std::array<FBin, PickingSAHBinCount> Bins{};
		for (auto It = Begin; It != End; ++It)
		{
			const FBox& TriangleBounds = Data.TriangleBounds[*It];
			FBin& Bin = Bins[SAHBinIndex(AxisValue(Data.Centroids[*It], Axis), Minimum, Scale)];
			if (Bin.Count == 0)
			{
				Bin.Bounds = TriangleBounds;
			}
			else
			{
				Bin.Bounds.Min = MinVector(Bin.Bounds.Min, TriangleBounds.Min);
				Bin.Bounds.Max = MaxVector(Bin.Bounds.Max, TriangleBounds.Max);
			}
			++Bin.Count;
		}

		// 오른쪽 누적(빈 i 이상)을 먼저 구하고, 왼쪽 누적을 늘려 가며 분할 비용을 계산한다.
		std::array<FBox, PickingSAHBinCount> RightBounds{};
		std::array<uint32, PickingSAHBinCount> RightCounts{};
		FBox RightAccum{};
		uint32 RightAccumCount = 0;
		for (int32 BinIndex = PickingSAHBinCount - 1; BinIndex >= 1; --BinIndex)
		{
			const FBin& Bin = Bins[BinIndex];
			if (Bin.Count > 0)
			{
				if (RightAccumCount == 0)
				{
					RightAccum = Bin.Bounds;
				}
				else
				{
					RightAccum.Min = MinVector(RightAccum.Min, Bin.Bounds.Min);
					RightAccum.Max = MaxVector(RightAccum.Max, Bin.Bounds.Max);
				}
				RightAccumCount += Bin.Count;
			}
			RightBounds[BinIndex] = RightAccum;
			RightCounts[BinIndex] = RightAccumCount;
		}

		FBox LeftAccum{};
		uint32 LeftAccumCount = 0;
		for (int32 Split = 0; Split < PickingSAHBinCount - 1; ++Split)
		{
			const FBin& Bin = Bins[Split];
			if (Bin.Count > 0)
			{
				if (LeftAccumCount == 0)
				{
					LeftAccum = Bin.Bounds;
				}
				else
				{
					LeftAccum.Min = MinVector(LeftAccum.Min, Bin.Bounds.Min);
					LeftAccum.Max = MaxVector(LeftAccum.Max, Bin.Bounds.Max);
				}
				LeftAccumCount += Bin.Count;
			}
			if (LeftAccumCount == 0 || RightCounts[Split + 1] == 0)
				continue;

			const float Cost = SurfaceArea(LeftAccum) * static_cast<float>(LeftAccumCount) +
				SurfaceArea(RightBounds[Split + 1]) * static_cast<float>(RightCounts[Split + 1]);
			if (Cost < BestCost)
			{
				BestCost = Cost;
				BestAxis = Axis;
				BestSplit = Split;
				BestMinimum = Minimum;
				BestScale = Scale;
			}
		}
	}

	uint32 LeftCount = 0;
	if (BestAxis >= 0)
	{
		const auto Middle = std::partition(Begin,
			End,
			[&](const uint32 TriangleIndex)
			{
				return SAHBinIndex(AxisValue(Data.Centroids[TriangleIndex], BestAxis), BestMinimum, BestScale) <= BestSplit;
			});
		LeftCount = static_cast<uint32>(Middle - Begin);
	}
	// 모든 중심이 같아 SAH 분할이 불가능하면 개수 기준으로 반씩 나눠 재귀가 끝나도록 한다.
	if (LeftCount == 0 || LeftCount == Count)
		LeftCount = Count / 2;

	const uint32 Left = BuildPickingBVHNode(Mesh, Data, First, LeftCount);
	const uint32 Right = BuildPickingBVHNode(Mesh, Data, First + LeftCount, Count - LeftCount);
	FMeshPickingBVHNode& Node = Mesh.PickingBVHNodes[NodeIndex];
	Node.Bounds = Bounds;
	Node.Left = Left;
	Node.Right = Right;
	return NodeIndex;
}

// 이진 트리를 4갈래로 합친다. 자식이 4개가 될 때까지 면적이 가장 큰 안쪽 자식을 그 자식 둘로 펼친다.
uint32 BuildPickingBVHNode4(const FStaticMeshData& Mesh, const uint32 NodeIndex)
{
	const TArray<FMeshPickingBVHNode>& Nodes = Mesh.PickingBVHNodes;
	uint32 Children[4] = {Nodes[NodeIndex].Left, Nodes[NodeIndex].Right, 0, 0};
	uint32 ChildCount = 2;
	while (ChildCount < 4)
	{
		int32 Widest = -1;
		for (uint32 Index = 0; Index < ChildCount; ++Index)
		{
			if (!Nodes[Children[Index]].bLeaf && (Widest < 0 || SurfaceArea(Nodes[Children[Index]].Bounds) > SurfaceArea(Nodes[Children[Widest]].Bounds)))
				Widest = static_cast<int32>(Index);
		}
		if (Widest < 0)
			break;
		const FMeshPickingBVHNode& Expanded = Nodes[Children[Widest]];
		Children[Widest] = Expanded.Left;
		Children[ChildCount++] = Expanded.Right;
	}

	FPickingBVHNode4 Node4{};
	const uint32 Node4Index = Mesh.PickingBVHNodes4.Add(Node4);
	for (uint32 Slot = 0; Slot < 4; ++Slot)
	{
		if (Slot >= ChildCount)
		{
			Node4.Count[Slot] = FPickingBVHNode4::EmptySlot;
			continue;
		}
		const FMeshPickingBVHNode& Child = Nodes[Children[Slot]];
		Node4.SetBounds(Slot, Child.Bounds);
		Node4.Child[Slot] = Child.bLeaf ? Child.First : BuildPickingBVHNode4(Mesh, Children[Slot]);
		Node4.Count[Slot] = Child.bLeaf ? Child.Count : 0;
	}
	Mesh.PickingBVHNodes4[Node4Index] = Node4;
	return Node4Index;
}

void EnsurePickingBVH(const FStaticMeshData& Mesh)
{
	if (Mesh.bPickingBVHBuilt)
		return;

	Mesh.PickingTriangleIndices.Reset();
	Mesh.PickingBVHNodes.Reset();
	Mesh.PickingBVHNodes4.Reset();
	const uint32 TriangleCount = static_cast<uint32>(Mesh.Indices.Num() / 3);
	if (TriangleCount > PickingBVHLeafTriangles)
	{
		Mesh.PickingTriangleIndices.Reserve(TriangleCount);
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
			Mesh.PickingTriangleIndices.Add(TriangleIndex);
		Mesh.PickingBVHNodes.Reserve(TriangleCount * 2);

		FPickingBuildData BuildData;
		BuildData.TriangleBounds.reserve(TriangleCount);
		BuildData.Centroids.reserve(TriangleCount);
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			BuildData.TriangleBounds.push_back(GetTriangleBounds(Mesh, TriangleIndex));
			BuildData.Centroids.push_back(GetTriangleCentroid(Mesh, TriangleIndex));
		}
		BuildPickingBVHNode(Mesh, BuildData, 0, TriangleCount);

		// 리프 순서가 된 삼각형 목록을 따라 위치만 연속 저장한다 (렌더링용 Vertices/Indices는 그대로)
		Mesh.PickingTriangles.Reset();
		Mesh.PickingTriangles.Reserve(TriangleCount);
		for (const uint32 TriangleIndex : Mesh.PickingTriangleIndices)
		{
			const uint32 FirstIndex = TriangleIndex * 3;
			Mesh.PickingTriangles.Add({Mesh.Vertices[Mesh.Indices[FirstIndex]].Position, Mesh.Vertices[Mesh.Indices[FirstIndex + 1]].Position, Mesh.Vertices[Mesh.Indices[FirstIndex + 2]].Position});
		}

		// 루트는 삼각형이 리프 크기보다 많아 항상 안쪽 노드다
		Mesh.PickingBVHNodes4.Reserve(Mesh.PickingBVHNodes.Num() / 2);
		BuildPickingBVHNode4(Mesh, 0);
	}
	Mesh.bPickingBVHBuilt = true;
}

void TraceTriangle(const FRay& Ray, const FStaticMeshData& Mesh, const uint32 TriangleIndex, float& InOutNearestT, bool& bInOutHit)
{
	const uint32 FirstIndex = TriangleIndex * 3;
	const FVector& A = Mesh.Vertices[Mesh.Indices[FirstIndex]].Position;
	const FVector& B = Mesh.Vertices[Mesh.Indices[FirstIndex + 1]].Position;
	const FVector& C = Mesh.Vertices[Mesh.Indices[FirstIndex + 2]].Position;
	float T = FLT_MAX;
	if (RayIntersectsTriangle(Ray, A, B, C, T) && T < InOutNearestT)
	{
		InOutNearestT = T;
		bInOutHit = true;
	}
}

// 피킹 사본의 삼각형을 검사한다. 기존 TraceTriangle과 같은 판정이며 인덱스·정점 배열을 거치지 않는다.
void TraceTriangle(const FRay& Ray, const FPickingTriangle& Triangle, float& InOutNearestT, bool& bInOutHit)
{
	float T = FLT_MAX;
	if (RayIntersectsTriangle(Ray, Triangle.V0, Triangle.V1, Triangle.V2, T) && T < InOutNearestT)
	{
		InOutNearestT = T;
		bInOutHit = true;
	}
}

// NodeDistance는 부모가 이미 구한 이 노드의 박스 진입 거리다 (루트는 0). 박스를 다시 검사하지 않는다.
void TracePickingBVHNode(const FTraceContext& Context, const FStaticMeshData& Mesh, const uint32 Node4Index, const float NodeDistance, float& InOutNearestT, bool& bInOutHit)
{
	if (NodeDistance >= InOutNearestT)
		return;

	// 자식 박스 4개를 한 번에 검사하고, 통과한 자식을 가까운 순서로 받는다
	const FPickingBVHNode4& Node = Mesh.PickingBVHNodes4[Node4Index];
	float Distances[4];
	uint32 Slots[4];
	const uint32 HitCount = RayIntersectsNode4(Context, Node, Distances, Slots);
	for (uint32 Index = 0; Index < HitCount; ++Index)
	{
		const uint32 Slot = Slots[Index];
		if (Distances[Slot] >= InOutNearestT)
			break;
		if (Node.Count[Slot] == 0)
		{
			TracePickingBVHNode(Context, Mesh, Node.Child[Slot], Distances[Slot], InOutNearestT, bInOutHit);
			continue;
		}
		for (uint32 Offset = 0; Offset < Node.Count[Slot]; ++Offset)
			TraceTriangle(Context.Ray, Mesh.PickingTriangles[Node.Child[Slot] + Offset], InOutNearestT, bInOutHit);
	}
}
} // namespace

void PrepareMeshPickingBVH(const FStaticMeshData& Mesh)
{
	EnsurePickingBVH(Mesh);
}

FRay ToLocalRay(const FRay& WorldRay, const FMatrix& WorldMatrix)
{
	// ray를 로컬공간으로
	FMatrix invWorld = WorldMatrix.Inverse();
	FRay LocalRay{};
	LocalRay.Origin = invWorld.TransformPosition(WorldRay.Origin);
	LocalRay.Direction = invWorld.TransformVector(WorldRay.Direction);

	return LocalRay;
}

bool ToLocalRayAffine(const FRay& WorldRay, const FMatrix& WorldMatrix, FRay& OutLocalRay)
{
	// 행벡터 규약(v * M): 0~2행은 축, 3행은 이동. 역행렬의 열 = 두 축의 외적 / det
	const FVector R0(WorldMatrix.M[0][0], WorldMatrix.M[0][1], WorldMatrix.M[0][2]);
	const FVector R1(WorldMatrix.M[1][0], WorldMatrix.M[1][1], WorldMatrix.M[1][2]);
	const FVector R2(WorldMatrix.M[2][0], WorldMatrix.M[2][1], WorldMatrix.M[2][2]);
	const FVector C0 = FVector::Cross(R1, R2);
	const FVector C1 = FVector::Cross(R2, R0);
	const FVector C2 = FVector::Cross(R0, R1);
	const float Det = FVector::Dot(R0, C0);
	if (fabsf(Det) < 1e-12f)
		return false;

	const float InvDet = 1.0f / Det;
	const FVector P(WorldRay.Origin.X - WorldMatrix.M[3][0], WorldRay.Origin.Y - WorldMatrix.M[3][1], WorldRay.Origin.Z - WorldMatrix.M[3][2]);
	OutLocalRay.Origin = FVector(FVector::Dot(P, C0), FVector::Dot(P, C1), FVector::Dot(P, C2)) * InvDet;
	OutLocalRay.Direction = FVector(FVector::Dot(WorldRay.Direction, C0), FVector::Dot(WorldRay.Direction, C1), FVector::Dot(WorldRay.Direction, C2)) * InvDet;
	return true;
}

bool RayIntersectsAABB(const FRay& Ray, const FVector& BoxMin, const FVector& BoxMax, float& OutT)
{
	float invRayDir = 1.0f / Ray.Direction.X;
	float tX1 = (BoxMin.X - Ray.Origin.X) * invRayDir;
	float tX2 = (BoxMax.X - Ray.Origin.X) * invRayDir;
	float tMinX = fmin(tX1, tX2);
	float tMaxX = fmax(tX1, tX2);

	invRayDir = 1.0f / Ray.Direction.Y;
	float tY1 = (BoxMin.Y - Ray.Origin.Y) * invRayDir;
	float tY2 = (BoxMax.Y - Ray.Origin.Y) * invRayDir;
	float tMinY = fmin(tY1, tY2);
	float tMaxY = fmax(tY1, tY2);

	invRayDir = 1.0f / Ray.Direction.Z;
	float tZ1 = (BoxMin.Z - Ray.Origin.Z) * invRayDir;
	float tZ2 = (BoxMax.Z - Ray.Origin.Z) * invRayDir;
	float tMinZ = fmin(tZ1, tZ2);
	float tMaxZ = fmax(tZ1, tZ2);

	float tEnter = fmax(fmax(tMinX, tMinY), tMinZ); // min 중에 가장 큰 값 (진입점)
	float tExit = fmin(fmin(tMaxX, tMaxY), tMaxZ);  // max 중에 가장 작은 값 (이탈점)

	if (tEnter > tExit)
	{ // 충돌 안함
		return false;
	}

	if (tExit < 0.0f)
	{ // 박스가 Ray 뒤에 있을 경우
		return false;
	}

	// 광선이 내부라면 tEnter는 음수.
	OutT = fmax(0.0f, tEnter);
	return true;
}

bool RayIntersectsBoundingSphere(const FRay& Ray, const FVector& SphereCenter, const float SphereRadius, float& OutT)
{
    if (SphereRadius < 0.0f) return false;

    // 임시 FVector 생성/정규화 없이 성분으로 계산한다.
    const float OX = Ray.Origin.X - SphereCenter.X;
    const float OY = Ray.Origin.Y - SphereCenter.Y;
    const float OZ = Ray.Origin.Z - SphereCenter.Z;
    const float RadiusSquared = SphereRadius * SphereRadius;
    const float C = OX * OX + OY * OY + OZ * OZ - RadiusSquared;
    if (C <= 0.0f)
    {
        OutT = 0.0f;
        return true;
    }

    const float DX = Ray.Direction.X;
    const float DY = Ray.Direction.Y;
    const float DZ = Ray.Direction.Z;
    const float B = OX * DX + OY * DY + OZ * DZ;
    // 구 밖에서 멀어지는 Ray(방향 0 포함)는 제곱근/나눗셈 전에 탈락한다.
    if (B >= 0.0f) return false;

    const float A = DX * DX + DY * DY + DZ * DZ;
    if (A <= 0.0f) return false;

    // B*B - A*C와 동치. 먼 작은 구에서 큰 두 수를 빼는 정밀도 손실을 줄인다.
    const float CrossX = OY * DZ - OZ * DY;
    const float CrossY = OZ * DX - OX * DZ;
    const float CrossZ = OX * DY - OY * DX;
    const float Discriminant = A * RadiusSquared -
        (CrossX * CrossX + CrossY * CrossY + CrossZ * CrossZ);
    if (Discriminant < 0.0f) return false;

    // (-B - sqrt(D)) / A를 유리화: 표면 가까이에서 뺄셈 오차를 줄인다.
    // 외부의 실제 교차에서만 sqrt 1회, 나눗셈 1회를 수행한다. 로컬 Ray의 t도 보존된다.
    OutT = C / (-B + sqrtf(Discriminant));
    return true;
}

bool RayIntersectsBoundingSphere(const FTraceContext& Context, const FVector& SphereCenter, const float SphereRadius, float& OutTEnter)
{
    return RayIntersectsBoundingSphere(Context.Ray, SphereCenter, SphereRadius, OutTEnter);
}

namespace
{
// 방향 성분이 0이면 부호를 유지한 아주 작은 값으로 바꾼 뒤 역수를 구한다.
// (직교 뷰처럼 축과 나란한 레이에서 무한대·NaN이 생기는 것을 막는다)
float SafeReciprocal(float Value)
{
	constexpr float MinMagnitude = 1e-20f;
	if (fabsf(Value) < MinMagnitude)
	{
		Value = (Value < 0.0f) ? -MinMagnitude : MinMagnitude;
	}
	return 1.0f / Value;
}
} // namespace

FTraceContext MakeTraceContext(const FRay& WorldRay, FBillboardTraceFn ResolveBillboard, const void* ViewContext)
{
	FTraceContext Context;
	Context.Ray = WorldRay;
	Context.InvDir = FVector(SafeReciprocal(WorldRay.Direction.X), SafeReciprocal(WorldRay.Direction.Y), SafeReciprocal(WorldRay.Direction.Z));
	Context.ResolveBillboard = ResolveBillboard;
	Context.ViewContext = ViewContext;
	return Context;
}

// 기존 RayIntersectsAABB와 같은 slab 판정이지만, 역수를 컨텍스트에서 받아 나눗셈이 없다.
bool RayIntersectsAABB(const FTraceContext& Context, const FVector& BoxMin, const FVector& BoxMax, float& OutTEnter)
{
	const FVector& O = Context.Ray.Origin;
	const FVector& I = Context.InvDir;

	const float tX1 = (BoxMin.X - O.X) * I.X;
	const float tX2 = (BoxMax.X - O.X) * I.X;
	const float tY1 = (BoxMin.Y - O.Y) * I.Y;
	const float tY2 = (BoxMax.Y - O.Y) * I.Y;
	const float tZ1 = (BoxMin.Z - O.Z) * I.Z;
	const float tZ2 = (BoxMax.Z - O.Z) * I.Z;

	// 역수는 SafeReciprocal로 항상 유한해 NaN이 생기지 않는다. fminf/fmaxf는 NaN 규칙 때문에 함수 호출로 컴파일되므로 std::min/max(CPU 명령 1개)를 쓴다
	const float tEnter = std::max(std::max(std::min(tX1, tX2), std::min(tY1, tY2)), std::min(tZ1, tZ2)); // 진입점
	const float tExit = std::min(std::min(std::max(tX1, tX2), std::max(tY1, tY2)), std::max(tZ1, tZ2));  // 이탈점

	if (tEnter > tExit || tExit < 0.0f)
	{ // 빗나감, 또는 박스가 레이 뒤에 있음
		return false;
	}

	OutTEnter = std::max(tEnter, 0.0f); // 레이 시작점이 박스 안이면 0
	return true;
}

uint32 RayIntersectsNode4(const FTraceContext& Context, const FPickingBVHNode4& Node, float OutTEnter[4], uint32 OutSlots[4])
{
	const __m128 OX = _mm_set1_ps(Context.Ray.Origin.X);
	const __m128 OY = _mm_set1_ps(Context.Ray.Origin.Y);
	const __m128 OZ = _mm_set1_ps(Context.Ray.Origin.Z);
	const __m128 IX = _mm_set1_ps(Context.InvDir.X);
	const __m128 IY = _mm_set1_ps(Context.InvDir.Y);
	const __m128 IZ = _mm_set1_ps(Context.InvDir.Z);

	const __m128 tX1 = _mm_mul_ps(_mm_sub_ps(_mm_load_ps(Node.MinX), OX), IX);
	const __m128 tX2 = _mm_mul_ps(_mm_sub_ps(_mm_load_ps(Node.MaxX), OX), IX);
	const __m128 tY1 = _mm_mul_ps(_mm_sub_ps(_mm_load_ps(Node.MinY), OY), IY);
	const __m128 tY2 = _mm_mul_ps(_mm_sub_ps(_mm_load_ps(Node.MaxY), OY), IY);
	const __m128 tZ1 = _mm_mul_ps(_mm_sub_ps(_mm_load_ps(Node.MinZ), OZ), IZ);
	const __m128 tZ2 = _mm_mul_ps(_mm_sub_ps(_mm_load_ps(Node.MaxZ), OZ), IZ);

	const __m128 tEnter = _mm_max_ps(_mm_max_ps(_mm_min_ps(tX1, tX2), _mm_min_ps(tY1, tY2)), _mm_min_ps(tZ1, tZ2)); // 진입점
	const __m128 tExit = _mm_min_ps(_mm_min_ps(_mm_max_ps(tX1, tX2), _mm_max_ps(tY1, tY2)), _mm_max_ps(tZ1, tZ2));  // 이탈점

	// RayIntersectsAABB의 빗나감 조건(tEnter > tExit || tExit < 0)을 뒤집은 것
	const uint32 HitMask = static_cast<uint32>(_mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(tEnter, tExit), _mm_cmpge_ps(tExit, _mm_setzero_ps()))));
	_mm_storeu_ps(OutTEnter, _mm_max_ps(tEnter, _mm_setzero_ps())); // 레이 시작점이 박스 안이면 0

	// 통과한 칸만 진입 거리 오름차순으로 끼워 넣는다 (최대 4개라 삽입 정렬)
	uint32 HitCount = 0;
	for (uint32 Slot = 0; Slot < 4; ++Slot)
	{
		if ((HitMask & (1u << Slot)) == 0 || Node.Count[Slot] == FPickingBVHNode4::EmptySlot)
			continue;
		uint32 Insert = HitCount++;
		for (; Insert > 0 && OutTEnter[OutSlots[Insert - 1]] > OutTEnter[Slot]; --Insert)
			OutSlots[Insert] = OutSlots[Insert - 1];
		OutSlots[Insert] = Slot;
	}
	return HitCount;
}

bool RayIntersectsTriangle(const FRay& Ray, const FVector& v1, const FVector& v2, const FVector& v3, float& OutT)
{
	constexpr float epsilon = 1e-5f;
	// 평면 정의
	FVector edge1 = v2 - v1;
	FVector edge2 = v3 - v1;

	FVector RayVector = Ray.Direction;

	const FVector rayCrossVec = FVector::Cross(RayVector, edge2);
	float det = FVector::Dot(rayCrossVec, edge1);
	if (det < epsilon)
	{ // 내적의 결과가 0에 가까우면 180도. 평행한 관계
		return false;
	}

	// det > 0이므로 u, v, t의 비교를 det 곱으로 옮겨 나눗셈을 통과한 삼각형에서만 한다.
	// 수식: Ray.Origin - v1 = u * edge1 + v * edge2 - t * Ray.Direction
	const float epsDet = epsilon * det;
	const float upperDet = det * (1.0f + epsilon);

	// 1. u 구하기 (u = uNum / det)
	FVector s = Ray.Origin - v1;
	const float uNum = FVector::Dot(s, rayCrossVec);

	if (uNum < -epsDet || uNum > upperDet)
	{
		return false;
	}

	FVector sCrossE1 = FVector::Cross(s, edge1);
	const float vNum = FVector::Dot(RayVector, sCrossE1);

	if (vNum < -epsDet || uNum + vNum > upperDet)
	{
		return false;
	}

	const float tNum = FVector::Dot(edge2, sCrossE1);

	if (tNum > epsDet)
	{
		OutT = tNum / det;
		return true;
	}

	return false;
}

// Mesh AABB를 통과한 Ray에 삼각형 교차를 적용해 가장 가까운 거리만 반환한다.
bool RayIntersectsMesh(const FRay& LocalRay, const FStaticMeshData& Mesh, float& OutT, const float MaxT)
{
	PrepareMeshPickingBVH(Mesh);
	bool bHit = false;
	float NearestT = MaxT;

	if (!Mesh.PickingBVHNodes4.IsEmpty())
	{
		// 로컬 레이의 역수는 메시당 한 번만 구해 모든 노드가 공유한다. 루트는 자식 박스 4개 검사가 루트 박스 검사를 대신한다.
		const FTraceContext Context = MakeTraceContext(LocalRay, nullptr, nullptr);
		TracePickingBVHNode(Context, Mesh, 0, 0.0f, NearestT, bHit);
	}
	else
	{
		const uint32 TriangleCount = static_cast<uint32>(Mesh.Indices.Num() / 3);
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
			TraceTriangle(LocalRay, Mesh, TriangleIndex, NearestT, bHit);
	}

	if (bHit)
		OutT = NearestT;
	return bHit;
}

FVector2 WorldToScreen(const FVector& WorldPos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
	FVector4 clip = FVector4(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f) * ViewProj;

	if (clip.W < 0.0001f)
		return FVector2(-FLT_MAX, -FLT_MAX);

	float ndcX = clip.X / clip.W;
	float ndcY = clip.Y / clip.W;

	FVector2 result;
	result.X = (ndcX * 0.5f + 0.5f) * ScreenW;
	result.Y = (1.0f - (ndcY * 0.5f + 0.5f)) * ScreenH; // Y 뒤집기
	return result;
}

float DistanceToSegment(const FVector2& P, const FVector2& A, const FVector2& B)
{
	FVector2 seg = B - A;
	float segLenSq = seg.X * seg.X + seg.Y * seg.Y;

	if (segLenSq < 1e-6f)
	{
		FVector2 d = P - A;
		return sqrtf(d.X * d.X + d.Y * d.Y);
	}

	FVector2 toP = P - A;
	float t = (toP.X * seg.X + toP.Y * seg.Y) / segLenSq;

	t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);

	FVector2 closest = A + seg * t;
	FVector2 diff = P - closest;
	return sqrtf(diff.X * diff.X + diff.Y * diff.Y);
}

bool RayIntersectsPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, float& OutT)
{
	float denom = Ray.Direction.Dot(PlaneNormal);

	if (fabsf(denom) < 1e-6f)
		return false;

	OutT = (PlanePoint - Ray.Origin).Dot(PlaneNormal) / denom;

	return OutT >= 0.0f;
}
