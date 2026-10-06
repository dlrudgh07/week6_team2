#include "EnginePCH.h"
#include "Editor/Viewports/PIEViewAdapter.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "../../Runtime/Components/PrimitiveComponent.h"
#include "../../Runtime/Components/StaticMeshComponent.h"
#include "../../Runtime/Components/BillboardComponent.h"
#include "../../Runtime/Particles/ParticleSubUVComponent.h"

#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/Rendering/GridRenderer.h"
#include "Input/InputSystem.h"
#include "../../Runtime/Materials/Material.h"
#include "Rendering/LineBatcher.h"
#include "../../Runtime/UObject/UObjectIterator.h"
#include "../../Runtime/Engine/World.h"

#include "../Viewports/PIEViewportPanel.h"

#include "Tasks/Tasks.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace
{
constexpr float Pi = 3.14159265358979323846f;
constexpr float MaximumCameraPitchDegrees = 89.0f;
constexpr float MaximumRotationDegreesPerFrame = 45.0f;
constexpr float MouseWheelDeltaPerStep = 120.0f;
constexpr float OrthographicZoomFactorPerStep = 0.9f;
constexpr float MinimumOrthographicWidth = 0.01f;
// 양방향 깊이 보정 뒤에도 절두체 평면 법선이 Core epsilon(1e-6)보다 충분히 크게
// 유지되게 한다. 정사각 View의 최악 조건에서 깊이 계수는 약 1 / (4 * sqrt(2) *
// Span)이다.
constexpr float MaximumOrthographicSpan = 80000.0f;

// 외부 설정과 화면 비율을 반영해 직교 투영 폭이 안전한 범위를 벗어나지 않게
// 한다.
void ConstrainOrthographicWidth(FCameraProjection& Projection, const FRect& Rect)
{
	const float AspectRatio = Rect.Width > 0.0f && Rect.Height > 0.0f ? Rect.Width / Rect.Height : 1.0f;
	// 폭과 높이 모두 상한 안에 들도록 세로로 긴 View에서는 폭 상한도 함께 줄인다.
	const float MaximumWidth = std::max(MinimumOrthographicWidth, MaximumOrthographicSpan * std::min(1.0f, AspectRatio));
	Projection.OrthoWidth = std::isnan(Projection.OrthoWidth) ? 10.0f : FMath::Clamp(Projection.OrthoWidth, MinimumOrthographicWidth, MaximumWidth);
}

// 네 View Rect의 중앙 경계에서 절반씩 줄여 Splitter가 차지할 실제 빈 공간을
// 만든다.
void ApplySplitterGutter(const FVector2D WindowSize, FRect ViewRects[4])
{
	const float SplitX = ViewRects[0].Width;
	const float SplitY = ViewRects[0].Height;
	const float HalfThickness = SplitterThickness * 0.5f;
	const float HalfGapX = std::min({HalfThickness, SplitX, WindowSize.X - SplitX});
	const float HalfGapY = std::min({HalfThickness, SplitY, WindowSize.Y - SplitY});
	const float RightX = SplitX + HalfGapX;
	const float BottomY = SplitY + HalfGapY;
	const float LeftWidth = SplitX - HalfGapX;
	const float TopHeight = SplitY - HalfGapY;
	const float RightWidth = WindowSize.X - RightX;
	const float BottomHeight = WindowSize.Y - BottomY;

	ViewRects[0] = {0.0f, 0.0f, LeftWidth, TopHeight};
	ViewRects[1] = {RightX, 0.0f, RightWidth, TopHeight};
	ViewRects[2] = {0.0f, BottomY, LeftWidth, BottomHeight};
	ViewRects[3] = {RightX, BottomY, RightWidth, BottomHeight};
}

// 엔진 FBox의 최소·최대점으로 Native AABB 중심과 반크기를 만든다.
FAABB MakeWorldBounds(const FBox& Value)
{
	const FVector Center = (Value.Min + Value.Max) * 0.5f;
	const FVector Extent = (Value.Max - Value.Min) * 0.5f;
	return {Center, Extent};
}

bool BuildRenderableObject(UPrimitiveComponent* Primitive, FRenderableObject& OutObject)
{
	if (!Primitive || !Primitive->IsVisible())
		return false;

	OutObject = {};
	OutObject.Primitive = Primitive;
	OutObject.WorldMatrix = Primitive->GetWorldMatrix();
	OutObject.WorldBounds = MakeWorldBounds(Primitive->GetWorldBounds());
	OutObject.BoundsRevision = Primitive->GetBoundsRevision();

	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive))
	{
		OutObject.StaticMeshData = StaticMeshComponent->GetMeshData();
		OutObject.bCanBeOccluded = OutObject.StaticMeshData != nullptr && !OutObject.StaticMeshData->Vertices.IsEmpty() && !OutObject.StaticMeshData->Indices.IsEmpty();
		OutObject.bCanOcclude = OutObject.bCanBeOccluded && !OutObject.StaticMeshData->Sections.IsEmpty();
		if (OutObject.bCanOcclude)
		{
			for (const FStaticMeshSection& Section : OutObject.StaticMeshData->Sections)
			{
				UMaterial* Material = StaticMeshComponent->GetMaterial(static_cast<int32>(Section.MaterialSlotIndex));
				if (!Material || Material->PSOType != EPSOType::StaticMesh_Opaque)
				{
					OutObject.bCanOcclude = false;
					break;
				}
			}
		}
	}
	return true;
}

// 양·음 방향 키 상태 차이로 -1~1 축 입력을 만든다.
float AxisValue(const ImGuiKey Positive, const ImGuiKey Negative)
{
	return (ImGui::IsKeyDown(Positive) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(Negative) ? 1.0f : 0.0f);
}

// 길이가 0이면 영벡터를, 아니면 정규화 벡터를 반환한다.
FVector NormalizedOrZero(const FVector Value)
{
	const float LengthSquared = Value.X * Value.X + Value.Y * Value.Y + Value.Z * Value.Z;
	if (LengthSquared <= 0.0f)
		return {};
	const float InverseLength = 1.0f / std::sqrt(LengthSquared);
	return {Value.X * InverseLength, Value.Y * InverseLength, Value.Z * InverseLength};
}

// quaternion으로 회전된 카메라 Forward의 Z 성분을 계산한다.
float CameraForwardZ(const FQuat& Rotation)
{
	return 2.0f * (Rotation.X * Rotation.Z - Rotation.W * Rotation.Y);
}

// +X Forward를 quaternion으로 회전해 카메라 월드 Forward를 구한다.
FVector CameraForward(const FQuat& Rotation)
{
	return {1.0f - 2.0f * (Rotation.Y * Rotation.Y + Rotation.Z * Rotation.Z), 2.0f * (Rotation.X * Rotation.Y + Rotation.W * Rotation.Z), 2.0f * (Rotation.X * Rotation.Z - Rotation.W * Rotation.Y)};
}

// +Y Right를 quaternion으로 회전해 카메라 화면의 수평 월드축을 구한다.
FVector CameraRight(const FQuat& Rotation)
{
	return {2.0f * (Rotation.X * Rotation.Y - Rotation.W * Rotation.Z), 1.0f - 2.0f * (Rotation.X * Rotation.X + Rotation.Z * Rotation.Z), 2.0f * (Rotation.Y * Rotation.Z + Rotation.W * Rotation.X)};
}

// +Z Up을 quaternion으로 회전해 카메라 화면의 수직 월드축을 구한다.
FVector CameraUp(const FQuat& Rotation)
{
	return {2.0f * (Rotation.X * Rotation.Z + Rotation.W * Rotation.Y), 2.0f * (Rotation.Y * Rotation.Z - Rotation.W * Rotation.X), 1.0f - 2.0f * (Rotation.X * Rotation.X + Rotation.Y * Rotation.Y)};
}

// 카메라 Forward의 XY 방향을 atan2로 바꿔 Euler Yaw를 구한다.
float CameraYawDegrees(const FQuat& Rotation)
{
	const FVector Forward = NormalizedOrZero(CameraForward(Rotation));
	return std::atan2(Forward.Y, Forward.X) * 180.0f / Pi;
}

// 카메라 Forward의 Z 성분을 asin해 Euler Pitch를 구한다.
float CameraPitchDegrees(const FQuat& Rotation)
{
	return std::asin(FMath::Clamp(CameraForwardZ(Rotation), -1.0f, 1.0f)) * 180.0f / Pi;
}

// Z-up Yaw와 Right축 Pitch만 합성해 Roll 없는 quaternion을 만든다.
FQuat MakeCameraRotation(const float YawDegrees, const float PitchDegrees)
{
	const float HalfYaw = YawDegrees * Pi / 360.0f;
	const float HalfPitch = PitchDegrees * Pi / 360.0f;
	const float SinYaw = std::sin(HalfYaw);
	const float CosYaw = std::cos(HalfYaw);
	const float SinPitch = std::sin(HalfPitch);
	const float CosPitch = std::cos(HalfPitch);

	// 카메라 입력은 Euler Yaw/Pitch 두 축으로만 계산한다. Core 경계에 전달할 때만
	// quaternion으로 변환한다.
	return {SinPitch * SinYaw, -SinPitch * CosYaw, CosPitch * SinYaw, CosPitch * CosYaw};
}

// 피킹 BVH 순회 중 후보마다 World 정밀 검사를 호출하고 Narrow 시간·횟수를 누적한다.
struct FPickTraceState
{
	UWorld* World = nullptr;
	FHitResult* Hit = nullptr;
	bool bTimeNarrow = false;
	uint64 NarrowCycles = 0;
	int32 NarrowTests = 0;
};

} // namespace

void FPIEViewAdapter::InitializeFromWorld(UWorld& World)
{
	SoftwareOcclusion.ResetScene();

	// 새 월드는 이전 월드와 토폴로지 리비전이 같을 수 있으므로 캡처 캐시를 버리고 전체 캡처를 강제한다.
	// (리비전은 1부터 시작하므로 0은 어떤 월드와도 일치하지 않는다)
	CapturedPrimitiveTopologyRevision = 0;
	RenderObjects.Reset();
	RenderObjectIndexByObjectIndex.Reset();

	UCameraComponent* MainCamera = World.GetMainCamera() ? World.GetMainCamera()->GetCameraComponent() : nullptr;
	assert(MainCamera != nullptr);

	const FCameraProjection Perspective{ECameraProjectionMode::Perspective, MainCamera->GetFieldOfView(), MainCamera->GetOrthoWidth(), MainCamera->GetNearZ(), MainCamera->GetFarZ()};

	// 씬의 메인 카메라 위치와 회전을 반영한다
	ViewCamera = {{MainCamera->GetComponentLocation(), MainCamera->GetRelativeRotationQuat()}, Perspective};
	ViewCamera.Transform.Rotation = MakeCameraRotation(CameraYawDegrees(ViewCamera.Transform.Rotation), CameraPitchDegrees(ViewCamera.Transform.Rotation));

	ConstrainOrthographicWidth(ViewCamera.Projection, ViewRect);

	PreparedView.bValid = false;
}

// PIE View 카메라를 PIE 월드의 MainCamera에 반영한다.
// 게임에서는 월드의 카메라가 곧 플레이어 화면이므로, 이 카메라를 기준으로 하는 빌보드·게임 로직이 PIE 화면과 일치한다.
void FPIEViewAdapter::SyncViewCameraToWorld(UWorld& World) const
{
	UCameraComponent* MainCamera = World.GetMainCamera() ? World.GetMainCamera()->GetCameraComponent() : nullptr;
	if (!MainCamera)
		return;

	const FQuat Rotation = ViewCamera.Transform.Rotation;
	MainCamera->SetRelativeLocation(ViewCamera.Transform.Location);
	MainCamera->SetRelativeRotation(Rotation.ToFRotator());
}

void FPIEViewAdapter::CaptureWorld(UWorld& World)
{
	const auto& Primitives = World.GetWorldPrimitiveComponents();
	const uint64 TopologyRevision = World.GetPrimitiveTopologyRevision();
	bool bNeedsFullCapture = CapturedPrimitiveTopologyRevision != TopologyRevision;

	if (!bNeedsFullCapture)
	{
		bool bChanged = false;
		for (const TWeakObjectPtr<UPrimitiveComponent>& WeakPrimitive : World.GetDirtyRenderPrimitiveComponents())
		{
			UPrimitiveComponent* Primitive = WeakPrimitive.Get();
			if (!Primitive)
				continue;

			const uint32 ObjectIndex = Primitive->GetInternalIndex();
			if (ObjectIndex >= static_cast<uint32>(RenderObjectIndexByObjectIndex.Num()))
			{
				bNeedsFullCapture = true;
				break;
			}

			const int32 RenderIndex = RenderObjectIndexByObjectIndex[ObjectIndex];
			if (RenderIndex < 0 || RenderIndex >= RenderObjects.Num() || RenderObjects[RenderIndex].Primitive != Primitive || !Primitive->IsVisible())
			{
				bNeedsFullCapture = true;
				break;
			}

			if (RenderObjects[RenderIndex].BoundsRevision == Primitive->GetBoundsRevision())
				continue;

			FRenderableObject UpdatedObject{};
			if (!BuildRenderableObject(Primitive, UpdatedObject))
			{
				bNeedsFullCapture = true;
				break;
			}
			UpdatedObject.StableIndex = RenderObjects[RenderIndex].StableIndex;
			RenderObjects[RenderIndex] = std::move(UpdatedObject);
			bChanged = true;
		}

		if (!bNeedsFullCapture)
		{
			World.ClearDirtyRenderPrimitiveComponents();
			if (bChanged)
				SoftwareOcclusion.SynchronizeObjects(RenderObjects);
			return;
		}
	}

	RenderObjects.Reset();
	RenderObjectIndexByObjectIndex.Reset();

	const int32 TotalPrimitives = Primitives.Num();
	if (TotalPrimitives <= 0)
	{
		CapturedPrimitiveTopologyRevision = TopologyRevision;
		World.ClearDirtyRenderPrimitiveComponents();
		SoftwareOcclusion.SynchronizeObjects(RenderObjects);
		return;
	}

	const uint32 NumWorkers = (std::max)(1u, Tasks::FTaskScheduler::Get().GetNumWorkers());
	const int32 ChunkSize = (TotalPrimitives + NumWorkers - 1) / NumWorkers;
	const int32 NumJobs = (TotalPrimitives + ChunkSize - 1) / ChunkSize;

	if (WorkerRenderObjectBuffers.Num() < NumJobs)
	{
		WorkerRenderObjectBuffers.SetNum(NumJobs);
	}
	for (int32 i = 0; i < NumJobs; ++i)
	{
		WorkerRenderObjectBuffers[i].Reset();
	}

	// 컴포넌트 정보 병렬 수집
	Tasks::ParallelFor(TotalPrimitives,
		ChunkSize,
		[&](int32 Start, int32 End)
		{
			const int32 JobIndex = Start / ChunkSize;
			TArray<FRenderableObject>& LocalList = WorkerRenderObjectBuffers[JobIndex];
			LocalList.Reserve(End - Start);
			for (int32 Index = Start; Index < End; ++Index)
			{
				UPrimitiveComponent* Primitive = Primitives[Index].Get();
				if (!Primitive || !Primitive->IsVisible())
				{
					continue;
				}

				FRenderableObject RenderObject{};
				if (BuildRenderableObject(Primitive, RenderObject))
					LocalList.Add(std::move(RenderObject));
			}
		});

	int32 TotalValid = 0;
	for (int32 JobIndex = 0; JobIndex < NumJobs; ++JobIndex)
	{
		TotalValid += WorkerRenderObjectBuffers[JobIndex].Num();
	}
	RenderObjects.Reserve(TotalValid);

	// 수집된 객체 병합 및 순서 식별자 부여
	uint32 StableIndex = 0;
	for (int32 JobIndex = 0; JobIndex < NumJobs; ++JobIndex)
	{
		for (FRenderableObject& Object : WorkerRenderObjectBuffers[JobIndex])
		{
			Object.StableIndex = StableIndex++;
			const uint32 ObjectIndex = Object.Primitive->GetInternalIndex();
			if (ObjectIndex >= static_cast<uint32>(RenderObjectIndexByObjectIndex.Num()))
			{
				const int32 PreviousSize = RenderObjectIndexByObjectIndex.Num();
				RenderObjectIndexByObjectIndex.SetNum(static_cast<int32>(ObjectIndex + 1));
				std::fill(RenderObjectIndexByObjectIndex.begin() + PreviousSize, RenderObjectIndexByObjectIndex.end(), -1);
			}
			RenderObjectIndexByObjectIndex[ObjectIndex] = static_cast<int32>(Object.StableIndex);
			RenderObjects.Add(std::move(Object));
		}
	}

	CapturedPrimitiveTopologyRevision = TopologyRevision;
	World.ClearDirtyRenderPrimitiveComponents();
	SoftwareOcclusion.SynchronizeObjects(RenderObjects);
}

void FPIEViewAdapter::SetViewCamera(int32 ViewIndex, const FViewCamera& Camera)
{
	ViewCamera = Camera;

	ConstrainOrthographicWidth(ViewCamera.Projection, ViewRect);

	if (ViewCamera.Projection.Mode == ECameraProjectionMode::Perspective)
	{
		ViewCamera.Transform.Rotation = MakeCameraRotation(CameraYawDegrees(ViewCamera.Transform.Rotation), CameraPitchDegrees(ViewCamera.Transform.Rotation));
	}

	PreparedView.bValid = false;
}

void FPIEViewAdapter::SetViewRect(int32 ViewIndex, const FRect& Rect)
{
	ViewRect = Rect;

	if (ViewRect.Width > 0.0f && ViewRect.Height > 0.0f)
	{
		ConstrainOrthographicWidth(ViewCamera.Projection, ViewRect);
	}

	PreparedView.bValid = false;
}

void FPIEViewAdapter::SetViewCameraTransform(int32 ViewIndex, const FCameraTransform& CameraTransform)
{
	ViewCamera.Transform = CameraTransform;
}

FMatrix FPIEViewAdapter::GetEngineViewProjection(int32 ViewIndex) const
{

	return PrepareView().EngineViewProjection;
}

FMatrix FPIEViewAdapter::GetEnginePerspectiveProjection() const
{
	return BuildProjectionMatrix(ViewCamera.Projection, ViewRect.Width / ViewRect.Height);
}

FVector FPIEViewAdapter::GetEngineCameraLocation(int32 ViewIndex) const
{
	return ViewCamera.Transform.Location;
}

FVector FPIEViewAdapter::GetEngineCameraForward(int32 ViewIndex) const
{
	return NormalizedOrZero(CameraForward(ViewCamera.Transform.Rotation));
}

FMatrix FPIEViewAdapter::BuildEngineBillboardMatrix(int32 ViewIndex, const FVector& WorldPosition, float Width, float Height) const
{
	if (ViewCamera.Projection.Mode == ECameraProjectionMode::Orthographic)
	{
		const FVector Facing = NormalizedOrZero(CameraForward(ViewCamera.Transform.Rotation)) * -1.0f;
		const FVector Right = NormalizedOrZero(CameraRight(ViewCamera.Transform.Rotation));
		const FVector Up = NormalizedOrZero(CameraUp(ViewCamera.Transform.Rotation));

		FMatrix EngineMatrix;
		EngineMatrix.SetIdentity();
		EngineMatrix.M[0][0] = Facing.X;
		EngineMatrix.M[0][1] = Facing.Y;
		EngineMatrix.M[0][2] = Facing.Z;
		EngineMatrix.M[1][0] = Right.X * Width;
		EngineMatrix.M[1][1] = Right.Y * Width;
		EngineMatrix.M[1][2] = Right.Z * Width;
		EngineMatrix.M[2][0] = Up.X * Height;
		EngineMatrix.M[2][1] = Up.Y * Height;
		EngineMatrix.M[2][2] = Up.Z * Height;
		EngineMatrix.M[3][0] = WorldPosition.X;
		EngineMatrix.M[3][1] = WorldPosition.Y;
		EngineMatrix.M[3][2] = WorldPosition.Z;
		return EngineMatrix;
	}

	const FBillboardTransform Result = ComputeBillboardTransform({WorldPosition, {Width, Height}}, ViewCamera.Transform);
	FMatrix EngineMatrix = Result.WorldMatrix;

	EngineMatrix.M[1][0] = -EngineMatrix.M[1][0];
	EngineMatrix.M[1][1] = -EngineMatrix.M[1][1];
	EngineMatrix.M[1][2] = -EngineMatrix.M[1][2];
	return EngineMatrix;
}

bool FPIEViewAdapter::IsOrthographic(int32 ViewIndex) const
{
	return ViewCamera.Projection.Mode == ECameraProjectionMode::Orthographic;
}

void FPIEViewAdapter::BuildRenderPackets(int32 ViewIndex, TArray<FRenderPacket>& OutPackets)
{
	OutPackets.Reset();
	if (ViewRect.Width <= 0.0f || ViewRect.Height <= 0.0f)
		return;

	const FPreparedView& View = PrepareView();
	const FViewCamera RenderCamera = GetRenderCamera();

	const int32 RenderWidth = (std::max(1, static_cast<int32>(ViewRect.Width)));
	const int32 RenderHeight = (std::max(1, static_cast<int32>(ViewRect.Height)));

	SoftwareOcclusion.Cull(0, RenderObjects, View.Frustum, View.EngineViewProjection, RenderCamera.Transform.Location, RenderWidth, RenderHeight, false, VisiblePrimitives, OcclusionStats);

	const int32 TotalPrimitives = VisiblePrimitives.Num();

	if (TotalPrimitives == 0)
	{
		OcclusionStats.RenderPackets = 0;
		return;
	}

	const uint32 NumWorkers = (std::max)(1u, Tasks::FTaskScheduler::Get().GetNumWorkers());
	const int32 ChunkSize = (TotalPrimitives + NumWorkers - 1) / NumWorkers;
	const int32 NumJobs = (TotalPrimitives + ChunkSize - 1) / ChunkSize;

	if (WorkerPacketBuffers.Num() < NumJobs)
	{
		WorkerPacketBuffers.SetNum(NumJobs);
	}
	for (int32 i = 0; i < NumJobs; ++i)
	{
		WorkerPacketBuffers[i].Reset();
	}

	const TArray<uint8>& VisibleLODs = SoftwareOcclusion.GetVisibleLODs(0);
	const UClass* StaticMeshClass = UStaticMeshComponent::StaticClass();
	const UClass* BillboardClass = UBillboardComponent::StaticClass();

	// Billboard·Particle이 PIE View 카메라를 향하도록 피킹과 같은 행렬 함수를 넘긴다
	FBillboardViewContext BillboardView;
	BillboardView.ViewContext = this;
	BillboardView.CameraLocation = GetEngineCameraLocation(0);
	BillboardView.BuildMatrixFn = [](const void* Context, const FVector& WorldPosition, float Width, float Height) -> FMatrix
	{
		return static_cast<const FPIEViewAdapter*>(Context)->BuildEngineBillboardMatrix(0, WorldPosition, Width, Height);
	};

	Tasks::ParallelFor(TotalPrimitives,
		ChunkSize,
		[&](int32 Start, int32 End)
		{
			const int32 JobIndex = Start / ChunkSize;
			TArray<FRenderPacket>& LocalList = WorkerPacketBuffers[JobIndex];
			LocalList.Reserve((End - Start) * 2);

			for (int32 i = Start; i < End; ++i)
			{
				UPrimitiveComponent* Primitive = VisiblePrimitives[i];
				if (Primitive)
				{
					uint8 TargetLOD = (i < VisibleLODs.Num()) ? VisibleLODs[i] : 0;
					if (Primitive->GetClass() == StaticMeshClass)
					{
						auto* SMC = static_cast<UStaticMeshComponent*>(Primitive);
						if (SMC->GetForcedLOD() >= 0)
						{
							// 강제 단계 지정 적용
							TargetLOD = static_cast<uint8>(SMC->GetForcedLOD());
						}
					}

					const int32 PrevCount = LocalList.Num();
					// 대다수인 StaticMesh는 클래스 비교만으로 빠르게 기본 경로로 보낸다
					if (Primitive->GetClass() != StaticMeshClass && Primitive->IsA(BillboardClass))
					{
						static_cast<UBillboardComponent*>(Primitive)->SubmitToRenderPacketsForView(LocalList, BillboardView);
					}
					else
					{
						Primitive->SubmitToRenderPackets(LocalList);
					}
					if (LocalList.Num() > PrevCount)
					{
						for (int32 p = PrevCount; p < LocalList.Num(); ++p)
						{
							FRenderPacket& Packet = LocalList[p];
							if (Packet.mesh && !Packet.mesh->LODs.IsEmpty())
							{
								const uint8 MaxLOD = static_cast<uint8>(Packet.mesh->LODs.Num() - 1);
								Packet.LODIndex = (std::min)(TargetLOD, MaxLOD);
								const bool bSingleSection = Packet.mesh->GetMeshData().Sections.Num() == 1;

								if (Packet.LODIndex > 0 && bSingleSection)
								{
									Packet.StartIndex = 0;
									Packet.IndexCount = Packet.mesh->GetIndexCount(Packet.LODIndex);
								}
								else if (Packet.LODIndex > 0)
								{
									// 섹션 정보에 따라 단계 설정
									Packet.LODIndex = 0;
								}
							}
							else
							{
								Packet.LODIndex = 0;
							}
						}
					}
				}
			}
		});

	// 패킷 총량 계산 및 일괄 취합
	int32 TotalPacketCount = 0;
	for (int32 j = 0; j < NumJobs; ++j)
	{
		TotalPacketCount += WorkerPacketBuffers[j].Num();
	}

	OutPackets.SetNum(TotalPacketCount, false);
	int32 DstOffset = 0;
	for (int32 j = 0; j < NumJobs; ++j)
	{
		const int32 Count = WorkerPacketBuffers[j].Num();
		if (Count > 0)
		{
			std::memcpy(OutPackets.GetData() + DstOffset, WorkerPacketBuffers[j].GetData(), sizeof(FRenderPacket) * Count);
			DstOffset += Count;
		}
	}

	OcclusionStats.RenderPackets = OutPackets.Num();
}

FViewCamera FPIEViewAdapter::GetRenderCamera() const
{
	FViewCamera Camera = ViewCamera;
	if (Camera.Projection.Mode != ECameraProjectionMode::Orthographic)
		return Camera;

	const FRect& Rect = ViewRect;
	const float AspectRatio = Rect.Width > 0.0f && Rect.Height > 0.0f ? Rect.Width / Rect.Height : 1.0f;
	const float HalfWidth = Camera.Projection.OrthoWidth * 0.5f;
	const float HalfHeight = HalfWidth / AspectRatio;
	const FVector Position = Camera.Transform.Location;
	const float Distance = std::sqrt(Position.X * Position.X + Position.Y * Position.Y + Position.Z * Position.Z);

	// 현재 카메라 평면의 앞뒤를 모두 포함하고 기존 FarClip보다 깊이 범위를 줄이지
	// 않는다.
	const float Radius = std::max(Camera.Projection.FarClip, 2.0f * Distance + 4.0f * std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight));
	const FVector Forward = NormalizedOrZero(CameraForward(Camera.Transform.Rotation));
	const float Retreat = Radius + Camera.Projection.NearClip;
	Camera.Transform.Location = Position - Forward * Retreat;
	Camera.Projection.FarClip = Camera.Projection.NearClip + 2.0f * Radius;
	return Camera;
}

const FPIEViewAdapter::FPreparedView& FPIEViewAdapter::PrepareView() const
{
	const auto& P = ViewCamera.Projection;
	const auto& L = ViewCamera.Transform.Location;
	const auto& R = ViewCamera.Transform.Rotation;
	const auto& Rect = ViewRect;
	const float Key[14]{L.X, L.Y, L.Z, R.X, R.Y, R.Z, R.W, static_cast<float>(P.Mode), P.FovDegrees, P.OrthoWidth, P.NearClip, P.FarClip, Rect.Width, Rect.Height};
	FPreparedView& Cached = PreparedView;
	// 고정 크기 입력 키는 할당 없는 배열로 비교하며 NaN도 매번 변경으로 취급한다.
	bool bKeyChanged = !Cached.bValid;
	for (int Index = 0; Index < 14; ++Index)
		bKeyChanged = bKeyChanged || Cached.Key[Index] != Key[Index];
	if (bKeyChanged)
	{
		const FViewCamera Camera = GetRenderCamera();
		// row-vector는 View 다음 Projection 순서로 합성하고 같은 VP로 컬링한다.
		const FMatrix VP = BuildViewMatrix(Camera.Transform) * BuildProjectionMatrix(Camera.Projection, Rect.Width / Rect.Height);
		Cached.EngineViewProjection = VP;
		Cached.Frustum = ExtractFrustumPlanes(VP);
		for (int Index = 0; Index < 14; ++Index)
			Cached.Key[Index] = Key[Index];
		Cached.bValid = true;
	}
	return Cached;
}

void FPIEViewAdapter::UpdateInput(const float DeltaTime, const FVector2D LocalMousePosition, const float MoveSpeed, const float MouseSensitivity)
{
	if (FInputSystem::IsMouseReleased(EMouseButton::Right))
	{
		// 우클릭 종료시 행동
	}
		
	// 우클릭 시작 위치를 Capture하고, Capture 밖에서는 좌클릭으로만 선택을
	// 바꾼다.

	if (FInputSystem::IsMousePressed(EMouseButton::Right))
	{
		// 우클릭시 행동
	}
	else if (FInputSystem::IsMousePressed(EMouseButton::Left))
	{
		// 좌클릭시 행동
	}

	FCameraMoveInput Input{};

	const float ForwardAxis = AxisValue(ImGuiKey_W, ImGuiKey_S);
	const float RightAxis = AxisValue(ImGuiKey_D, ImGuiKey_A);
	const float VerticalAxis = AxisValue(ImGuiKey_E, ImGuiKey_Q);
	const bool bOrthographic = ViewCamera.Projection.Mode == ECameraProjectionMode::Orthographic;
	// 직교 카메라 이동은 화면 평면으로 제한한다.
	// A/D는 수평, W/S는 수직으로 이동한다. 선택한 축 정렬 View가 뒤집히거나
	// 깊이축을 따라 이동하지 않도록 Q/E와 마우스 회전은 의도적으로 무시한다.
	const FVector Axis = bOrthographic ? NormalizedOrZero({0.0f, RightAxis, ForwardAxis}) : NormalizedOrZero({ForwardAxis, RightAxis, VerticalAxis});
	Input.MoveAxis = {Axis.X * MoveSpeed, Axis.Y * MoveSpeed, Axis.Z * MoveSpeed};

	bool bUpdateEulerRotation = false;
	float UpdatedYawDegrees = 0.0f;
	float UpdatedPitchDegrees = 0.0f;

	const FQuat& CurrentRotation = ViewCamera.Transform.Rotation;
	const float CurrentYawDegrees = CameraYawDegrees(CurrentRotation);
	const float CurrentPitchDegrees = CameraPitchDegrees(CurrentRotation);
	const float YawDeltaDegrees = FMath::Clamp(FPIEViewportPanel::DeltaX * MouseSensitivity, -MaximumRotationDegreesPerFrame, MaximumRotationDegreesPerFrame);
	const float PitchDeltaDegrees = FMath::Clamp(-FPIEViewportPanel::DeltaY * MouseSensitivity, -MaximumRotationDegreesPerFrame, MaximumRotationDegreesPerFrame);
	UpdatedYawDegrees = CurrentYawDegrees + YawDeltaDegrees;
	UpdatedPitchDegrees = FMath::Clamp(CurrentPitchDegrees + PitchDeltaDegrees, -MaximumCameraPitchDegrees, MaximumCameraPitchDegrees);
	bUpdateEulerRotation = true;
	

	FViewCamera UpdatedCamera = ApplyCameraMovement(ViewCamera, Input, DeltaTime);

	if (bUpdateEulerRotation)
		UpdatedCamera.Transform.Rotation = MakeCameraRotation(UpdatedYawDegrees, UpdatedPitchDegrees);
	ViewCamera = UpdatedCamera;
}