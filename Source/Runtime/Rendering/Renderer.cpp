#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"
#include "Material.h"

#include "RenderCommand.h"
#include "GPUProfiler.h"
#include "Core/StatDefinitions.h"
#include "Core/EngineTimer.h"
#include "Camera/CameraComponent.h"
#include "Tasks/Tasks.h"

#include <algorithm>
#include <functional>
#include <limits>

namespace
{
	struct FBatchCounters
	{
		uint64 Draws = 0;
		uint64 Triangles = 0;
		uint64 UploadBytes = 0;
	};

	// Destruction and publication occur on the calling thread, never on workers.
	struct FBatchStats : FBatchCounters
	{
		const bool bCountDraws = FStats::IsEnabled(StatIds::DrawCalls());
		const bool bCountTriangles = FStats::IsEnabled(StatIds::Triangles());
		const bool bCountUploadBytes = FStats::IsEnabled(StatIds::CBUpload());

		bool IsCollecting() const { return bCountDraws || bCountTriangles || bCountUploadBytes; }

		~FBatchStats()
		{
			if (bCountDraws)
				FStats::Add(StatIds::DrawCalls(), static_cast<double>(Draws));
			if (bCountTriangles)
				FStats::Add(StatIds::Triangles(), static_cast<double>(Triangles));
			if (bCountUploadBytes)
				FStats::Add(StatIds::CBUpload(), static_cast<double>(UploadBytes));
		}
	};

	EPSOType GetPacketPSO(const FRenderPacket& Packet)
	{
		return Packet.material ? Packet.material->PSOType : EPSOType::Count;
	}

	bool IsOpaquePSO(const EPSOType PSO)
	{
		return PSO == EPSOType::StaticMesh_Opaque || PSO == EPSOType::StaticMesh_Wireframe;
	}

	bool CompareStateThenDepth(const FRenderPacket& A, const FRenderPacket& B)
	{
		const EPSOType APSO = GetPacketPSO(A);
		const EPSOType BPSO = GetPacketPSO(B);
		if (APSO != BPSO)
			return static_cast<uint8>(APSO) < static_cast<uint8>(BPSO);

		if (A.material != B.material)
			return std::less<>{}(A.material, B.material);

		if (A.mesh != B.mesh)
			return std::less<>{}(A.mesh, B.mesh);

		return IsOpaquePSO(APSO)
			? A.CameraDistanceSquared < B.CameraDistanceSquared
			: A.CameraDistanceSquared > B.CameraDistanceSquared;
	}

}

bool FRenderer::Init()
{
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	if (RenderCommand::GetRenderDevice()->SupportsConstantBufferOffsetting())
	{
		return EnsureConstantBufferCapacity(Temp, InitialPacketCapacity);
	}
	Temp = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));
	return Temp && Temp->GetBuffer();
}

bool FRenderer::EnsureConstantBufferCapacity(TUniquePtr<FConstantBuffer>& Buffer, uint32 PacketCount)
{
	const uint64 RequiredBytes = static_cast<uint64>(PacketCount) * PerObjectSlotSize;
	const uint64 MaxBytes = (std::numeric_limits<uint32>::max)() / PerObjectSlotSize * static_cast<uint64>(PerObjectSlotSize);
	if (RequiredBytes > MaxBytes)
	{
		LOG(Warning, "Per-object constant buffer size exceeds the D3D11 byte-width limit.");
		return false;
	}
	const uint64 CurrentBytes = Buffer && Buffer->GetBuffer() ? Buffer->GetBufferSize() : 0;
	if (CurrentBytes >= RequiredBytes)
	{
		return true;
	}
	const uint32 NewBytes = static_cast<uint32>((std::min)(MaxBytes, (std::max)(RequiredBytes, CurrentBytes * 2)));
	auto NewBuffer = RenderCommand::CreateConstantBuffer(NewBytes);
	if (!NewBuffer || !NewBuffer->GetBuffer())
	{
		LOG(Warning, "Failed to allocate per-object constant buffer ({} bytes).", NewBytes);
		return false;
	}
	Buffer = std::move(NewBuffer);
	return true;
}

// 지연 워커 초기화
void FRenderer::EnsureDeferredWorkers()
{
	if (bDeferredWorkersInitialized)
	{
		return;
	}
	bDeferredWorkersInitialized = true;
	const FRenderDevice* Device = RenderCommand::GetRenderDevice();
	// 에뮬레이션된 커맨드 리스트에서는 오프셋만 바꾸는 바인딩을 사용하지 않는다.
	if (!Device->SupportsConstantBufferOffsetting() || !Device->SupportsNativeCommandLists())
	{
		return;
	}

	uint32 WorkerCount = Tasks::FTaskScheduler::Get().GetNumWorkers();
	if (WorkerCount == 0)
	{
		WorkerCount = (std::max)(1u, std::thread::hardware_concurrency());
	}

	DeferredWorkers.SetNum(WorkerCount);

	for (uint32 Index = 0; Index < WorkerCount; ++Index)
	{
		DeferredWorkers[Index].Context = RenderCommand::CreateDeferredContext();

		if (!DeferredWorkers[Index].Context
			|| FAILED(DeferredWorkers[Index].Context.As(&DeferredWorkers[Index].Context1)))
		{
			LOG(Warning, "Deferred rendering unavailable; using immediate context.");
			DeferredWorkers.SetNum(0);
			return;
		}
	}
}

// 기존 단일 카메라의 ViewProjection으로 렌더 패킷 배열 전체를 그린다.
void FRenderer::RenderAll(TArray<FRenderPacket>& InPackets, UCameraComponent* CameraComponent)
{
	RenderAll(InPackets, CameraComponent->GetViewProjectionMatrix());
}

// 불투명 우선·반투명 거리순으로 배열을 정렬해 View 행렬과 Section 범위로 그린다.
void FRenderer::RenderAll(TArray<FRenderPacket>& InPackets, const FMatrix& ViewProjection)
{
	RenderOpaque(InPackets, ViewProjection);
	RenderTranslucent(ViewProjection);
}

// TArray 기반 불투명 메시 지연 컨텍스트 병렬 렌더링
void FRenderer::RenderOpaque(TArray<FRenderPacket>& InPackets, const FMatrix& ViewProjection, bool bWireframe)
{
	FStatScope SubmitScope(StatIds::RenderSubmit());
	FBatchStats BatchStats;
	const int32 TotalPackets = InPackets.Num();
	if (TotalPackets == 0)
	{
		return;
	}

	if (!ViewCB)
	{
		ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	}
	RenderCommand::UpdateBufferData(ViewCB.get(), &ViewProjection, sizeof(FMatrix));

	EnsureDeferredWorkers();

	{
		//FStatScope SortScope(StatIds::RenderSort());
		//std::sort(InPackets.begin(), InPackets.end(), CompareStateThenDepth);
	}

	{
		FStatScope MaterialScope(StatIds::RenderMaterials());
		// 머티리얼 파라미터 사전 일괄 갱신
		TArray<UMaterial*> UniqueMaterials;
		UniqueMaterials.Reserve(8);
		for (const FRenderPacket& Packet : InPackets)
		{
			if (Packet.material && std::find(UniqueMaterials.begin(), UniqueMaterials.end(), Packet.material) == UniqueMaterials.end())
			{
				UniqueMaterials.Add(Packet.material);
			}
		}
		for (UMaterial* Mat : UniqueMaterials)
		{
			UpdateMaterialParams(Mat);
		}

	}

	const int32 NumWorkers = DeferredWorkers.Num();
	const bool bUseOffsets = RenderCommand::GetRenderDevice()->SupportsConstantBufferOffsetting();

	if (TotalPackets <= 500 || NumWorkers <= 1)
	{
		FGPUStatScope OpaqueScope(StatIds::GpuOpaque(), L"Opaque Immediate");
		const UStaticMesh* LastMesh = nullptr;
		uint8 LastLOD = 0xFF;
		const UMaterial* LastMaterial = nullptr;
		EPSOType LastPSO = EPSOType::Count;

		if (bUseOffsets)
		{
			FStatScope UploadScope(StatIds::RenderUpload());
			if (!EnsureConstantBufferCapacity(Temp, TotalPackets))
			{
				return;
			}
			void* MappedData = RenderCommand::MapBufferWriteDiscard(Temp.get());
			if (!MappedData)
			{
				return;
			}
			uint8* Base = static_cast<uint8*>(MappedData);
			for (int32 i = 0; i < TotalPackets; ++i)
			{
				FPerObjectConstants Constants;
				Constants.World = InPackets[i].model;
				std::memcpy(Base + static_cast<size_t>(i) * PerObjectSlotSize, &Constants, sizeof(Constants));
			}
			RenderCommand::UnmapBuffer(Temp.get());
			if (BatchStats.bCountUploadBytes)
				BatchStats.UploadBytes += static_cast<uint64>(TotalPackets) * sizeof(FPerObjectConstants);
		}

		RenderCommand::BindConstantBuffer(2, ViewCB.get(), EShaderBindFlagBits::Vertex);

		FStatScope DrawScope(StatIds::RenderDrawLoop());
		for (int32 i = 0; i < TotalPackets; ++i)
		{
			const FRenderPacket& RenderPacket = InPackets[i];

			if (RenderPacket.mesh == nullptr || RenderPacket.material == nullptr)
			{
				continue;
			}

			if (LastMesh != RenderPacket.mesh || LastLOD != RenderPacket.LODIndex)
			{
				RenderCommand::BindMesh(RenderPacket.mesh, RenderPacket.LODIndex);
				LastMesh = RenderPacket.mesh;
				LastLOD = RenderPacket.LODIndex;
			}

			if (LastMaterial != RenderPacket.material)
			{
				const bool bPSOChanged = LastPSO != RenderPacket.material->PSOType;
				BindMaterial(RenderPacket.material, nullptr, bPSOChanged, bWireframe);
				LastMaterial = RenderPacket.material;
				LastPSO = RenderPacket.material->PSOType;
			}

			const uint32 FirstConstant = i * (PerObjectSlotSize / 16);
			const uint32 NumConstants = PerObjectSlotSize / 16;

			if (bUseOffsets)
			{
				RenderCommand::BindConstantBufferRange(0, Temp.get(), EShaderBindFlagBits::Vertex, FirstConstant, NumConstants);
			}
			else
			{
				void* MappedData = RenderCommand::MapBufferWriteDiscard(Temp.get());
				if (!MappedData)
				{
					return;
				}
				FPerObjectConstants Constants;
				Constants.World = RenderPacket.model;
				std::memcpy(MappedData, &Constants, sizeof(Constants));
				RenderCommand::UnmapBuffer(Temp.get());
				if (BatchStats.bCountUploadBytes)
					BatchStats.UploadBytes += sizeof(FPerObjectConstants);
				RenderCommand::BindConstantBuffer(0, Temp.get(), EShaderBindFlagBits::Vertex);
			}
			const uint32 IndexCount = RenderPacket.IndexCount ? RenderPacket.IndexCount : RenderPacket.mesh->GetIndexCount(RenderPacket.LODIndex);
			RenderCommand::DrawIndexed(IndexCount, RenderPacket.StartIndex);
			if (BatchStats.bCountDraws)
				++BatchStats.Draws;
			if (BatchStats.bCountTriangles)
				BatchStats.Triangles += IndexCount / 3;
		}
		return;
	}

	const int32 ChunkSize = 1 + (TotalPackets - 1) / NumWorkers;
	const int32 NumJobs = 1 + (TotalPackets - 1) / ChunkSize;
	for (int32 Index = 0; Index < NumJobs; ++Index)
	{
		const int32 PacketCount = (std::min)(ChunkSize, TotalPackets - Index * ChunkSize);
		if (!EnsureConstantBufferCapacity(DeferredWorkers[Index].PerObjectCB, PacketCount))
		{
			return;
		}
	}

	// 현재 바인딩된 렌더 타깃과 뷰포트 정보 획득
	ID3D11RenderTargetView* RTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
	ID3D11DepthStencilView* DSV = nullptr;
	RenderCommand::GetRenderDevice()->GetContext()->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, RTVs, &DSV);

	UINT NumRTVs = 0;
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		if (RTVs[i])
		{
			NumRTVs = i + 1;
		}
	}

	UINT NumViewports = 1;
	D3D11_VIEWPORT Viewport{};
	RenderCommand::GetRenderDevice()->GetContext()->RSGetViewports(&NumViewports, &Viewport);

	std::vector<ComPtr<ID3D11CommandList>> CommandLists(NumJobs);
	std::vector<FBatchCounters> WorkerCounters;
	if (BatchStats.IsCollecting())
		WorkerCounters.resize(NumJobs);

	{
		FStatScope WorkerScope(StatIds::RenderWorkers());
		Tasks::ParallelFor(TotalPackets, ChunkSize, [&](int32 Start, int32 End)
		{
			const int32 JobIndex = Start / ChunkSize;
			if (JobIndex >= DeferredWorkers.Num())
			{
				return;
			}

			// Accumulate locally; publish once instead of writing adjacent worker slots per draw.
			FBatchCounters LocalCounters;

			ID3D11DeviceContext* Context = DeferredWorkers[JobIndex].Context.Get();
			FConstantBuffer* WorkerCB = DeferredWorkers[JobIndex].PerObjectCB.get();
			ID3D11DeviceContext1* Context1 = DeferredWorkers[JobIndex].Context1.Get();

			Context->OMSetRenderTargets(NumRTVs, RTVs, DSV);
			Context->RSSetViewports(NumViewports, &Viewport);
			RenderCommand::BindConstantBuffer(2, ViewCB.get(), EShaderBindFlagBits::Vertex, Context);

			const UStaticMesh* LastMesh  = nullptr;
			const UMaterial* LastMaterial = nullptr;
			uint8 LastLOD = 0xFF;
			EPSOType LastPSO = EPSOType::Count;

			assert(static_cast<uint32>(End - Start) * PerObjectSlotSize <= WorkerCB->GetBufferSize());

			void* MappedData = RenderCommand::MapBufferWriteDiscard(WorkerCB, Context);

			if (!MappedData)
			{
				ComPtr<ID3D11CommandList> Discarded;
				Context->FinishCommandList(FALSE, Discarded.GetAddressOf());
				return;
			}

			uint8* Base = static_cast<uint8*>(MappedData);

			for (int32 i = Start; i < End; ++i)
			{
				FPerObjectConstants Constants;
				Constants.World = InPackets[i].model;

				const uint32 LocalIndex = i - Start;

				uint8* Dest = Base + LocalIndex * PerObjectSlotSize;

				std::memcpy(Dest, &Constants, sizeof(FPerObjectConstants));
			}

			RenderCommand::UnmapBuffer(WorkerCB, Context);
			if (BatchStats.bCountUploadBytes)
				LocalCounters.UploadBytes = static_cast<uint64>(End - Start) * sizeof(FPerObjectConstants);

		for (int32 i = Start; i < End; ++i)
		{
			const FRenderPacket& RenderPacket = InPackets[i];
			if (RenderPacket.mesh != nullptr && RenderPacket.material != nullptr)
			{
				if (LastMesh != RenderPacket.mesh || LastLOD != RenderPacket.LODIndex)
				{
					RenderCommand::BindMesh(RenderPacket.mesh, RenderPacket.LODIndex, Context);
					LastMesh = RenderPacket.mesh;
					LastLOD = RenderPacket.LODIndex;
				}

				if (LastMaterial != RenderPacket.material)
				{
					const bool bPSOChanged = LastPSO != RenderPacket.material->PSOType;
					BindMaterial(RenderPacket.material, Context, bPSOChanged, bWireframe);
					LastMaterial = RenderPacket.material;
					LastPSO = RenderPacket.material->PSOType;
				}

				const uint32 LocalIndex = i - Start;
				const uint32 FirstConstant = LocalIndex * (PerObjectSlotSize / 16);
				const uint32 NumConstants = PerObjectSlotSize / 16;

				RenderCommand::BindConstantBufferRange(0, WorkerCB, EShaderBindFlagBits::Vertex, FirstConstant, NumConstants, Context1);

				const uint32 IndexCount = RenderPacket.IndexCount ? RenderPacket.IndexCount : RenderPacket.mesh->GetIndexCount(RenderPacket.LODIndex);
				RenderCommand::DrawIndexed(
					IndexCount,
					RenderPacket.StartIndex,
					0,
					Context
				);
				if (BatchStats.bCountDraws)
					++LocalCounters.Draws;
				if (BatchStats.bCountTriangles)
					LocalCounters.Triangles += IndexCount / 3;
			}
		}

			if (FAILED(Context->FinishCommandList(FALSE, CommandLists[JobIndex].GetAddressOf())))
			{
				CommandLists[JobIndex].Reset();
			}
			if (BatchStats.IsCollecting())
				WorkerCounters[JobIndex] = LocalCounters;
		});
	}

	// 메인 스레드에서 커맨드 리스트 순차 실행
	const bool bAllJobsSucceeded = std::all_of(CommandLists.begin(), CommandLists.end(), [](const auto& List) { return List != nullptr; });
	if (!bAllJobsSucceeded)
	{
		// 로그와 컨텍스트 폐기는 워커가 모두 종료된 뒤 메인 스레드에서 처리한다.
		LOG(Warning, "Per-object buffer upload or command-list recording failed; skipping this render batch.");
		DeferredWorkers.SetNum(0);
		bDeferredWorkersInitialized = false;
	}
	if (BatchStats.bCountUploadBytes)
	{
		for (const FBatchCounters& Counters : WorkerCounters)
			BatchStats.UploadBytes += Counters.UploadBytes;
	}
	{
		FStatScope ExecuteScope(StatIds::RenderExecute());
		FGPUStatScope OpaqueScope(StatIds::GpuOpaque(), L"Opaque Command Lists");
		for (int32 i = 0; bAllJobsSucceeded && i < NumJobs; ++i)
		{
			if (CommandLists[i])
			{
				RenderCommand::ExecuteCommandList(CommandLists[i].Get(), false);
				if (BatchStats.bCountDraws)
					BatchStats.Draws += WorkerCounters[i].Draws;
				if (BatchStats.bCountTriangles)
					BatchStats.Triangles += WorkerCounters[i].Triangles;
			}
		}
	}

	// 실행 후 메인 즉시 컨텍스트의 렌더 타깃과 뷰포트 상태 복구
	RenderCommand::GetRenderDevice()->GetContext()->OMSetRenderTargets(NumRTVs, RTVs, DSV);
	RenderCommand::GetRenderDevice()->GetContext()->RSSetViewports(NumViewports, &Viewport);

	// 획득한 렌더 타깃 참조 해제
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		if (RTVs[i])
		{
			RTVs[i]->Release();
		}
	}
	if (DSV)
	{
		DSV->Release();
	}
}

// 반투명 객체가 없으므로 아무 작업도 수행하지 않음
void FRenderer::RenderTranslucent(const FMatrix& ViewProjection)
{
}

// 기존 인터페이스 호환용 함수
void FRenderer::DrawPackets(uint32 Begin, uint32 End, const FMatrix& ViewProjection)
{
}

// 머티리얼 바인딩
void FRenderer::BindMaterial(UMaterial* material, ID3D11DeviceContext* Context, const bool bBindPipelineState, const bool bWireframe)
{
	if (bBindPipelineState)
	{
		const EPSOType EffectivePSO = (bWireframe && (material->PSOType == EPSOType::StaticMesh_Opaque || material->PSOType == EPSOType::StaticMesh_Wireframe))
			? EPSOType::StaticMesh_Wireframe
			: material->PSOType;
		RenderCommand::BindPipelineState(FRenderResourceManager::GetPSO(EffectivePSO), Context);
	}
	for (int i = 0; i < material->Textures.size(); i++)
	{
		RenderCommand::BindShaderResource(i, material->Textures[i], EShaderBindFlagBits::Pixel, Context);
	}
	RenderCommand::BindSamplerState(0, material->SamplerState, EShaderBindFlagBits::Pixel, Context);
	if (material->ParamBuffer)
	{
		RenderCommand::BindConstantBuffer(1, material->ParamBuffer.get(), EShaderBindFlagBits::Pixel, Context);
	}
}

// 머티리얼 파라미터 버퍼 갱신
void FRenderer::UpdateMaterialParams(UMaterial* material)
{
	if (!material || !material->ParamBuffer)
	{
		return;
	}

	switch (material->PSOType)
	{
	case EPSOType::StaticMesh_Opaque:
	case EPSOType::StaticMesh_Wireframe:
	{
		const float TotalTime = EngineTimer::GetTotalTime();
		FStaticMeshMaterialParams Params{};
		Params.BaseColor = material->BaseColor;
		Params.UVOffset = material->UVScrollSpeed * TotalTime;
		Params.bOpaque = 1.0f;
		RenderCommand::UpdateBufferData(material->ParamBuffer.get(), &Params, sizeof(FStaticMeshMaterialParams));
		break;
	}
	case EPSOType::StaticMesh_Translucent:
	{
		const float TotalTime = EngineTimer::GetTotalTime();
		FStaticMeshMaterialParams Params{};
		Params.BaseColor = material->BaseColor;
		Params.UVOffset = material->UVScrollSpeed * TotalTime;
		Params.bOpaque = 0.0f;
		RenderCommand::UpdateBufferData(material->ParamBuffer.get(), &Params, sizeof(FStaticMeshMaterialParams));
		break;
	}
	default:
		break;
	}
}

// 오브젝트 상수 버퍼 갱신
void FRenderer::UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrixRegister& ViewProjection)
{
	FPerObjectConstants Constants;
	Constants.World = RenderPacket.model;
	RenderCommand::UpdateBufferData(Temp.get(), &Constants);
}
