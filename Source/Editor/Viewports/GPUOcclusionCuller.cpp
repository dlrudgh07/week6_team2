#include "EnginePCH.h"
#include "Editor/Viewports/GPUOcclusionCuller.h"

#include "Component/PrimitiveComponent.h"
#include "Rendering/RenderCommand.h"
#include "Rendering/GPUProfiler.h"
#include "Core/StatDefinitions.h"
#include "Rendering/RenderUtil.h"
#include "Rendering/Texture2D.h"
#include "Tasks/Tasks.h"

#include <algorithm>
#include <cstring>

FGPUOcclusionCuller::FGPUOcclusionCuller()
{
}

FGPUOcclusionCuller::~FGPUOcclusionCuller()
{
    ResetScene();
}

bool FGPUOcclusionCuller::Init()
{
    if (bInitialized)
    {
        return true;
    }

    FString CSOPath;
    FShaderByteCode ByteCode = RenderUtil::GetOrCompile(
        "Resources/Shader/OcclusionCullingCS.hlsl",
        "mainCS",
        EShaderType::Compute,
        CSOPath);

    if (!ByteCode.IsValid())
    {
        LOG(Error, "[GPUOcclusion] Failed to compile compute shader");
        return false;
    }

    ComputeShader = RenderCommand::CreateComputeShader(ByteCode);
    if (!ComputeShader || !ComputeShader->IsValid())
    {
        LOG(Error, "[GPUOcclusion] Failed to create compute shader device object");
        return false;
    }

    ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FGPUCullConstants));
    if (!ConstantBuffer)
    {
        LOG(Error, "[GPUOcclusion] Failed to create constant buffer");
        return false;
    }

    FString HZBCSOPath;
    FShaderByteCode HZBByteCode = RenderUtil::GetOrCompile(
        "Resources/Shader/HZBBuildCS.hlsl",
        "mainCS",
        EShaderType::Compute,
        HZBCSOPath);

    if (HZBByteCode.IsValid())
    {
        HZBBuildShader = RenderCommand::CreateComputeShader(HZBByteCode);
    }

    HZBBuildConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FHZBBuildConstants));

    bInitialized = true;
    return true;
}

void FGPUOcclusionCuller::ResetScene()
{
    CachedObjectCount = 0;
    CachedBoundsRevisions.Reset();
    bNeedsUpload = true;
    for (int32 View = 0; View < MaxSupportedViews; ++View)
    {
        CurrentWriteBufferIndex[View] = 0;
        bHasHistory[View] = false;
        bHasHZB[View] = false;
    }
}

void FGPUOcclusionCuller::EnsureCapacity(uint32 Count)
{
    if (Count <= CurrentCapacity && InstanceBuffer != nullptr)
    {
        return;
    }

    ID3D11Device* Device = RenderCommand::GetDevice();
    if (!Device)
    {
        return;
    }

    uint32 NewCapacity = (std::max)(65536u, Count);
    uint32 NewWordCapacity = (NewCapacity + 15) / 16;

    // 인스턴스 구조화 버퍼 생성
    D3D11_BUFFER_DESC InstDesc{};
    InstDesc.ByteWidth = sizeof(FGPUInstanceBound) * NewCapacity;
    InstDesc.Usage = D3D11_USAGE_DYNAMIC;
    InstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    InstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    InstDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    InstDesc.StructureByteStride = sizeof(FGPUInstanceBound);

    InstanceBuffer.Reset();
    InstanceSRV.Reset();

    HRESULT hr = Device->CreateBuffer(&InstDesc, nullptr, InstanceBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        LOG(Error, "[GPUOcclusion] Failed to create instance buffer");
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
    SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.FirstElement = 0;
    SRVDesc.Buffer.NumElements = NewCapacity;

    hr = Device->CreateShaderResourceView(InstanceBuffer.Get(), &SRVDesc, InstanceSRV.GetAddressOf());
    if (FAILED(hr))
    {
        LOG(Error, "[GPUOcclusion] Failed to create instance SRV");
        return;
    }

    // 가시성 출력 버퍼 생성
    D3D11_BUFFER_DESC VisDesc{};
    VisDesc.ByteWidth = sizeof(uint32) * NewWordCapacity;
    VisDesc.Usage = D3D11_USAGE_DEFAULT;
    VisDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    VisDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    VisDesc.StructureByteStride = sizeof(uint32);

    VisibilityBuffer.Reset();
    VisibilityUAV.Reset();

    hr = Device->CreateBuffer(&VisDesc, nullptr, VisibilityBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        LOG(Error, "[GPUOcclusion] Failed to create visibility buffer");
        return;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
    UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    UAVDesc.Buffer.FirstElement = 0;
    UAVDesc.Buffer.NumElements = NewWordCapacity;

    hr = Device->CreateUnorderedAccessView(VisibilityBuffer.Get(), &UAVDesc, VisibilityUAV.GetAddressOf());
    if (FAILED(hr))
    {
        LOG(Error, "[GPUOcclusion] Failed to create visibility UAV");
        return;
    }

    // 판독용 스테이징 버퍼 생성
    D3D11_BUFFER_DESC StagingDesc{};
    StagingDesc.ByteWidth = sizeof(uint32) * NewWordCapacity;
    StagingDesc.Usage = D3D11_USAGE_STAGING;
    StagingDesc.BindFlags = 0;
    StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    for (int32 View = 0; View < MaxSupportedViews; ++View)
    {
        for (int32 Buf = 0; Buf < 2; ++Buf)
        {
            StagingBuffers[View][Buf].Reset();
            Device->CreateBuffer(&StagingDesc, nullptr, StagingBuffers[View][Buf].GetAddressOf());
        }
        CurrentWriteBufferIndex[View] = 0;
        bHasHistory[View] = false;
    }

    CurrentCapacity = NewCapacity;
    CurrentWordCapacity = NewWordCapacity;
    bNeedsUpload = true;
}

void FGPUOcclusionCuller::EnsureHZBResources(int32 ViewIndex, uint32 Width, uint32 Height)
{
    ID3D11Device* Device = RenderCommand::GetDevice();
    if (!Device)
    {
        return;
    }

    if (!HZBTexture[ViewIndex])
    {
        D3D11_TEXTURE2D_DESC HZBDesc{};
        HZBDesc.Width = HZBWidth;
        HZBDesc.Height = HZBHeight;
        HZBDesc.MipLevels = HZBMipCount;
        HZBDesc.ArraySize = 1;
        HZBDesc.Format = DXGI_FORMAT_R32_FLOAT;
        HZBDesc.SampleDesc.Count = 1;
        HZBDesc.Usage = D3D11_USAGE_DEFAULT;
        HZBDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

        Device->CreateTexture2D(&HZBDesc, nullptr, HZBTexture[ViewIndex].ReleaseAndGetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC FullSRVDesc{};
        FullSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
        FullSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        FullSRVDesc.Texture2D.MostDetailedMip = 0;
        FullSRVDesc.Texture2D.MipLevels = HZBMipCount;
        Device->CreateShaderResourceView(HZBTexture[ViewIndex].Get(), &FullSRVDesc, HZBFullSRV[ViewIndex].ReleaseAndGetAddressOf());

        for (uint32 Mip = 0; Mip < HZBMipCount; ++Mip)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC MipSRVDesc{};
            MipSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
            MipSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            MipSRVDesc.Texture2D.MostDetailedMip = Mip;
            MipSRVDesc.Texture2D.MipLevels = 1;
            Device->CreateShaderResourceView(HZBTexture[ViewIndex].Get(), &MipSRVDesc, HZBMipSRV[ViewIndex][Mip].ReleaseAndGetAddressOf());

            D3D11_UNORDERED_ACCESS_VIEW_DESC MipUAVDesc{};
            MipUAVDesc.Format = DXGI_FORMAT_R32_FLOAT;
            MipUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            MipUAVDesc.Texture2D.MipSlice = Mip;
            Device->CreateUnorderedAccessView(HZBTexture[ViewIndex].Get(), &MipUAVDesc, HZBMipUAV[ViewIndex][Mip].ReleaseAndGetAddressOf());
        }
    }

    if (Width > 0 && Height > 0 && (Width != DepthCopyWidth[ViewIndex] || Height != DepthCopyHeight[ViewIndex]))
    {
        D3D11_TEXTURE2D_DESC DepthCopyDesc{};
        DepthCopyDesc.Width = Width;
        DepthCopyDesc.Height = Height;
        DepthCopyDesc.MipLevels = 1;
        DepthCopyDesc.ArraySize = 1;
        DepthCopyDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        DepthCopyDesc.SampleDesc.Count = 1;
        DepthCopyDesc.Usage = D3D11_USAGE_DEFAULT;
        DepthCopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        Device->CreateTexture2D(&DepthCopyDesc, nullptr, DepthCopyTexture[ViewIndex].ReleaseAndGetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC DepthSRVDesc{};
        DepthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        DepthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        DepthSRVDesc.Texture2D.MostDetailedMip = 0;
        DepthSRVDesc.Texture2D.MipLevels = 1;
        Device->CreateShaderResourceView(DepthCopyTexture[ViewIndex].Get(), &DepthSRVDesc, DepthCopySRV[ViewIndex].ReleaseAndGetAddressOf());

        DepthCopyWidth[ViewIndex] = Width;
        DepthCopyHeight[ViewIndex] = Height;
    }
}

void FGPUOcclusionCuller::BuildHZB(int32 ViewIndex, FTexture2D* SceneDepthTexture)
{
    if (!SceneDepthTexture || !HZBBuildShader || !HZBBuildConstantBuffer)
    {
        return;
    }

    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    if (!Context)
    {
        return;
    }

    const int32 ClampedView = std::clamp(ViewIndex, 0, MaxSupportedViews - 1);
    const uint32 DepthW = SceneDepthTexture->GetWidth();
    const uint32 DepthH = SceneDepthTexture->GetHeight();
    if (DepthW == 0 || DepthH == 0)
    {
        return;
    }

    EnsureHZBResources(ClampedView, DepthW, DepthH);

    if (!HZBTexture[ClampedView])
    {
        return;
    }

    FGPUStatScope HZBScope(StatIds::GpuHZB(), L"HZB Build");

    // OM 깊이 타겟 일시 해제 및 원본 깊이 직접 바인딩
    ID3D11RenderTargetView* CurrentRTVs[8] = { nullptr };
    ID3D11DepthStencilView* CurrentDSV = nullptr;
    Context->OMGetRenderTargets(1, CurrentRTVs, &CurrentDSV);
    if (CurrentDSV)
    {
        Context->OMSetRenderTargets(1, CurrentRTVs, nullptr);
    }

    RenderCommand::CSSetShader(HZBBuildShader.get(), Context);
    RenderCommand::CSSetSampler(0, ESamplerState::PointClamp, Context);

    // 첫 패스 다운샘플링
    RenderCommand::CSSetShaderResource(0, SceneDepthTexture->GetSRV(), Context);
    RenderCommand::CSSetUnorderedAccessView(0, HZBMipUAV[ClampedView][0].Get(), Context);

    RenderCommand::Dispatch((HZBWidth + 15) / 16, (HZBHeight + 15) / 16, 1, Context);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    ID3D11UnorderedAccessView* NullUAV = nullptr;
    RenderCommand::CSSetShaderResource(0, NullSRV, Context);
    RenderCommand::CSSetUnorderedAccessView(0, NullUAV, Context);

    // OM 깊이 타겟 복원
    if (CurrentDSV)
    {
        Context->OMSetRenderTargets(1, CurrentRTVs, CurrentDSV);
        CurrentDSV->Release();
    }
    if (CurrentRTVs[0])
    {
        CurrentRTVs[0]->Release();
    }

    // 밉맵 연속 다운샘플링
    for (uint32 Mip = 1; Mip < HZBMipCount; ++Mip)
    {
        const uint32 MipW = (std::max)(1u, HZBWidth >> Mip);
        const uint32 MipH = (std::max)(1u, HZBHeight >> Mip);

        RenderCommand::CSSetShaderResource(0, HZBMipSRV[ClampedView][Mip - 1].Get(), Context);
        RenderCommand::CSSetUnorderedAccessView(0, HZBMipUAV[ClampedView][Mip].Get(), Context);

        RenderCommand::Dispatch((MipW + 15) / 16, (MipH + 15) / 16, 1, Context);

        RenderCommand::CSSetShaderResource(0, NullSRV, Context);
        RenderCommand::CSSetUnorderedAccessView(0, NullUAV, Context);
    }

    bHasHZB[ClampedView] = true;
}

void FGPUOcclusionCuller::SynchronizeObjects(const TArray<FRenderableObject>& Objects)
{
    const uint32 ObjectCount = static_cast<uint32>(Objects.Num());
    if (ObjectCount != CachedObjectCount)
    {
        CachedObjectCount = ObjectCount;
        CachedBoundsRevisions.SetNum(ObjectCount);
        for (uint32 i = 0; i < ObjectCount; ++i)
        {
            CachedBoundsRevisions[i] = Objects[i].BoundsRevision;
        }
        bNeedsUpload = true;
        return;
    }

    if (!bNeedsUpload)
    {
        for (uint32 i = 0; i < ObjectCount; ++i)
        {
            if (CachedBoundsRevisions[i] != Objects[i].BoundsRevision)
            {
                CachedBoundsRevisions[i] = Objects[i].BoundsRevision;
                bNeedsUpload = true;
            }
        }
    }
}

void FGPUOcclusionCuller::Cull(
    const int32 ViewIndex,
    const TArray<FRenderableObject>& Objects,
    const FFrustumPlanes& Frustum,
    const FMatrix& ViewProjection,
    const FVector& CameraLocation,
    TArray<UPrimitiveComponent*>& OutVisible,
    TArray<uint8>& OutLODs,
    FSoftwareOcclusionStats& OutStats)
{
    OutVisible.Reset();
    OutLODs.Reset();
    const uint32 TotalObjects = static_cast<uint32>(Objects.Num());
    if (TotalObjects == 0)
    {
        return;
    }

    if (!bInitialized && !Init())
    {
        return;
    }

    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    if (!Context)
    {
        return;
    }

    EnsureCapacity(TotalObjects);

    // 인스턴스 데이터 전송
    if (bNeedsUpload && InstanceBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE Mapped{};
        HRESULT hr = Context->Map(InstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
        if (SUCCEEDED(hr))
        {
            FGPUInstanceBound* Dst = static_cast<FGPUInstanceBound*>(Mapped.pData);
            const uint32 NumWorkers = (std::max)(1u, Tasks::FTaskScheduler::Get().GetNumWorkers());
            const int32 ChunkSize = (static_cast<int32>(TotalObjects)  + NumWorkers - 1) / NumWorkers;

            Tasks::ParallelFor(static_cast<int32>(TotalObjects), ChunkSize, [&](int32 Start, int32 End)
            {
                for (int32 i = Start; i < End; ++i)
                {
                    const FAABB& Bounds = Objects[i].WorldBounds;
                    Dst[i].Center = Bounds.Center;
                    Dst[i].Radius = Bounds.Extent.Length();
                    Dst[i].Extent = Bounds.Extent;
                    Dst[i].Pad = 0;
                }
            });

            Context->Unmap(InstanceBuffer.Get(), 0);
            bNeedsUpload = false;
        }
    }

    const int32 ClampedView = std::clamp(ViewIndex, 0, MaxSupportedViews - 1);
    const uint32 NumWords = (TotalObjects + 15) / 16;

    // 상수 버퍼 갱신
    FGPUCullConstants Constants{};
    for (int32 i = 0; i < 6; ++i)
    {
        Constants.FrustumPlanes[i] = FVector4(Frustum.Planes[i].Normal, Frustum.Planes[i].Distance);
    }
    Constants.ViewProjection = ViewProjection;
    Constants.CameraPosition = CameraLocation;
    Constants.Pad0 = 0.0f;
    Constants.HZBSize = FVector2(static_cast<float>(HZBWidth), static_cast<float>(HZBHeight));
    Constants.NumInstances = TotalObjects;
    Constants.NumWords = NumWords;
    Constants.bUseHZB = bHasHZB[ClampedView] ? 1 : 0;
    Constants.NumHZBMips = HZBMipCount;
    Constants.DepthBias = 0.0001f;
    Constants.Pad = 0.0f;
    RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants, sizeof(FGPUCullConstants), Context);

    // 컴퓨트 파이프라인 바인딩
    RenderCommand::CSSetShader(ComputeShader.get(), Context);
    RenderCommand::CSSetConstantBuffer(0, ConstantBuffer.get(), Context);
    RenderCommand::CSSetShaderResource(0, InstanceSRV.Get(), Context);
    if (bHasHZB[ClampedView])
    {
        RenderCommand::CSSetShaderResource(1, HZBFullSRV[ClampedView].Get(), Context);
        RenderCommand::CSSetSampler(0, ESamplerState::PointClamp, Context);
    }
    RenderCommand::CSSetUnorderedAccessView(0, VisibilityUAV.Get(), Context);

    // 디스패치 실행
    const uint32 GroupCount = (TotalObjects + 63) / 64;
    {
        FGPUStatScope CullScope(StatIds::GpuCull(), L"Occlusion Dispatch");
        RenderCommand::Dispatch(GroupCount, 1, 1, Context);
    }

    // 바인딩 해제
    ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
    ID3D11UnorderedAccessView* NullUAV = nullptr;
    RenderCommand::CSSetShaderResources(0, 2, NullSRVs, Context);
    RenderCommand::CSSetUnorderedAccessView(0, NullUAV, Context);

    const int32 WriteSlot = CurrentWriteBufferIndex[ClampedView];
    ID3D11Buffer* TargetStaging = StagingBuffers[ClampedView][WriteSlot].Get();

    // 결과 스테이징 복사
    if (TargetStaging && VisibilityBuffer)
    {
        RenderCommand::CopyResource(TargetStaging, VisibilityBuffer.Get(), Context);
    }

    int32 ReadSlot = WriteSlot;
    if (bHasHistory[ClampedView])
    {
        // 이전 프레임 완결 버퍼 사용
        ReadSlot = 1 - WriteSlot;
        CurrentWriteBufferIndex[ClampedView] = 1 - WriteSlot;
    }
    else
    {
        bHasHistory[ClampedView] = true;
    }

    ID3D11Buffer* ReadStaging = StagingBuffers[ClampedView][ReadSlot].Get();
    if (!ReadStaging)
    {
        return;
    }

    ReadbackBits.SetNum(NumWords);

    D3D11_MAPPED_SUBRESOURCE ReadMapped{};
    HRESULT hr;
    {
        FStatScope ReadbackScope(StatIds::GpuReadbackCPU());
        hr = Context->Map(ReadStaging, 0, D3D11_MAP_READ, 0, &ReadMapped);
    }
    if (FAILED(hr))
        FStats::Add(StatIds::GpuReadbackFailures(), 1);
    if (SUCCEEDED(hr))
    {
        std::memcpy(ReadbackBits.GetData(), ReadMapped.pData, NumWords * sizeof(uint32));
        Context->Unmap(ReadStaging, 0);

        // 단계별 수량 계수
        uint32 LodCounts[3] = { 0, 0, 0 };
        for (uint32 WordIdx = 0; WordIdx < NumWords; ++WordIdx)
        {
            uint32 Mask = ReadbackBits[WordIdx];
            if (Mask == 0)
            {
                continue;
            }

            uint32 BaseIdx = WordIdx * 16;
            for (uint32 Bit = 0; Bit < 16; ++Bit)
            {
                uint32 ObjIdx = BaseIdx + Bit;
                if (ObjIdx >= TotalObjects)
                {
                    break;
                }

                uint32 Code = (Mask >> (Bit * 2)) & 0x03;
                if (Code > 0 && Objects[ObjIdx].Primitive)
                {
                    ++LodCounts[Code - 1];
                }
            }
        }

        const uint32 TotalVisible = LodCounts[0] + LodCounts[1] + LodCounts[2];
        OutVisible.SetNum(TotalVisible);
        OutLODs.SetNum(TotalVisible);

        uint32 Offsets[3] = { 0, LodCounts[0], LodCounts[0] + LodCounts[1] };

        // 단계별 오프셋 위치에 직접 기록
        for (uint32 WordIdx = 0; WordIdx < NumWords; ++WordIdx)
        {
            uint32 Mask = ReadbackBits[WordIdx];
            if (Mask == 0)
            {
                continue;
            }

            uint32 BaseIdx = WordIdx * 16;
            for (uint32 Bit = 0; Bit < 16; ++Bit)
            {
                uint32 ObjIdx = BaseIdx + Bit;
                if (ObjIdx >= TotalObjects)
                {
                    break;
                }

                uint32 Code = (Mask >> (Bit * 2)) & 0x03;
                if (Code > 0 && Objects[ObjIdx].Primitive)
                {
                    const uint32 LodIdx = Code - 1;
                    const uint32 TargetIdx = Offsets[LodIdx]++;
                    OutVisible[TargetIdx] = Objects[ObjIdx].Primitive;
                    OutLODs[TargetIdx] = static_cast<uint8>(LodIdx);
                }
            }
        }
    }

    OutStats.CapturedPrimitives = TotalObjects;
    OutStats.FinalVisible = static_cast<uint32>(OutVisible.Num());
    OutStats.FrustumRejected = TotalObjects - OutStats.FinalVisible;
}
