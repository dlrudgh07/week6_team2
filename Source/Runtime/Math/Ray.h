#pragma once

#include "Math/EngineMath.h"

#include <cfloat>

struct FStaticMeshData;
struct FPickingBVHNode4;
class UBillboardComponent;
class UPrimitiveComponent;

struct FRay
{
	// 광선 시작위치
	FVector Origin;
	// 광선 방향
	FVector Direction;

	FVector PointAt(float t) const
	{
		return Origin + Direction * t;
	}
};

// Broad phase에서 구한 객체 AABB 진입 거리를 정밀 피킹까지 전달한다.
// Billboard처럼 일반 Bounds를 신뢰할 수 없는 후보는 거리 가지치기에서 제외한다.
struct FLineTraceCandidate
{
	UPrimitiveComponent* Primitive = nullptr;
	float BoundsDistance = 0.0f;
	bool bHasBoundsDistance = false;
};

// 월드 레이를 로컬로. 방향은 정규화하지 않는다 (t가 월드 거리로 유지되도록)
FRay ToLocalRay(const FRay& WorldRay, const FMatrix& WorldMatrix);
// 아핀 행렬(이동·회전·스케일) 전용. 4x4 역행렬 없이 외적 3개로 로컬 레이를 구한다. 행렬이 퇴화(det≈0)면 false.
bool ToLocalRayAffine(const FRay& WorldRay, const FMatrix& WorldMatrix, FRay& OutLocalRay);

bool RayIntersectsAABB(const FRay& Ray, const FVector& BoxMin, const FVector& BoxMax, float& OutT);

// Ray와 같은 공간의 중심/반지름을 받는다. 방향은 정규화하지 않아도 된다.
// 내부/표면에서 시작하면 OutT = 0, 외부에서는 최초 진입 t를 반환한다.
// 실패 시 OutT는 유지한다. Radius는 0 이상이며, 방향이 0이면 점 포함 여부를 판정한다.
bool RayIntersectsBoundingSphere(const FRay& Ray, const FVector& SphereCenter, float SphereRadius, float& OutT);

	// View별 Billboard 행렬 공급자 (UWorld::FBillboardTraceTransform과 같은 형식)
using FBillboardTraceFn = FMatrix (*)(const UBillboardComponent&, const void*);

// 피킹 한 번(클릭 한 번) 동안 모든 후보가 공유하는 값을 한 곳에 모은다.
// 레이 역수는 클릭당 한 번만 구하고, 최근접 거리는 루프가 갱신한다.
struct FTraceContext
{
	FRay Ray;                                     // 월드 레이
	FVector InvDir;                               // 1/D (0 성분은 부호를 유지한 아주 작은 값으로 바꾼 뒤 역수)
	float BestDistance = FLT_MAX;                 // 지금까지 찾은 최근접 교차 거리
	FBillboardTraceFn ResolveBillboard = nullptr; // 있으면 Billboard는 클릭한 View의 행렬로 판정
	const void* ViewContext = nullptr;
};

FTraceContext MakeTraceContext(const FRay& WorldRay, FBillboardTraceFn ResolveBillboard, const void* ViewContext);

// 월드 AABB 판정 (컨텍스트의 역수 사용, 나눗셈 없음). 맞으면 진입 거리(0 이상)를 돌려준다.
bool RayIntersectsAABB(const FTraceContext& Context, const FVector& BoxMin, const FVector& BoxMax, float& OutTEnter);

// 4갈래 노드의 자식 박스 4개를 SSE로 한 번에 검사한다. RayIntersectsAABB와 계산 순서가 같아 진입 거리도 같다.
// 레이가 통과한 칸을 진입 거리 오름차순으로 OutSlots에 담고 그 개수를 돌려준다. OutTEnter는 칸 번호로 읽는다.
uint32 RayIntersectsNode4(const FTraceContext& Context, const FPickingBVHNode4& Node, float OutTEnter[4], uint32 OutSlots[4]);

bool RayIntersectsBoundingSphere(const FTraceContext& Context, const FVector& SphereCenter, float SphereRadius, float& OutTEnter);

bool RayIntersectsTriangle(const FRay& Ray, const FVector& v1, const FVector& v2, const FVector& v3, float& OutT);

// StaticMesh 로드 시 피킹용 Triangle BVH를 미리 구축해 첫 클릭 비용을 제거한다.
void PrepareMeshPickingBVH(const FStaticMeshData& Mesh);

// MaxT보다 가까운 교차만 찾는다. 로컬 레이 방향이 비정규화라 t는 월드 거리와 같으므로 전역 최근접 거리를 넘길 수 있다.
bool RayIntersectsMesh(const FRay& LocalRay, const FStaticMeshData& Mesh, float& OutT, float MaxT = FLT_MAX);

FVector2 WorldToScreen(const FVector& WorldPos, const FMatrix& ViewProj, int ScreenW, int ScreenH);

float DistanceToSegment(const FVector2& P, const FVector2& A, const FVector2& B);

bool RayIntersectsPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, float& OutT);
