#pragma once

#include "Core/Types.h"
#include "Container/Array.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Editor/Viewports/MultipleViewports.h"
#include "Editor/Viewports/SoftwareOcclusion.h"
#include "Rendering/Shader.h"
#include "Rendering/Buffer.h"

class FTexture2D;

struct alignas(16) FGPUInstanceBound
{
    FVector Center;
    float Radius;
    FVector Extent;
    uint32 Pad;
};

struct alignas(16) FGPUCullConstants
{
    FVector4 FrustumPlanes[6];
    FMatrix ViewProjection;
    FVector CameraPosition;
    float Pad0;
    FVector2 HZBSize;
    uint32 NumInstances;
    uint32 NumWords;
    uint32 bUseHZB;
    uint32 NumHZBMips;
    float DepthBias;
    float Pad;
};

struct alignas(16) FHZBBuildConstants
{
    uint32 DestWidth;
    uint32 DestHeight;
    uint32 SourceWidth;
    uint32 SourceHeight;
    uint32 bIsFirstPass;
    uint32 Pad[3];
};

class FGPUOcclusionCuller
{
public:
    FGPUOcclusionCuller();
    ~FGPUOcclusionCuller();

    // 자원 초기화
    bool Init();

    // 씬 객체 동기화 및 버퍼 갱신
    void SynchronizeObjects(const TArray<FRenderableObject>& Objects);

    // 컴퓨트 셰이더 실행 및 가시성 판정
    void Cull(
        int32 ViewIndex,
        const TArray<FRenderableObject>& Objects,
        const FFrustumPlanes& Frustum,
        const FMatrix& ViewProjection,
        const FVector& CameraLocation,
        TArray<UPrimitiveComponent*>& OutVisible,
        TArray<uint8>& OutLODs,
        FSoftwareOcclusionStats& OutStats);

    // 깊이 버퍼 다운샘플링 피라미드 생성
    void BuildHZB(int32 ViewIndex, FTexture2D* SceneDepthTexture);

    // 씬 리셋
    void ResetScene();

    // 인스턴스 버퍼 재업로드 요청
    void MarkNeedsUpload() { bNeedsUpload = true; }

    bool IsInitialized() const { return bInitialized; }

private:
    void EnsureCapacity(uint32 Count);
    void EnsureHZBResources(int32 ViewIndex, uint32 Width, uint32 Height);

    static constexpr int32 MaxSupportedViews = 4;
    static constexpr uint32 HZBWidth = 512;
    static constexpr uint32 HZBHeight = 256;
    static constexpr uint32 HZBMipCount = 10;

    bool bInitialized = false;
    uint32 CurrentCapacity = 0;
    uint32 CurrentWordCapacity = 0;
    uint32 CachedObjectCount = 0;
    bool bNeedsUpload = true;
    TArray<uint64> CachedBoundsRevisions;

    TUniquePtr<FComputeShader> ComputeShader;
    TUniquePtr<FConstantBuffer> ConstantBuffer;

    TUniquePtr<FComputeShader> HZBBuildShader;
    TUniquePtr<FConstantBuffer> HZBBuildConstantBuffer;

    ComPtr<ID3D11Buffer> InstanceBuffer;
    ComPtr<ID3D11ShaderResourceView> InstanceSRV;

    ComPtr<ID3D11Buffer> VisibilityBuffer;
    ComPtr<ID3D11UnorderedAccessView> VisibilityUAV;

    // 더블 버퍼링 스테이징 버퍼
    ComPtr<ID3D11Buffer> StagingBuffers[MaxSupportedViews][2];
    int32 CurrentWriteBufferIndex[MaxSupportedViews]{};
    bool bHasHistory[MaxSupportedViews]{};

    // 깊이 피라미드 자원
    ComPtr<ID3D11Texture2D> HZBTexture[MaxSupportedViews];
    ComPtr<ID3D11ShaderResourceView> HZBFullSRV[MaxSupportedViews];
    ComPtr<ID3D11ShaderResourceView> HZBMipSRV[MaxSupportedViews][HZBMipCount];
    ComPtr<ID3D11UnorderedAccessView> HZBMipUAV[MaxSupportedViews][HZBMipCount];
    bool bHasHZB[MaxSupportedViews]{};
    FMatrix LastViewProjection[MaxSupportedViews]{};

    // 깊이 복사 버퍼
    ComPtr<ID3D11Texture2D> DepthCopyTexture[MaxSupportedViews];
    ComPtr<ID3D11ShaderResourceView> DepthCopySRV[MaxSupportedViews];
    uint32 DepthCopyWidth[MaxSupportedViews]{};
    uint32 DepthCopyHeight[MaxSupportedViews]{};

    TArray<uint32> ReadbackBits;
};
