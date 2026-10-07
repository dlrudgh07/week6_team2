#pragma once

#include "Editor/Viewports/MultipleViewportsAdapterTypes.h"
#include "Editor/Viewports/SoftwareOcclusion.h"

#include "Math/Matrix.h"
#include "Rendering/RenderPacket.h"

#include "../../Runtime/Containers/Array.h"
#include "ViewportAdapter.h"
class UPrimitiveComponent;
class UWorld;

// PIE 단일 View의 카메라·컬링·렌더 패킷 생성을 담당한다.
class FPIEViewAdapter : public IViewportAdapter
{
  public:
	// World를 기준으로 PIE View의 초기 카메라 상태를 구성한다.
	void InitializeFromWorld(UWorld& World) override;

	// PIE View 카메라를 World의 MainCamera에 반영한다. (빌보드 등 MainCamera 기준 계산을 PIE 화면과 일치시킨다)
	void SyncViewCameraToWorld(UWorld& World) const;

	// 현재 World의 렌더링 대상 정보를 갱신한다.
	void CaptureWorld(UWorld& World) override;

	// PIE View의 카메라를 설정한다.
	void SetViewCamera(int32 ViewIndex, const FViewCamera& Camera) override;

	void SetViewRect(int32 ViewIndex, const FRect& Rect) override;

	void SetViewCameraTransform(int32 ViewIndex, const FCameraTransform& CameraTransform) override;

	// 현재 PIE View 카메라를 반환한다.
	const FViewCamera& GetViewCamera(int32 ViewIndex) const override
	{
		return ViewCamera;
	}

	// View 카메라의 ViewProjection을 반환한다.
	FMatrix GetEngineViewProjection(int32 ViewIndex) const override;

	FMatrix GetEnginePerspectiveProjection(const int32 ViewIndex) const override;

	// 카메라 위치를 반환한다.
	FVector GetEngineCameraLocation(int32 ViewIndex) const override;

	// 카메라 Forward를 반환한다.
	FVector GetEngineCameraForward(int32 ViewIndex) const override;

	// Billboard 계산에 사용할 월드 행렬을 생성한다.
	FMatrix BuildEngineBillboardMatrix(int32 ViewIndex, const FVector& WorldPosition, float Width, float Height) const override;

	// 현재 View가 직교 투영인지 반환한다.
	bool IsOrthographic(int32 ViewIndex) const override;

	// 현재 PIE View의 RenderPacket을 생성한다.
	void BuildRenderPackets(int32 ViewIndex, TArray<FRenderPacket>& OutPackets) override;

	// 소프트웨어 Occlusion 상태를 초기화한다.
	void ResetSoftwareOcclusionScene() override
	{
		SoftwareOcclusion.ResetScene();
	}

	void SetSoftwareOcclusionSettings(const FSoftwareOcclusionSettings& Value) override
	{
		SoftwareOcclusion.SetSettings(Value);
	}

	const FSoftwareOcclusionSettings& GetSoftwareOcclusionSettings() const override
	{
		return SoftwareOcclusion.GetSettings();
	}

	void PostRenderOpaque(int32 ViewIndex, FRHITexture2D* SceneDepthTexture) override
	{
		SoftwareOcclusion.PostRenderOpaque(0, SceneDepthTexture);
	}

	void SettleDynamicObjects() override
	{
		SoftwareOcclusion.SettleDynamicObjects();
	}

	void UpdateInput(const float DeltaTime, const FVector2D LocalMousePosition, const float MoveSpeed, const float MouseSensitivity);
	
	const FRect& GetViewRect(int32 ViewIndex = 0) const override { return ViewRect; }

	bool IsViewActive(int32 ViewIndex) const override { return true; }
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