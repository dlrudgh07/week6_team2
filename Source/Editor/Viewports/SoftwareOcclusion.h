#pragma once

#include "Editor/Viewports/MultipleViewports.h"


enum class ESoftwareOcclusionMode : uint8
{
    Disabled,
    LinearSubcells,
    HierarchicalSubcells,
    StaticBVHHierarchical,
    StaticBVHFrustumOnly,
    GPUCompute,
};

class FGPUOcclusionCuller;
class FTexture2D;

enum class ESoftwareOccluderGeometry : uint8
{
    DistanceAdaptive,
    Bounds,
    MeshTriangles,
};

struct FSoftwareOcclusionSettings
{
    ESoftwareOcclusionMode Mode = ESoftwareOcclusionMode::Disabled;
    ESoftwareOccluderGeometry OccluderGeometry = ESoftwareOccluderGeometry::DistanceAdaptive;
    int32 TileSize = 8;
    int32 MinimumOccluderTiles = 16;
    uint32 TriangleBudget = 500000;
    float CpuTimeBudgetMs = 4.0f;
    float DepthBias = 0.0001f;
    bool bDebugBounds = false;
    float BoxOccluderDistanceThreshold = 10.0f;
};

struct FSoftwareOcclusionStats
{
    float NearestOccluderDistance = 0.0f;
    bool bUsingMeshOccluder = false;
    uint32 CapturedPrimitives = 0;
    uint32 StaticObjects = 0;
    uint32 DynamicObjects = 0;
    uint32 FrustumRejected = 0;
    uint32 OcclusionTested = 0;
    uint32 OcclusionRejected = 0;
    uint32 FinalVisible = 0;
    uint32 RenderPackets = 0;
    uint32 OccludersRasterized = 0;
    uint32 SourceTriangles = 0;
    uint32 ClippedTriangles = 0;
    uint32 BVHNodesTested = 0;
    uint32 BVHNodesPruned = 0;
    float SubcellCoveragePercent = 0.0f;
    float FullTileCoveragePercent = 0.0f;
    float BVHBuildMs = 0.0f;
    float CullMs = 0.0f;
    bool bTriangleBudgetExceeded = false;
    bool bCpuBudgetExceeded = false;
    bool bOcclusionSuspended = false;
};

enum class ESoftwareOcclusionDebugState : uint8
{
    Visible,
    Occluded,
    Fallback,
    StaticVisible,
    DynamicVisible,
};

struct FSoftwareOcclusionDebugBounds
{
    FAABB Bounds{};
    ESoftwareOcclusionDebugState State = ESoftwareOcclusionDebugState::Visible;
};

class FSoftwareOcclusionCuller
{
public:
    FSoftwareOcclusionCuller();
    ~FSoftwareOcclusionCuller();

    struct FCandidateDistance
    {
        uint32 Index = 0;
        float DistSq = 0.0f;
    };

    void ResetScene();
    void SetSettings(const FSoftwareOcclusionSettings& InSettings);
    const FSoftwareOcclusionSettings& GetSettings() const { return Settings; }
    const TArray<FSoftwareOcclusionDebugBounds>& GetDebugBounds() const { return DebugBounds; }
    FGPUOcclusionCuller* GetGPUCuller();
    const TArray<uint8>& GetVisibleLODs(int32 ViewIndex) const
    {
        if (ViewIndex >= 0 && ViewIndex < MaxViews)
        {
            return VisibleLODs[ViewIndex];
        }
        static const TArray<uint8> Empty;
        return Empty;
    }
    bool DidLastRayQueryRebuildBVH() const { return bLastRayQueryRebuiltBVH; }
    float GetLastRayQueryBVHBuildMs() const { return LastRayQueryBVHBuildMs; }

    // 조작 종료 시 동적 객체를 정적으로 복귀
    void SettleDynamicObjects() { bPendingSettle = true; }

    // World capture 당 한 번 호출하여 Static/Dynamic 상태와 BVH rebuild 필요성을 갱신한다.
    void SynchronizeObjects(const TArray<FRenderableObject>& Objects);

    // Ray와 교차하는 정적 BVH 리프의 객체와 BVH 밖의 동적 객체를 피킹 후보로 수집한다.
    void GatherRayCandidates(
        const FRay& Ray,
        const TArray<FRenderableObject>& Objects,
        TArray<FLineTraceCandidate>& OutCandidates);

    // View 하나의 프러스텀과 Software Occlusion을 실행한다.
    void Cull(
        int32 ViewIndex,
        const TArray<FRenderableObject>& Objects,
        const FFrustumPlanes& Frustum,
        const FMatrix& ViewProjection,
        const FVector& CameraLocation,
        int32 ViewWidth,
        int32 ViewHeight,
        bool bWireframe,
        TArray<UPrimitiveComponent*>& OutVisible,
        FSoftwareOcclusionStats& OutStats);

    void PostRenderOpaque(int32 ViewIndex, FTexture2D* SceneDepthTexture);

private:
    static constexpr int32 MaxBufferExtent = 320;
    static constexpr int32 SubcellsPerAxis = 4;
    static constexpr int32 SubcellCount = SubcellsPerAxis * SubcellsPerAxis;
    static constexpr uint16 FullCoverageMask = 0xffff;
    static constexpr uint32 BVHLeafSize = 32;
    static constexpr int32 SAHBinCount = 16;
    // 적응형 폴백: 프러스텀 통과 물체 중 가려낸 비율이 기준 미만이면 일정 프레임 동안 오클루전을 끈다.
    static constexpr int32 MaxViews = 4;
    static constexpr float MinOcclusionRejectRatio = 0.2f;
    static constexpr int32 OcclusionProbeInterval = 30;

    struct FObjectState
    {
        uint32 SerialNumber = 0;
        uint64 BoundsRevision = 0;
        uint64 SeenSerial = 0;
        bool bDynamic = false;
    };

    struct FOcclusionTile
    {
        uint16 CoverageMask = 0;
        float SubcellDepth[SubcellCount]{};
        bool bDirty = false;
    };

    struct FHZBCell
    {
        float Depth = 1.0f;
        bool bCovered = false;
        bool bDirty = false;
    };

    struct FHZBLevel
    {
        int32 Width = 0;
        int32 Height = 0;
        TArray<FHZBCell> Cells;
    };

    struct FBVHNode
    {
        FAABB Bounds{};
        uint32 Left = 0;
        uint32 Right = 0;
        uint32 First = 0;
        uint32 Count = 0;
        bool bLeaf = false;
    };

    struct FProjectedBounds
    {
        float MinX = 0.0f;
        float MinY = 0.0f;
        float MaxX = 0.0f;
        float MaxY = 0.0f;
        float NearestDepth = 0.0f;
        bool bValid = false;
        bool bUncertain = true;
    };

    FSoftwareOcclusionSettings Settings{};
    TArray<FObjectState> ObjectStates;
    uint64 SyncSerial = 0;
    bool bInitialized = false;
    bool bBVHDirty = true;
    bool bPendingSettle = false;
    int32 LastBuiltObjectCount = -1;

    TArray<uint32> StaticObjectIndices;
    TArray<uint32> DynamicObjectIndices;
    TArray<uint32> BypassObjectIndices;
    TArray<uint32> BuiltStaticObjectIndices;
    TArray<uint32> BVHObjectIndices;
    TArray<FBVHNode> BVHNodes;
    float LastBVHBuildMs = 0.0f;
    bool bLastRayQueryRebuiltBVH = false;
    float LastRayQueryBVHBuildMs = 0.0f;

    int32 BufferWidth = 0;
    int32 BufferHeight = 0;
    int32 TilesX = 0;
    int32 TilesY = 0;
    TArray<FOcclusionTile> Tiles;
    TArray<FHZBLevel> HZBLevels;
    TArray<uint32> DirtyTiles;
    TArray<uint32> DirtyHZBCells;
    TArray<uint32> NextDirtyHZBCells;
    TArray<FVector4> TransformedVertices;
    TArray<uint32> CandidateIndices;
    TArray<uint8> VisibilityFlags;
    struct FClippedTriangle
    {
        FVector4 V0;
        FVector4 V1;
        FVector4 V2;
    };

    TArray<FSoftwareOcclusionDebugBounds> DebugBounds;
    TArray<TArray<UPrimitiveComponent*>> WorkerVisibleBuffers;
    TArray<uint32> WorkerRejectedBuffers;
    TArray<TArray<uint32>> WorkerCandidateBuffers;
    TArray<uint32> OccluderIndices;
    TArray<TArray<FClippedTriangle>> WorkerClippedTriangleBuffers;
    TArray<FCandidateDistance> CandidateDistances;
    TArray<FCandidateDistance> CandidateDistancesTemp;
    int32 SuspendedFrames[MaxViews]{};

    FMatrix CurrentViewProjection{};
    FVector CurrentCameraLocation{};
    FFrustumPlanes CurrentFrustum{};
    uint32 UsedTriangles = 0;
    double CullStartSeconds = 0.0;
    bool bAllowRasterization = true;
    FSoftwareOcclusionStats* ActiveStats = nullptr;
#if defined(ENGINE_DEBUG)
    bool bSelfTestsRan = false;
#endif

    void PrepareBuffers(int32 ViewWidth, int32 ViewHeight);
    void ClearBuffers();
    void EnsureBVH(const TArray<FRenderableObject>& Objects);
    uint32 BuildBVHNode(const TArray<FRenderableObject>& Objects, uint32 First, uint32 Count);
    void TraverseRayBVH(const FTraceContext& Context, const TArray<FRenderableObject>& Objects, uint32 NodeIndex,
        float NodeDistance, float& ClosestDist, TArray<FLineTraceCandidate>& OutCandidates) const;
    void TraverseBVH(const TArray<FRenderableObject>& Objects, uint32 NodeIndex, bool bFrustumAccepted, bool bUseOcclusion, TArray<UPrimitiveComponent*>& OutVisible);
    void ProcessObject(const FRenderableObject& Object, bool bStatic, bool bFrustumAccepted, bool bUseOcclusion, TArray<UPrimitiveComponent*>& OutVisible);

    FProjectedBounds ProjectBounds(const FAABB& Bounds) const;
    bool IsOccluded(const FProjectedBounds& Bounds, bool bUseHierarchy) const;
    bool QueryHZBRegion(int32 MinTileX, int32 MinTileY, int32 MaxTileX, int32 MaxTileY, float NearestDepth) const;
    bool QueryHZBCell(int32 Level, int32 X, int32 Y, int32 MinTileX, int32 MinTileY, int32 MaxTileX, int32 MaxTileY, float NearestDepth) const;
    bool QueryEdgeSubcells(const FProjectedBounds& Bounds, int32 FullMinX, int32 FullMinY, int32 FullMaxX, int32 FullMaxY) const;

    void RasterizeOccluder(const FRenderableObject& Object, const FProjectedBounds& Projected, bool bUseMesh = false);
    void RasterizeClippedTriangle(const FVector4& A, const FVector4& B, const FVector4& C);
    void UpdateDirtyHZB();
    void UpdateHZBParent(int32 Level, int32 X, int32 Y);

    bool ShouldUseMeshOccluder(const FRenderableObject& Object) const;
    float DistanceSquaredToBounds(const FAABB& Bounds) const;
    void AddDebugBounds(const FAABB& Bounds, ESoftwareOcclusionDebugState State);
#if defined(ENGINE_DEBUG)
    void RunDebugSelfTests();
#endif

    TUniquePtr<FGPUOcclusionCuller> GPUCuller;
    TArray<uint8> VisibleLODs[MaxViews];
};
