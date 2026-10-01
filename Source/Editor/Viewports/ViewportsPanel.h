#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/MultipleViewportsAdapterTypes.h"
#include "Rendering/RenderingInfo.h"
class FMultipleViewportsAdapter;

class FViewportsPanel : public IEditorPanel
{
public:
	// View 표시 모드를 편집할 Adapter를 연결한다. 엔진이 수명을 보장한다.
	void SetViewportAdapter(FMultipleViewportsAdapter* Value) { ViewportAdapter = Value; }
	// 네 View의 렌더 타깃과 UI 제어 상태를 초기화한다.
	bool Init() override;
	// 프레임 입력에서 Splitter Drag와 View별 UI 요청을 수집한다.
	void Tick(float DeltaTime) override;
	// 각 View Texture와 Splitter·Layout·Preset 조작 UI를 ImGui 패널에 그린다.
	void OnRender() override;
	const char* GetPanelName() const override { return "Viewports"; }

	// Core가 계산한 View Rect에 맞춰 해당 Slot의 활성 상태와 렌더 타깃을 갱신한다.
	void SetView(int32 ViewIndex, const FRect& Rect, bool bActive);
	const FRenderingInfo& GetRenderingInfo(int32 ViewIndex) const;

	FVector2 GetContentSize() const { return {ContentSize.x, ContentSize.y}; }
	FVector2 GetLocalMousePosition() const;
	bool IsHovered() const { return bHovered; }

	float ConsumeHorizontalDrag();
	float ConsumeVerticalDrag();
	// Core의 현재 Layout과 View별 Preset을 UI 표시 상태에 동기화한다.
	void SetControlState(ELayoutMode LayoutMode, int32 SingleViewIndex, const EMultipleViewportsCameraPreset CameraPresets[4]);
	// UI에서 발생한 Layout 변경 요청을 한 번 소비하도록 반환한다.
	bool ConsumeLayoutRequest(ELayoutMode& OutMode, int32& OutSingleViewIndex);
	// UI에서 발생한 View별 Camera Preset 요청을 한 번 소비하도록 반환한다.
	bool ConsumeCameraPresetRequest(int32& OutViewIndex, EMultipleViewportsCameraPreset& OutPreset);

private:
	FMultipleViewportsAdapter* ViewportAdapter = nullptr;
	struct FViewSlot
	{
		// View 하나의 Rect·활성 상태·Color/Depth 타깃과 렌더 정보를 담는다.
		FRect Rect{};
		bool bActive = false;
		uint32 Width = 0;
		uint32 Height = 0;
		TUniquePtr<FTexture2D> ColorTarget;
		TUniquePtr<FTexture2D> DepthTarget;
		FRenderingInfo RenderingInfo{};
	};

	// Rect 크기가 바뀐 Slot의 Color/Depth Texture와 렌더 정보를 다시 만든다.
	void ResizeSlot(FViewSlot& Slot, uint32 Width, uint32 Height);
	// 콘솔 stat 명령으로 켜진 항목을 해당 View의 좌상단에 겹쳐 그린다.
	void DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin) const;

	FViewSlot Slots[4]{};
	ImVec2 ContentOrigin{};
	ImVec2 ContentSize{1.0f, 1.0f};
	bool bHovered = false;
	float PendingHorizontalDrag = 0.0f;
	float PendingVerticalDrag = 0.0f;
	// 누른 뒤 커서가 Splitter 영역을 벗어나도 버튼을 놓을 때까지 드래그를 유지한다.
	bool bDraggingVerticalSplitter = false;
	bool bDraggingHorizontalSplitter = false;
	ELayoutMode CurrentLayoutMode = ELayoutMode::QuadSplit;
	int32 CurrentSingleViewIndex = 0;
	EMultipleViewportsCameraPreset CurrentCameraPresets[4]{};
	bool bHasLayoutRequest = false;
	ELayoutMode RequestedLayoutMode = ELayoutMode::QuadSplit;
	int32 RequestedSingleViewIndex = 0;
	int32 PendingCameraPresetViewIndex = InvalidViewIndex;
	EMultipleViewportsCameraPreset PendingCameraPreset = EMultipleViewportsCameraPreset::Perspective;
};
