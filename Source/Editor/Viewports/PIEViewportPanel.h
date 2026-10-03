#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Rendering/RenderingInfo.h"
#include "Editor/Viewports/MultipleViewportsAdapterTypes.h"
#include "../../Runtime/Engine/World.h"
class FPIEViewportPanel : public IEditorPanel
{
public:
	// 네 View의 렌더 타깃과 UI 제어 상태를 초기화한다.
	bool Init() override;
	// 프레임 입력에서 Splitter Drag와 View별 UI 요청을 수집한다.
	void Tick(float DeltaTime) override;
	// 각 View Texture와 Splitter·Layout·Preset 조작 UI를 ImGui 패널에 그린다.
	void OnRender() override;
	void SetView(const FRect& Rect);
	const char* GetPanelName() const override { return "PIEViewports"; }


	const FRenderingInfo& GetRenderingInfo() const;

	FVector2D GetContentSize() const { return {ContentSize.x, ContentSize.y}; }
	FVector2D GetLocalMousePosition() const;
	bool IsHovered() const { return bHovered; }
	bool IsActive() const { return bActive; }
	void SetActive(bool bInActive){ bActive = bInActive; }
	static float DeltaX;
	static float DeltaY;
private:
	FRect Rect{};
	bool bActive = false;
	uint32 Width = 0;
	uint32 Height = 0;
	TUniquePtr<FRHITexture2D> ColorTarget;
	TUniquePtr<FRHITexture2D> DepthTarget;
	FRenderingInfo RenderingInfo{};

	void Resize(uint32 NewWidth, uint32 NewHeight);
	void DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin) const;

	ImVec2 ContentOrigin{};
	ImVec2 ContentSize{1.0f, 1.0f};
	bool bHovered = false;
	bool bFocus = false;

};
