#pragma once

#include "Editor/Viewports/MultipleViewportsAdapterTypes.h"
#include "Editor/Viewports/SoftwareOcclusion.h"

#include "Math/Matrix.h"
#include "Rendering/RenderPacket.h"

class UWorld;

class IViewportAdapter
{
  public:
	virtual ~IViewportAdapter() = default;
	virtual void InitializeFromWorld(UWorld& World) = 0;
	virtual void CaptureWorld(UWorld& World) = 0;
	virtual void SetViewCamera(int32 ViewIndex, const FViewCamera& Camera) = 0;
	virtual const FViewCamera& GetViewCamera(int32 ViewIndex = 0) const = 0;
	virtual void SetViewRect(int32 ViewIndex, const FRect& Rect) = 0;
	virtual const FRect& GetViewRect(int32 ViewIndex = 0) const = 0;
	virtual FMatrix GetEngineViewProjection(int32 ViewIndex = 0) const = 0;
	virtual FMatrix GetEnginePerspectiveProjection() const = 0;
	virtual FVector GetEngineCameraLocation(int32 ViewIndex = 0) const = 0;
	virtual FVector GetEngineCameraForward(int32 ViewIndex = 0) const = 0;
	virtual FMatrix BuildEngineBillboardMatrix(int32 ViewIndex, const FVector& WorldPosition, float Width, float Height) const = 0;
	virtual bool IsOrthographic(int32 ViewIndex = 0) const = 0;
	//virtual int32 GetViewCount() const = 0;
	virtual bool IsViewActive(int32 ViewIndex) const = 0;
	virtual void UpdateInput(float DeltaTime, FVector2D LocalMousePosition, float MoveSpeed, float MouseSensitivity) = 0;
	virtual void BuildRenderPackets(int32 ViewIndex, TArray<FRenderPacket>& OutPackets) = 0;
	virtual void ResetSoftwareOcclusionScene() = 0;
	virtual void SetSoftwareOcclusionSettings(const FSoftwareOcclusionSettings& Value) = 0;
	virtual const FSoftwareOcclusionSettings& GetSoftwareOcclusionSettings() const = 0;
	virtual void PostRenderOpaque(int32 ViewIndex, FRHITexture2D* SceneDepthTexture) = 0;
	virtual void SettleDynamicObjects() = 0;
	virtual int32 GetEditorViewIndex() const
	{
		return 0;
	}

	// View 표시 모드. PIE에서는 아무 동작도 하지 않는다.
	virtual void SetViewWireframe(int32 Index, bool Value)
	{
		(void)Index;
		(void)Value;
	}

	virtual bool IsViewWireframe(int32 Index) const
	{
		(void)Index;
		return false;
	}

	virtual void SetViewSceneDepthMode(int32 Index, bool Value)
	{
		(void)Index;
		(void)Value;
	}

	virtual bool IsViewSceneDepthMode(int32 Index) const
	{
		(void)Index;
		return false;
	}
};