#pragma once

#include "Editor/Viewports/MultipleViewportsAdapterTypes.h"
#include "Editor/Viewports/SoftwareOcclusion.h"

#include "Math/Matrix.h"
#include "Rendering/RenderPacket.h"

#include "../../Runtime/Containers/Array.h"

class UPrimitiveComponent;
class UWorld;

// PIE 단일 View의 카메라·컬링·렌더 패킷 생성을 담당한다.
class FPIEViewAdapter
{
  public:
	// World를 기준으로 PIE View의 초기 카메라 상태를 구성한다.
	void InitializeFromWorld(UWorld& World);

	// 현재 World의 렌더링 대상 정보를 갱신한다.
	void CaptureWorld(UWorld& World);

	// PIE View의 카메라를 설정한다.
	void SetViewCamera(const FViewCamera& Camera);

	void SetViewRect(const FRect& Rect);

	// 현재 PIE View 카메라를 반환한다.
	const FViewCamera& GetViewCamera() const
	{
		return ViewCamera;
	}

	// View 카메라의 ViewProjection을 반환한다.
	FMatrix GetEngineViewProjection() const;

	// 카메라 위치를 반환한다.
	FVector GetEngineCameraLocation() const;

	// 카메라 Forward를 반환한다.
	FVector GetEngineCameraForward() const;

	// Billboard 계산에 사용할 월드 행렬을 생성한다.
	FMatrix BuildEngineBillboardMatrix(const FVector& WorldPosition, float Width, float Height) const;

	// 현재 View가 직교 투영인지 반환한다.
	bool IsOrthographic() const;

	// 현재 PIE View의 RenderPacket을 생성한다.
	void BuildRenderPackets(TArray<FRenderPacket>& OutPackets);

	// 소프트웨어 Occlusion 상태를 초기화한다.
	void ResetSoftwareOcclusionScene()
	{
		SoftwareOcclusion.ResetScene();
	}

	void SetSoftwareOcclusionSettings(const FSoftwareOcclusionSettings& Value)
	{
		SoftwareOcclusion.SetSettings(Value);
	}

	const FSoftwareOcclusionSettings& GetSoftwareOcclusionSettings() const
	{
		return SoftwareOcclusion.GetSettings();
	}

	void PostRenderOpaque(FRHITexture2D* SceneDepthTexture)
	{
		SoftwareOcclusion.PostRenderOpaque(0, SceneDepthTexture);
	}

	void SettleDynamicObjects()
	{
		SoftwareOcclusion.SettleDynamicObjects();
	}

	void UpdateInput(const float DeltaTime, const FVector2D LocalMousePosition, const float MoveSpeed, const float MouseSensitivity);

  private:
	// 직교 View의 렌더·컬링에 사용할 카메라를 계산한다.
	FViewCamera GetRenderCamera() const;

	// 카메라·투영·화면 크기를 기준으로 파생 렌더 정보를 캐시한다.
	struct FPreparedView
	{
		float Key[14]{};
		FFrustumPlanes Frustum{};
		FMatrix EngineViewProjection{};
		bool bValid = false;
	};

	mutable FPreparedView PreparedView{};

	FRect ViewRect{};

	FSoftwareOcclusionStats OcclusionStats{};

	// 카메라가 변경되지 않았다면 기존 View 정보를 재사용한다.
	const FPreparedView& PrepareView() const;

	FViewCamera ViewCamera{};

	// PIE View의 렌더링에 필요한 월드 데이터.
	TArray<FRenderableObject> RenderObjects;
	TArray<TArray<FRenderableObject>> WorkerRenderObjectBuffers;
	TArray<int32> RenderObjectIndexByObjectIndex;
	uint64 CapturedPrimitiveTopologyRevision = 0;

	TArray<TArray<FRenderPacket>> WorkerPacketBuffers;

	// 불투명 파티클 정렬용 데이터.
	TArray<FParticleSortInput> SortInputs;
	TArray<ObjectId> SortedParticleIds;

	// 컬링된 Primitive 목록.
	TArray<UPrimitiveComponent*> VisiblePrimitives;

	FSoftwareOcclusionCuller SoftwareOcclusion;
};