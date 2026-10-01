// 다중 뷰포트의 레이아웃·카메라·가시성 계산을 제공한다.
#include "EnginePCH.h"
#include "Editor/Viewports/MultipleViewports.h"
#include "Component/PrimitiveComponent.h"

#include <algorithm>
#include <cassert>
#include <math.h>
#include <float.h>
#include "Math/EngineMath.h"
#include "Math/VectorRegister.h"

#include <vector>



// float 무한대 대응은 유한 최댓값으로 대체하지 않는다.
static_assert(std::numeric_limits<float>::has_infinity, "Float type must support infinity");

// MultipleViewportsMax의 비교 순서와 동률 선택을 보존하는 float 전용 보조 함수다.
static float MultipleViewportsMax(float A, float B) { return A < B ? B : A; }
// MultipleViewportsMin의 비교 순서와 동률 선택을 보존하는 float 전용 보조 함수다.
static float MultipleViewportsMin(float A, float B) { return B < A ? B : A; }
// 두 float을 임시 값 하나로 교환한다.
static void MultipleViewportsSwap(float& A, float& B) { const float Temp = A; A = B; B = Temp; }
static constexpr float Pi = 3.14159265358979323846f;
static constexpr float Epsilon = 1.0e-6f;

// 두 벡터의 각 성분을 더한다.
static FVector Add(const FVector A, const FVector B) { return {A.X + B.X, A.Y + B.Y, A.Z + B.Z}; }
// 두 벡터의 각 성분을 뺀다.
static FVector Subtract(const FVector A, const FVector B) { return {A.X - B.X, A.Y - B.Y, A.Z - B.Z}; }
// 벡터의 각 성분에 스칼라를 곱한다.
static FVector Scale(const FVector V, const float S) { return {V.X * S, V.Y * S, V.Z * S}; }
// 두 벡터의 내적을 계산한다.
static float Dot(const FVector A, const FVector B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
// 오른손 좌표계의 벡터 외적을 계산한다.
static FVector Cross(const FVector A, const FVector B)
{
    return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X};
}
// 제곱근 없이 벡터 길이의 제곱을 계산한다.
static float LengthSquared(const FVector V) { return Dot(V, V); }
// 0이 아닌 벡터를 길이 1로 정규화한다.
static FVector Normalize(const FVector V)
{
    const float Length = sqrtf(LengthSquared(V));
    assert(Length > Epsilon);
    return Scale(V, 1.0f / Length);
}

// 0이 아닌 quaternion을 길이 1로 정규화한다.
static FQuat Normalize(const FQuat Q)
{
    const float Length = sqrtf(Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W);
    assert(Length > Epsilon);
    return {Q.X / Length, Q.Y / Length, Q.Z / Length, Q.W / Length};
}

// Hamilton product로 두 quaternion 회전을 합성한다.
static FQuat Multiply(const FQuat A, const FQuat B)
{
    return {
        A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y,
        A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X,
        A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W,
        A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z};
}

// 단위 축과 각도로 회전 quaternion을 만든다.
static FQuat AxisAngle(const FVector Axis, const float Radians)
{
    const float Half = Radians * 0.5f;
    const float Sine = sinf(Half);
    const FVector UnitAxis = Normalize(Axis);
    return {UnitAxis.X * Sine, UnitAxis.Y * Sine, UnitAxis.Z * Sine, cosf(Half)};
}

// 정규화 quaternion의 벡터 회전 공식을 사용해 방향을 회전한다.
static FVector Rotate(const FQuat Rotation, const FVector V)
{
    const FQuat Q = Normalize(Rotation);
    const FVector U{Q.X, Q.Y, Q.Z};
    return Add(Add(Scale(U, 2.0f * Dot(U, V)), Scale(V, Q.W * Q.W - Dot(U, U))), Scale(Cross(U, V), 2.0f * Q.W));
}

// 모든 원소가 0인 4x4 행렬을 만든다.
static FMatrix ZeroMatrix()
{
    FMatrix Result;
    for (int Row = 0; Row < 4; ++Row)
        for (int Column = 0; Column < 4; ++Column)
            Result.M[Row][Column] = 0.0f;
    return Result;
}

// 평면 방정식을 법선 길이 1이 되도록 정규화한다.
static FPlane NormalizePlane(const FPlane Plane)
{
    const float Length = sqrtf(LengthSquared(Plane.Normal));
    assert(Length > Epsilon);
    return {Scale(Plane.Normal, 1.0f / Length), Plane.Distance / Length};
}

// 세 축의 slab 구간을 교차해 Ray와 AABB의 충돌을 검사한다.
static bool MultipleViewportsRayIntersectsAABB(const FRay& Ray, const FAABB& Bounds)
{
    const float Origins[3] = {Ray.Origin.X, Ray.Origin.Y, Ray.Origin.Z};
    const float Directions[3] = {Ray.Direction.X, Ray.Direction.Y, Ray.Direction.Z};
    const float Centers[3] = {Bounds.Center.X, Bounds.Center.Y, Bounds.Center.Z};
    const float Extents[3] = {Bounds.Extent.X, Bounds.Extent.Y, Bounds.Extent.Z};
    float Minimum = 0.0f;
    float Maximum = HUGE_VALF;

    for (int Axis = 0; Axis < 3; ++Axis)
    {
        const float Low = Centers[Axis] - Extents[Axis];
        const float High = Centers[Axis] + Extents[Axis];
        if (fabsf(Directions[Axis]) <= Epsilon)
        {
            if (Origins[Axis] < Low || Origins[Axis] > High)
            {
                return false;
            }
            continue;
        }

        float Near = (Low - Origins[Axis]) / Directions[Axis];
        float Far = (High - Origins[Axis]) / Directions[Axis];
        if (Near > Far)
        {
            MultipleViewportsSwap(Near, Far);
        }
        Minimum = MultipleViewportsMax(Minimum, Near);
        Maximum = MultipleViewportsMin(Maximum, Far);
        if (Minimum > Maximum)
        {
            return false;
        }
    }
    return Maximum >= 0.0f;
}

// Möller–Trumbore 알고리즘으로 Ray와 삼각형의 교차 거리를 구한다.
static bool MultipleViewportsRayIntersectsTriangle(const FRay& Ray, const FTriangle& Triangle, float& OutDistance)
{
    const FVector Edge1 = Subtract(Triangle.V1, Triangle.V0);
    const FVector Edge2 = Subtract(Triangle.V2, Triangle.V0);
    const FVector P = Cross(Ray.Direction, Edge2);
    const float Determinant = Dot(Edge1, P);
    if (fabsf(Determinant) <= Epsilon)
    {
        return false;
    }

    const float InverseDeterminant = 1.0f / Determinant;
    const FVector T = Subtract(Ray.Origin, Triangle.V0);
    const float U = Dot(T, P) * InverseDeterminant;
    if (U < 0.0f || U > 1.0f)
    {
        return false;
    }

    const FVector Q = Cross(T, Edge1);
    const float V = Dot(Ray.Direction, Q) * InverseDeterminant;
    if (V < 0.0f || U + V > 1.0f)
    {
        return false;
    }

    const float Distance = Dot(Edge2, Q) * InverseDeterminant;
    if (Distance < 0.0f)
    {
        return false;
    }
    OutDistance = Distance;
    return true;
}


// 회전된 카메라 기저와 위치 내적으로 View 행렬을 구성한다.
FMatrix BuildViewMatrix(const FCameraTransform& Transform)
{
    const FVector Forward = Rotate(Transform.Rotation, {1.0f, 0.0f, 0.0f});
    const FVector Right = Rotate(Transform.Rotation, {0.0f, 1.0f, 0.0f});
    const FVector Up = Rotate(Transform.Rotation, {0.0f, 0.0f, 1.0f});
    FMatrix Result = ZeroMatrix();
    Result.M[0][0] = Forward.X; Result.M[1][0] = Forward.Y; Result.M[2][0] = Forward.Z; Result.M[3][0] = -Dot(Forward, Transform.Location);
    Result.M[0][1] = Right.X; Result.M[1][1] = Right.Y; Result.M[2][1] = Right.Z; Result.M[3][1] = -Dot(Right, Transform.Location);
    Result.M[0][2] = Up.X; Result.M[1][2] = Up.Y; Result.M[2][2] = Up.Z; Result.M[3][2] = -Dot(Up, Transform.Location);
    Result.M[3][3] = 1.0f;
    return Result;
}

// 원근은 FOV, 직교는 전체 폭을 기준으로 Projection 행렬을 구성한다.
FMatrix BuildProjectionMatrix(const FCameraProjection& Projection, const float AspectRatio)
{
    assert(AspectRatio > 0.0f);
    assert(Projection.NearClip > 0.0f && Projection.FarClip > Projection.NearClip);
    FMatrix Result = ZeroMatrix();
    if (Projection.Mode == EProjectionMode::Perspective)
    {
        assert(Projection.FovDegrees > 0.0f && Projection.FovDegrees < 180.0f);
        const float ScaleY = 1.0f / tanf(Projection.FovDegrees * Pi / 360.0f);
        Result.M[1][0] = ScaleY / AspectRatio;
        Result.M[2][1] = ScaleY;
        Result.M[0][2] = Projection.FarClip / (Projection.FarClip - Projection.NearClip);
        Result.M[3][2] = -Projection.NearClip * Projection.FarClip / (Projection.FarClip - Projection.NearClip);
        Result.M[0][3] = 1.0f;
    }
    else
    {
        assert(Projection.OrthoWidth > 0.0f);
        const float Height = Projection.OrthoWidth / AspectRatio;
        Result.M[1][0] = 2.0f / Projection.OrthoWidth;
        Result.M[2][1] = 2.0f / Height;
        Result.M[0][2] = 1.0f / (Projection.FarClip - Projection.NearClip);
        Result.M[3][2] = -Projection.NearClip / (Projection.FarClip - Projection.NearClip);
        Result.M[3][3] = 1.0f;
    }
    return Result;
}

// 화면 좌표를 NDC로 바꾸고 카메라 기저에 결합해 월드 Ray를 만든다.
FRay Deproject(const FViewCamera& Camera, const FVector2 ScreenPos, const FVector2 ViewportSize)
{
    assert(ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f);
    const float NdcX = 2.0f * ScreenPos.X / ViewportSize.X - 1.0f;
    const float NdcY = 1.0f - 2.0f * ScreenPos.Y / ViewportSize.Y;
    const float AspectRatio = ViewportSize.X / ViewportSize.Y;
    const FVector Forward = Rotate(Camera.Transform.Rotation, {1.0f, 0.0f, 0.0f});
    const FVector Right = Rotate(Camera.Transform.Rotation, {0.0f, 1.0f, 0.0f});
    const FVector Up = Rotate(Camera.Transform.Rotation, {0.0f, 0.0f, 1.0f});

    if (Camera.Projection.Mode == EProjectionMode::Perspective)
    {
        const float Tangent = tanf(Camera.Projection.FovDegrees * Pi / 360.0f);
        const FVector Direction = Normalize(Add(Forward, Add(Scale(Right, NdcX * Tangent * AspectRatio), Scale(Up, NdcY * Tangent))));
        return {Camera.Transform.Location, Direction};
    }

    assert(Camera.Projection.OrthoWidth > 0.0f);
    const float Height = Camera.Projection.OrthoWidth / AspectRatio;
    const FVector Origin = Add(Add(Add(Camera.Transform.Location, Scale(Forward, Camera.Projection.NearClip)), Scale(Right, NdcX * Camera.Projection.OrthoWidth * 0.5f)), Scale(Up, NdcY * Height * 0.5f));
    return {Origin, Normalize(Forward)};
}

// 로컬 이동을 월드로 회전하고 Yaw 뒤 현재 Right축 Pitch를 합성한다.
FViewCamera ApplyCameraMovement(const FViewCamera& Current, const FCameraMoveInput& Input, const float DeltaTime)
{
    assert(DeltaTime >= 0.0f);
    FViewCamera Result = Current;
    const FVector LocalTranslation{Input.MoveAxis.X + Input.ZoomDelta, Input.MoveAxis.Y, Input.MoveAxis.Z};
    Result.Transform.Location = Add(Result.Transform.Location, Scale(Rotate(Current.Transform.Rotation, LocalTranslation), DeltaTime));

    if (Input.MouseDelta.X != 0.0f || Input.MouseDelta.Y != 0.0f)
    {
        const FQuat Yaw = AxisAngle({0.0f, 0.0f, 1.0f}, Input.MouseDelta.X * DeltaTime * Pi / 180.0f);
        const FVector CurrentRight = Rotate(Current.Transform.Rotation, {0.0f, 1.0f, 0.0f});
        const FQuat Pitch = AxisAngle(CurrentRight, -Input.MouseDelta.Y * DeltaTime * Pi / 180.0f);
        Result.Transform.Rotation = Normalize(Multiply(Pitch, Multiply(Yaw, Current.Transform.Rotation)));
    }
    return Result;
}

// Rect가 렌더 가능한 양수 크기인지 확인한다.
bool IsViewRectValid(const FRect& Rect) { return Rect.Width > 0.0f && Rect.Height > 0.0f; }

// Rect 유효성과 Single·Quad 레이아웃 규칙으로 View 활성 여부를 판정한다.
bool IsViewActive(const FViewSet& Views, const int32 ViewIndex, const FRect ViewRects[4])
{
    if (ViewIndex < 0 || ViewIndex >= 4 || !IsViewRectValid(ViewRects[ViewIndex]))
    {
        return false;
    }
    return Views.Mode == ELayoutMode::QuadSplit || ViewIndex == 0;
}

// 네 Rect를 순서대로 검사해 화면 좌표를 포함하는 View를 찾는다.
int32 DetermineHoveredView(const FVector2 ScreenPos, const FRect ViewRects[4])
{
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FRect& Rect = ViewRects[Index];
        if (IsViewRectValid(Rect) && ScreenPos.X >= Rect.X && ScreenPos.X < Rect.X + Rect.Width && ScreenPos.Y >= Rect.Y && ScreenPos.Y < Rect.Y + Rect.Height)
        {
            return Index;
        }
    }
    return InvalidViewIndex;
}

// Capture 중에는 고정 View를 유지하고 아니면 Hover View를 사용한다.
int32 DetermineActiveView(const FViewInputState& State, const FVector2 ScreenPos, const FRect ViewRects[4])
{
    if (State.CapturedViewIndex != InvalidViewIndex)
    {
        return State.CapturedViewIndex;
    }
    return DetermineHoveredView(ScreenPos, ViewRects);
}

// 기존 입력 상태를 복사한 뒤 지정 View에 Capture를 시작한다.
FViewInputState BeginCapture(const FViewInputState& Current, const int32 ViewIndex)
{
    assert(ViewIndex >= 0 && ViewIndex < 4);
    FViewInputState Result = Current;
    Result.CapturedViewIndex = ViewIndex;
    return Result;
}

// 기존 입력 상태를 복사한 뒤 Capture View를 해제한다.
FViewInputState EndCapture(const FViewInputState& Current)
{
    FViewInputState Result = Current;
    Result.CapturedViewIndex = InvalidViewIndex;
    return Result;
}

// Drag 픽셀을 창 축 길이로 나눠 Split 비율에 누적하고 clamp한다.
FSplitRatio ApplySplitterDrag(const FSplitRatio& Current, const EDragAxis Axis, const float DeltaPixels, const FVector2 WindowSize, const float MinRatio)
{
    FSplitRatio Result = Current;
    if (Axis == EDragAxis::Horizontal)
    {
        assert(WindowSize.X > 0.0f);
        Result.Horizontal += DeltaPixels / WindowSize.X;
    }
    else
    {
        assert(WindowSize.Y > 0.0f);
        Result.Vertical += DeltaPixels / WindowSize.Y;
    }
    return ClampSplitRatio(Result, MinRatio);
}

// Splitter가 창 가장자리에 붙지 않도록 두 비율을 대칭 범위로 제한한다.
FSplitRatio ClampSplitRatio(const FSplitRatio& Raw, const float MinRatio)
{
    assert(MinRatio >= 0.0f && MinRatio <= 0.5f);
    return {
        FMath::Clamp(Raw.Horizontal, MinRatio, 1.0f - MinRatio),
        FMath::Clamp(Raw.Vertical, MinRatio, 1.0f - MinRatio)};
}

// 가로·세로 Split 위치로 창을 빈틈없는 네 Rect로 나눈다.
void ComputeViewRects(const FSplitRatio& Ratio, const FVector2 WindowSize, FRect OutRects[4])
{
    assert(WindowSize.X >= 0.0f && WindowSize.Y >= 0.0f);
    assert(Ratio.Horizontal >= 0.0f && Ratio.Horizontal <= 1.0f);
    assert(Ratio.Vertical >= 0.0f && Ratio.Vertical <= 1.0f);
    const float LeftWidth = WindowSize.X * Ratio.Horizontal;
    const float TopHeight = WindowSize.Y * Ratio.Vertical;
    const float RightWidth = WindowSize.X - LeftWidth;
    const float BottomHeight = WindowSize.Y - TopHeight;
    OutRects[0] = {0.0f, 0.0f, LeftWidth, TopHeight};
    OutRects[1] = {LeftWidth, 0.0f, RightWidth, TopHeight};
    OutRects[2] = {0.0f, TopHeight, LeftWidth, BottomHeight};
    OutRects[3] = {LeftWidth, TopHeight, RightWidth, BottomHeight};
}

// row-vector ViewProjection 열 조합으로 좌·우·상·하·근·원 평면을 추출한다.
FFrustumPlanes ExtractFrustumPlanes(const FMatrix& Matrix)
{
    auto MakePlane = [&Matrix](const int ColumnA, const float ScaleA, const int ColumnB, const float ScaleB)
    {
        return NormalizePlane({
            {ScaleA * Matrix.M[0][ColumnA] + ScaleB * Matrix.M[0][ColumnB], ScaleA * Matrix.M[1][ColumnA] + ScaleB * Matrix.M[1][ColumnB], ScaleA * Matrix.M[2][ColumnA] + ScaleB * Matrix.M[2][ColumnB]},
            ScaleA * Matrix.M[3][ColumnA] + ScaleB * Matrix.M[3][ColumnB]});
    };
    FFrustumPlanes Result{};
    Result.Planes[0] = MakePlane(3, 1.0f, 0, 1.0f);
    Result.Planes[1] = MakePlane(3, 1.0f, 0, -1.0f);
    Result.Planes[2] = MakePlane(3, 1.0f, 1, 1.0f);
    Result.Planes[3] = MakePlane(3, 1.0f, 1, -1.0f);
    Result.Planes[4] = NormalizePlane({{Matrix.M[0][2], Matrix.M[1][2], Matrix.M[2][2]}, Matrix.M[3][2]});
    Result.Planes[5] = MakePlane(3, 1.0f, 2, -1.0f);
    for (int32 PlaneIndex = 0; PlaneIndex < 6; ++PlaneIndex)
    {
        const FPlane& Plane = Result.Planes[PlaneIndex];
        Result.NormalX[PlaneIndex] = Plane.Normal.X;
        Result.NormalY[PlaneIndex] = Plane.Normal.Y;
        Result.NormalZ[PlaneIndex] = Plane.Normal.Z;
        Result.Distance[PlaneIndex] = Plane.Distance;
    }
    return Result;
}

static EFrustumContainment ClassifyAABBInFrustumSIMD(
    const FAABB& Bounds,
    const FFrustumPlanes& Frustum,
    const bool bTrackIntersections)
{
    const FVectorRegister CenterX = VectorSIMD::SetVal(Bounds.Center.X);
    const FVectorRegister CenterY = VectorSIMD::SetVal(Bounds.Center.Y);
    const FVectorRegister CenterZ = VectorSIMD::SetVal(Bounds.Center.Z);
    const FVectorRegister ExtentX = VectorSIMD::SetVal(Bounds.Extent.X);
    const FVectorRegister ExtentY = VectorSIMD::SetVal(Bounds.Extent.Y);
    const FVectorRegister ExtentZ = VectorSIMD::SetVal(Bounds.Extent.Z);
    const FVectorRegister Zero = VectorSIMD::SetZero();
    bool bIntersects = false;

    for (int32 PlaneOffset = 0; PlaneOffset < 8; PlaneOffset += 4)
    {
        const FVectorRegister NormalX = VectorSIMD::LoadAligned(Frustum.NormalX + PlaneOffset);
        const FVectorRegister NormalY = VectorSIMD::LoadAligned(Frustum.NormalY + PlaneOffset);
        const FVectorRegister NormalZ = VectorSIMD::LoadAligned(Frustum.NormalZ + PlaneOffset);
        FVectorRegister SignedDistance = VectorSIMD::Mul(NormalX, CenterX);
        SignedDistance = VectorSIMD::Add(SignedDistance, VectorSIMD::Mul(NormalY, CenterY));
        SignedDistance = VectorSIMD::Add(SignedDistance, VectorSIMD::Mul(NormalZ, CenterZ));
        SignedDistance = VectorSIMD::Add(SignedDistance, VectorSIMD::LoadAligned(Frustum.Distance + PlaneOffset));

        FVectorRegister Radius = VectorSIMD::Mul(VectorSIMD::Abs(NormalX), ExtentX);
        Radius = VectorSIMD::Add(Radius, VectorSIMD::Mul(VectorSIMD::Abs(NormalY), ExtentY));
        Radius = VectorSIMD::Add(Radius, VectorSIMD::Mul(VectorSIMD::Abs(NormalZ), ExtentZ));

        const int32 ValidLaneMask = PlaneOffset == 0 ? 0xf : 0x3;
        const int32 OutsideMask = _mm_movemask_ps(_mm_cmplt_ps(VectorSIMD::Add(SignedDistance, Radius), Zero)) & ValidLaneMask;
        if (OutsideMask != 0)
            return EFrustumContainment::Outside;
        if (bTrackIntersections)
        {
            const int32 IntersectMask = _mm_movemask_ps(_mm_cmplt_ps(VectorSIMD::Sub(SignedDistance, Radius), Zero)) & ValidLaneMask;
            bIntersects = bIntersects || IntersectMask != 0;
        }
    }

    return bIntersects ? EFrustumContainment::Intersect : EFrustumContainment::Inside;
}

EFrustumContainment ClassifyAABBInFrustum(const FAABB& Bounds, const FFrustumPlanes& Frustum)
{
    return ClassifyAABBInFrustumSIMD(Bounds, Frustum, true);
}

// 각 평면에 대한 AABB projected radius로 완전한 바깥 여부를 검사한다.
bool IsAABBInFrustum(const FAABB& Bounds, const FFrustumPlanes& Frustum)
{
    return ClassifyAABBInFrustumSIMD(Bounds, Frustum, false) != EFrustumContainment::Outside;
}



// Ray와 월드 AABB가 교차하는 대상만 Narrow Phase 후보로 모은다.
void FindPickCandidates(const FRay& WorldRay, const TArray<FPickableObject>& Objects, TArray<ObjectId>& OutCandidates)
{
    OutCandidates.Reset();
    for (const FPickableObject& Object : Objects)
    {
        if (MultipleViewportsRayIntersectsAABB(WorldRay, Object.WorldBounds))
        {
            OutCandidates.Add(Object.Id);
        }
    }
}

// 후보별 모든 삼각형을 검사해 Ray에 가장 가까운 양의 교차를 선택한다.
FPickHit PickNarrowPhase(const FRay& WorldRay, const TArray<ObjectId>& Candidates, const TMap<ObjectId, TArray<FTriangle>>& TrianglesById)
{
    FPickHit Result{};
    float Nearest = HUGE_VALF;
    for (const ObjectId Id : Candidates)
    {
        const auto* Found = TrianglesById.Find(Id);
        if (Found == nullptr)
        {
            continue;
        }
        for (const FTriangle& Triangle : *Found)
        {
            float Distance = 0.0f;
            if (MultipleViewportsRayIntersectsTriangle(WorldRay, Triangle, Distance) && Distance < Nearest)
            {
                Nearest = Distance;
                Result.bHit = true;
                Result.Id = Id;
                Result.Distance = Distance;
                Result.HitPoint = Add(WorldRay.Origin, Scale(WorldRay.Direction, Distance));
            }
        }
    }
    return Result;
}

// AABB Broad Phase와 삼각형 Narrow Phase를 차례로 수행한다.
FPickHit Pick(const FRay& WorldRay, const TArray<FPickableObject>& Objects, const TMap<ObjectId, TArray<FTriangle>>& TrianglesById)
{
    TArray<ObjectId> Candidates;
    FindPickCandidates(WorldRay, Objects, Candidates);
    if (Candidates.IsEmpty())
    {
        return {};
    }
    return PickNarrowPhase(WorldRay, Candidates, TrianglesById);
}

// 카메라를 향하는 직교기저를 만들고 크기를 반영해 Billboard 행렬을 만든다.
FBillboardTransform ComputeBillboardTransform(const FBillboardComputeInput& Input, const FCameraTransform& ViewCamera)
{
    FVector Forward = Subtract(ViewCamera.Location, Input.WorldPosition);
    if (LengthSquared(Forward) <= Epsilon * Epsilon)
    {
        Forward = Rotate(ViewCamera.Rotation, {-1.0f, 0.0f, 0.0f});
    }
    Forward = Normalize(Forward);
    FVector ReferenceUp{0.0f, 0.0f, 1.0f};
    if (fabsf(Dot(Forward, ReferenceUp)) > 0.999f)
    {
        ReferenceUp = {0.0f, 1.0f, 0.0f};
    }
    const FVector Right = Normalize(Cross(ReferenceUp, Forward));
    const FVector Up = Normalize(Cross(Forward, Right));

    FMatrix Matrix = ZeroMatrix();
    Matrix.M[0][0] = Forward.X; Matrix.M[0][1] = Forward.Y; Matrix.M[0][2] = Forward.Z;
    Matrix.M[1][0] = Right.X * Input.Size.X; Matrix.M[1][1] = Right.Y * Input.Size.X; Matrix.M[1][2] = Right.Z * Input.Size.X;
    Matrix.M[2][0] = Up.X * Input.Size.Y; Matrix.M[2][1] = Up.Y * Input.Size.Y; Matrix.M[2][2] = Up.Z * Input.Size.Y;
    Matrix.M[3][0] = Input.WorldPosition.X; Matrix.M[3][1] = Input.WorldPosition.Y; Matrix.M[3][2] = Input.WorldPosition.Z;
    Matrix.M[3][3] = 1.0f;
    return {Matrix};
}

// 거리 제곱을 캐시한 뒤 stable sort로 먼 파티클부터 ID를 출력한다.
void SortParticlesByCameraDistance(const TArray<FParticleSortInput>& Particles, const FVector& CameraLocation, TArray<ObjectId>& OutSortedBackToFront)
{
    // 파티클 ID와 미리 계산한 카메라 거리 제곱을 함께 담는다.
    struct FEntry { ObjectId Id; float DistanceSquared; };
    TArray<FEntry> Entries;
    for (const FParticleSortInput& Particle : Particles)
    {
        Entries.Add({Particle.Id, LengthSquared(Subtract(Particle.WorldPosition, CameraLocation))});
    }
    std::stable_sort(Entries.begin(), Entries.end(), [](const FEntry& A, const FEntry& B) { return A.DistanceSquared > B.DistanceSquared; });
    OutSortedBackToFront.Reset();
    for (const FEntry& Entry : Entries)
    {
        OutSortedBackToFront.Add(Entry.Id);
    }
}
