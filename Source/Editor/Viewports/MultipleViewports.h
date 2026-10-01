// 다중 뷰포트의 레이아웃·카메라·가시성 계산을 제공한다.
#pragma once

#include "Core/Types.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"
#include "Collision/Ray.h"

// 좌표 규약: +X Forward, +Y Right, +Z Up.
using ObjectId = uint32;
inline constexpr ObjectId InvalidObjectId = 0;
inline constexpr int32 InvalidViewIndex = -1;

// 화면상의 2차원 좌표나 크기를 담는다.
// 위 엔진 헤더의 동일 값 타입을 직접 사용한다(중복 정의 없음).
// 월드의 3차원 위치나 방향을 담는다.
// 위 엔진 헤더의 동일 값 타입을 직접 사용한다(중복 정의 없음).
// 3차원 회전을 나타내는 단위 quaternion 성분을 담는다.
// 위 엔진 헤더의 동일 값 타입을 직접 사용한다(중복 정의 없음).
// 행렬은 엔진 FMatrix의 row-vector 규약을 사용하며 이동은 M[3][0..2]에 둔다.
// 평면의 법선과 원점으로부터의 부호 있는 거리를 담는다.
struct FPlane { FVector Normal; float Distance; };
// 화면상의 좌상단 위치와 너비·높이를 담는다.
struct FRect { float X, Y, Width, Height; };

enum class EProjectionMode { Perspective, Orthographic };
enum class ELayoutMode { Single, QuadSplit };

// 카메라의 월드 위치와 회전을 담는다.
struct FCameraTransform
{
    FVector Location;
    FQuat Rotation{0.0f, 0.0f, 0.0f, 0.0f};
};

// 원근·직교 투영에 필요한 모드와 절두체 값을 담는다.
struct FCameraProjection
{
    EProjectionMode Mode;
    float FovDegrees;
    float OrthoWidth;
    float NearClip;
    float FarClip;
};

// 한 View에서 사용하는 카메라 Transform과 투영 설정을 담는다.
struct FViewCamera
{
    FCameraTransform Transform;
    FCameraProjection Projection;
};

// 레이아웃 모드와 최대 네 View의 카메라 상태를 담는다.
struct FViewSet
{
    ELayoutMode Mode;
    FViewCamera Cameras[4];
};

// 월드 공간 광선의 시작점과 정규화 방향을 담는다.
// 위 엔진 헤더의 동일 값 타입을 직접 사용한다(중복 정의 없음).

// 한 프레임의 카메라 회전·이동·줌 입력을 담는다.
struct FCameraMoveInput
{
    FVector2 MouseDelta;
    FVector MoveAxis;
    float ZoomDelta;
};

// 카메라의 월드 축을 직교기저로 변환해 View 행렬을 만든다.
FMatrix BuildViewMatrix(const FCameraTransform& Transform);
// 투영 모드에 따라 원근 또는 직교 Projection 행렬을 만든다.
FMatrix BuildProjectionMatrix(const FCameraProjection& Projection, float AspectRatio);
// 화면 좌표를 카메라 기저와 투영값으로 역투영해 월드 Ray를 만든다.
FRay Deproject(const FViewCamera& Camera, FVector2 ScreenPos, FVector2 ViewportSize);
// 로컬 이동과 Yaw·Pitch 입력을 현재 카메라에 적분한다.
FViewCamera ApplyCameraMovement(const FViewCamera& Current, const FCameraMoveInput& Input, float DeltaTime);

// Rect의 너비와 높이가 모두 양수인지 검사한다.
bool IsViewRectValid(const FRect& Rect);
// 레이아웃 모드와 Rect 유효성으로 해당 View의 활성 여부를 판정한다.
bool IsViewActive(const FViewSet& Views, int32 ViewIndex, const FRect ViewRects[4]);

// 현재 Hover View와 우클릭으로 Capture한 View 인덱스를 담는다.
struct FViewInputState
{
    int32 HoveredViewIndex = InvalidViewIndex;
    int32 CapturedViewIndex = InvalidViewIndex;
};

// 화면 좌표를 포함하는 첫 번째 유효 View Rect를 찾는다.
int32 DetermineHoveredView(FVector2 ScreenPos, const FRect ViewRects[4]);
// Capture가 있으면 고정 View를, 없으면 Hover View를 활성 View로 고른다.
int32 DetermineActiveView(const FViewInputState& State, FVector2 ScreenPos, const FRect ViewRects[4]);
// 지정한 View를 입력 Capture 대상으로 설정한 새 상태를 반환한다.
FViewInputState BeginCapture(const FViewInputState& Current, int32 ViewIndex);
// 입력 Capture를 해제한 새 상태를 반환한다.
FViewInputState EndCapture(const FViewInputState& Current);

// 가로·세로 Splitter가 차지하는 창의 비율을 담는다.
struct FSplitRatio
{
    float Horizontal = 0.5f;
    float Vertical = 0.5f;
};

enum class EDragAxis { Horizontal, Vertical };

// 픽셀 Drag를 창 크기로 정규화해 해당 Split 비율에 반영한다.
FSplitRatio ApplySplitterDrag(const FSplitRatio& Current, EDragAxis Axis, float DeltaPixels, FVector2 WindowSize, float MinRatio);
// 두 Split 비율을 허용된 최소·최대 범위로 제한한다.
FSplitRatio ClampSplitRatio(const FSplitRatio& Raw, float MinRatio);
// Split 비율과 창 크기로 2x2 View Rect 네 개를 계산한다.
void ComputeViewRects(const FSplitRatio& Ratio, FVector2 WindowSize, FRect OutRects[4]);

// 축 정렬 Bounding Box의 중심과 반크기를 담는다.
struct FAABB { FVector Center; FVector Extent; };

class UPrimitiveComponent;
struct FStaticMeshData;

// 렌더 대상의 컴포넌트 포인터 월드 행렬 월드 경계를 담는다
struct FRenderableObject
{
    UPrimitiveComponent* Primitive = nullptr;
    FMatrix WorldMatrix{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    FAABB WorldBounds;
    const FStaticMeshData* StaticMeshData = nullptr;
    uint64 BoundsRevision = 0;
    uint32 StableIndex = 0;
    bool bCanBeOccluded = false; //가려질 수 있는가?
    bool bCanOcclude = false; //남을 가릴 수 있는가?
};

// View 절두체를 이루는 여섯 개의 정규화 평면과 SIMD 판정용 SoA 캐시를 담는다.
struct FFrustumPlanes
{
    FPlane Planes[6];
    alignas(16) float NormalX[8]{};
    alignas(16) float NormalY[8]{};
    alignas(16) float NormalZ[8]{};
    alignas(16) float Distance[8]{};
};

enum class EFrustumContainment : uint8 { Outside, Intersect, Inside };

// ViewProjection 행렬의 행 조합으로 여섯 절두체 평면을 추출한다.
FFrustumPlanes ExtractFrustumPlanes(const FMatrix& ViewProjection);
// 네 평면씩 SIMD로 검사해 AABB가 절두체의 밖·경계·안 중 어디에 있는지 분류한다.
EFrustumContainment ClassifyAABBInFrustum(const FAABB& Bounds, const FFrustumPlanes& Frustum);
// AABB의 projected radius를 이용해 절두체 포함 여부를 검사한다.
bool IsAABBInFrustum(const FAABB& Bounds, const FFrustumPlanes& Frustum);

// 삼각형의 월드 공간 세 꼭짓점을 담는다.
struct FTriangle { FVector V0, V1, V2; };

// 피킹 후보의 식별자와 Broad Phase용 월드 AABB를 담는다.
struct FPickableObject
{
    ObjectId Id;
    FAABB WorldBounds;
};

// 가장 가까운 피킹 결과의 성공 여부·대상·거리·교차점을 담는다.
struct FPickHit
{
    bool bHit = false;
    ObjectId Id = InvalidObjectId;
    float Distance = 0.0f;
    FVector HitPoint;
};

// Ray와 AABB의 slab 교차로 Narrow Phase 후보 ID를 출력한다.
void FindPickCandidates(const FRay& WorldRay, const TArray<FPickableObject>& Objects, TArray<ObjectId>& OutCandidates);
// 후보 삼각형에 Möller–Trumbore 교차를 적용해 가장 가까운 Hit를 찾는다.
FPickHit PickNarrowPhase(const FRay& WorldRay, const TArray<ObjectId>& Candidates, const TMap<ObjectId, TArray<FTriangle>>& TrianglesById);
// AABB Broad Phase 뒤 삼각형 Narrow Phase를 수행해 가장 가까운 대상을 고른다.
FPickHit Pick(const FRay& WorldRay, const TArray<FPickableObject>& Objects, const TMap<ObjectId, TArray<FTriangle>>& TrianglesById);

// Billboard의 월드 위치와 화면에 보일 너비·높이를 담는다.
struct FBillboardComputeInput
{
    FVector WorldPosition;
    FVector2 Size;
};

// 카메라를 향하도록 계산된 Billboard 월드 행렬을 담는다.
struct FBillboardTransform
{
    FMatrix WorldMatrix{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
};

// 파티클 식별자와 정렬 기준이 되는 월드 위치를 담는다.
struct FParticleSortInput
{
    ObjectId Id;
    FVector WorldPosition;
};

// 카메라 기저로 Billboard의 Right·Up·Forward 축을 만들어 월드 행렬을 계산한다.
FBillboardTransform ComputeBillboardTransform(const FBillboardComputeInput& Input, const FCameraTransform& ViewCamera);
// 카메라와의 거리 제곱을 기준으로 파티클 ID를 먼 순서부터 안정 정렬한다.
void SortParticlesByCameraDistance(const TArray<FParticleSortInput>& Particles, const FVector& CameraLocation, TArray<ObjectId>& OutSortedBackToFront);
