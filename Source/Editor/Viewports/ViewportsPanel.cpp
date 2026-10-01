#include "EnginePCH.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/Viewports/MultipleViewportsAdapter.h"

#include "Core/StatOverlay.h"
#include "Core/Stats.h"
#include "Core/StatDefinitions.h"
#include "Rendering/RenderCommand.h"

#include <algorithm>
#include <cassert>
#include <format>

namespace
{
constexpr ImU32 SplitterColor = IM_COL32(55, 55, 55, 230);
constexpr ImU32 SplitterHoverColor = IM_COL32(255, 192, 0, 255);
constexpr const char* CameraPresetLabels[] = {
	"Perspective", "Ortho (Current)", "Top", "Bottom", "Front", "Back", "Left", "Right"};

// Stat Overlay
constexpr float StatOverlayMargin = 8.0f;
constexpr float StatOverlayPadding = 6.0f;
constexpr ImU32 StatOverlayBackgroundColor = IM_COL32(0, 0, 0, 140);
constexpr ImU32 TitleColor = IM_COL32(255, 210, 60, 255);
constexpr ImU32 ValueColor = IM_COL32(235, 235, 235, 255);
}

// 네 View의 렌더 타깃을 최소 크기로 초기화한다.
bool FViewportsPanel::Init()
{
	for (FViewSlot& Slot : Slots)
		ResizeSlot(Slot, 1, 1);
	return true;
}

// 패널의 프레임 갱신 인터페이스이며 별도 계산은 하지 않는다.
void FViewportsPanel::Tick(float DeltaTime)
{
	(void)DeltaTime;
}

// Core Rect에 맞춰 View 활성 상태와 타깃 크기를 갱신한다.
void FViewportsPanel::SetView(const int32 ViewIndex, const FRect& Rect, const bool bActive)
{
	assert(ViewIndex >= 0 && ViewIndex < 4);
	FViewSlot& Slot = Slots[ViewIndex];
	Slot.Rect = Rect;
	Slot.bActive = bActive && Rect.Width > 0.0f && Rect.Height > 0.0f;
	if (!Slot.bActive)
		return;

	const uint32 Width = static_cast<uint32>(std::max(1.0f, Rect.Width));
	const uint32 Height = static_cast<uint32>(std::max(1.0f, Rect.Height));
	if (Width != Slot.Width || Height != Slot.Height)
		ResizeSlot(Slot, Width, Height);
}

// 인덱스를 검사해 해당 View의 렌더 정보를 반환한다.
const FRenderingInfo& FViewportsPanel::GetRenderingInfo(const int32 ViewIndex) const
{
	assert(ViewIndex >= 0 && ViewIndex < 4);
	return Slots[ViewIndex].RenderingInfo;
}

// 마우스 위치에서 패널 원점을 빼 로컬 좌표로 바꾼다.
FVector2 FViewportsPanel::GetLocalMousePosition() const
{
	const ImVec2 Mouse = ImGui::GetMousePos();
	return {Mouse.x - ContentOrigin.x, Mouse.y - ContentOrigin.y};
}

// 누적 가로 Splitter 이동량을 반환하고 초기화한다.
float FViewportsPanel::ConsumeHorizontalDrag()
{
	const float Result = PendingHorizontalDrag;
	PendingHorizontalDrag = 0.0f;
	return Result;
}

// 누적 세로 Splitter 이동량을 반환하고 초기화한다.
float FViewportsPanel::ConsumeVerticalDrag()
{
	const float Result = PendingVerticalDrag;
	PendingVerticalDrag = 0.0f;
	return Result;
}

// Layout·Single 대상·Preset을 UI 표시와 동기화한다.
void FViewportsPanel::SetControlState(const ELayoutMode LayoutMode, const int32 SingleViewIndex, const EMultipleViewportsCameraPreset CameraPresets[4])
{
	CurrentLayoutMode = LayoutMode;
	CurrentSingleViewIndex = SingleViewIndex;
	for (int32 Index = 0; Index < 4; ++Index)
		CurrentCameraPresets[Index] = CameraPresets[Index];
}

// 대기 Layout 요청을 한 번 반환하고 플래그를 지운다.
bool FViewportsPanel::ConsumeLayoutRequest(ELayoutMode& OutMode, int32& OutSingleViewIndex)
{
	if (!bHasLayoutRequest)
		return false;
	OutMode = RequestedLayoutMode;
	OutSingleViewIndex = RequestedSingleViewIndex;
	bHasLayoutRequest = false;
	return true;
}

// 대기 Preset 요청을 한 번 반환하고 지운다.
bool FViewportsPanel::ConsumeCameraPresetRequest(int32& OutViewIndex, EMultipleViewportsCameraPreset& OutPreset)
{
	if (PendingCameraPresetViewIndex == InvalidViewIndex)
		return false;
	OutViewIndex = PendingCameraPresetViewIndex;
	OutPreset = PendingCameraPreset;
	PendingCameraPresetViewIndex = InvalidViewIndex;
	return true;
}

// View Texture와 Splitter·Layout·Preset UI를 그리고 요청을 기록한다.
void FViewportsPanel::OnRender()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
	ImGui::Begin("Viewports", nullptr,
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoTitleBar);

	ContentOrigin = ImGui::GetCursorScreenPos();
	ContentSize = ImGui::GetContentRegionAvail();
	ContentSize.x = std::max(1.0f, ContentSize.x);
	ContentSize.y = std::max(1.0f, ContentSize.y);
	bHovered = ImGui::IsWindowHovered();

	// 전체 캔버스를 한 번 확보한 뒤 각 렌더 타깃을 창 DrawList에 직접 그린다.
	// Image 항목 네 개를 따로 배치하면 ImGui 레이아웃과 클리핑 상태가 삽입 순서에
	// 영향을 받아, Core가 올바른 사각형을 줘도 아래쪽 행이 잘릴 수 있다.
	ImGui::Dummy(ContentSize);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(ContentOrigin,
		{ContentOrigin.x + ContentSize.x, ContentOrigin.y + ContentSize.y}, true);
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		const FViewSlot& Slot = Slots[ViewIndex];
		if (!Slot.bActive || !Slot.ColorTarget)
			continue;

		const ImVec2 ViewMin{ContentOrigin.x + Slot.Rect.X, ContentOrigin.y + Slot.Rect.Y};
		const ImVec2 ViewMax{ViewMin.x + Slot.Rect.Width, ViewMin.y + Slot.Rect.Height};
		DrawList->AddImage(Slot.ColorTarget->GetSRV(), ViewMin, ViewMax);
	}
	DrawList->PopClipRect();

	if (Slots[1].bActive || Slots[2].bActive || Slots[3].bActive)
	{
		// View Rect 사이에 비워 둔 gutter의 중앙에 Splitter 버튼을 배치한다.
		const float SplitX = (Slots[0].Rect.X + Slots[0].Rect.Width + Slots[1].Rect.X) * 0.5f;
		const float SplitY = (Slots[0].Rect.Y + Slots[0].Rect.Height + Slots[2].Rect.Y) * 0.5f;

		const ImVec2 VerticalMin{ContentOrigin.x + SplitX - SplitterThickness * 0.5f, ContentOrigin.y};
		const ImVec2 VerticalMax{VerticalMin.x + SplitterThickness, ContentOrigin.y + ContentSize.y};
		const ImVec2 HorizontalMin{ContentOrigin.x, ContentOrigin.y + SplitY - SplitterThickness * 0.5f};
		const ImVec2 HorizontalMax{ContentOrigin.x + ContentSize.x, HorizontalMin.y + SplitterThickness};

		ImGui::SetCursorScreenPos(VerticalMin);
		ImGui::InvisibleButton("##MultipleViewportsHorizontalSplitter", {SplitterThickness, ContentSize.y});
		ImGui::SetCursorScreenPos(HorizontalMin);
		ImGui::InvisibleButton("##MultipleViewportsVerticalSplitter", {ContentSize.x, SplitterThickness});

		const bool bVerticalHovered = bHovered && ImGui::IsMouseHoveringRect(VerticalMin, VerticalMax);
		const bool bHorizontalHovered = bHovered && ImGui::IsMouseHoveringRect(HorizontalMin, HorizontalMax);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			bDraggingVerticalSplitter = bVerticalHovered;
			bDraggingHorizontalSplitter = bHorizontalHovered;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			bDraggingVerticalSplitter = false;
			bDraggingHorizontalSplitter = false;
		}

		const bool bVerticalHighlighted = bDraggingVerticalSplitter || bVerticalHovered;
		const bool bHorizontalHighlighted = bDraggingHorizontalSplitter || bHorizontalHovered;
		DrawList->AddRectFilled(VerticalMin, VerticalMax,
			bVerticalHighlighted ? SplitterHoverColor : SplitterColor);
		DrawList->AddRectFilled(HorizontalMin, HorizontalMax,
			bHorizontalHighlighted ? SplitterHoverColor : SplitterColor);

		if (bDraggingVerticalSplitter)
			PendingHorizontalDrag += ImGui::GetIO().MouseDelta.x;
		if (bDraggingHorizontalSplitter)
			PendingVerticalDrag += ImGui::GetIO().MouseDelta.y;

		if (bVerticalHighlighted && bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		else if (bVerticalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		else if (bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}
	else
	{
		bDraggingVerticalSplitter = false;
		bDraggingHorizontalSplitter = false;
	}

	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		if (!Slots[ViewIndex].bActive)
			continue;
		ImGui::SetCursorScreenPos({
			ContentOrigin.x + Slots[ViewIndex].Rect.X + 8.0f,
			ContentOrigin.y + Slots[ViewIndex].Rect.Y + 8.0f});
		ImGui::PushID(100 + ViewIndex);
		int SelectedPreset = static_cast<int>(CurrentCameraPresets[ViewIndex]);
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::Combo("##CameraPreset", &SelectedPreset, CameraPresetLabels, IM_ARRAYSIZE(CameraPresetLabels)))
		{
			PendingCameraPresetViewIndex = ViewIndex;
			PendingCameraPreset = static_cast<EMultipleViewportsCameraPreset>(SelectedPreset);
		}
		ImGui::SameLine();
		// 레이아웃과 독립적으로 각 View의 장면 Fill Mode를 편집한다.
        if (ViewportAdapter)
        {
            int Mode = ViewportAdapter->IsViewWireframe(ViewIndex) ? 1 : 0;
            const char* Labels[] = {"Solid", "Wireframe"};
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::Combo("##FillMode", &Mode, Labels, 2))
                ViewportAdapter->SetViewWireframe(ViewIndex, Mode == 1);
            ImGui::SameLine();
        }
        if (CurrentLayoutMode == ELayoutMode::QuadSplit)
		{
			if (ImGui::SmallButton("Single"))
			{
				RequestedLayoutMode = ELayoutMode::Single;
				RequestedSingleViewIndex = ViewIndex;
				bHasLayoutRequest = true;
			}
		}
		else if (ViewIndex == CurrentSingleViewIndex && ImGui::SmallButton("Quad"))
		{
			RequestedLayoutMode = ELayoutMode::QuadSplit;
			RequestedSingleViewIndex = ViewIndex;
			bHasLayoutRequest = true;
		}
		ImGui::PopID();
	}

	// 마지막으로 선택된 뷰포트만 오버레이
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		if (ViewIndex != ViewportAdapter->GetEditorViewIndex()) continue;
		if (!Slots[ViewIndex].bActive)
			continue;
		DrawStatOverlay(DrawList, {
			ContentOrigin.x + Slots[ViewIndex].Rect.X,
			ContentOrigin.y + Slots[ViewIndex].Rect.Y});
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

// 항목별 줄을 모아 한 번에 배경과 텍스트를 그린다.
void FViewportsPanel::DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin) const
{
	if (!DrawList || !FStatOverlay::IsAnyEnabled())
		return;


	// 제목은 UE처럼 노란색, 값은 흰색으로 구분한다.
	struct FStatLine { FString Text; ImU32 Color; };
	TArray<FStatLine> Lines;

	if (FStatOverlay::IsEnabled(EStatFlags::FPS))
	{
		Lines.Add({"Frame", TitleColor});
		Lines.Add({std::format("  Resolution {} x {}", FStatOverlay::GetScreenWidth(), FStatOverlay::GetScreenHeight()), ValueColor});
		Lines.Add({std::format("  {:.0f} fps", FStatOverlay::GetFPS()), ValueColor});
		Lines.Add({std::format("  {:.2f} ms", FStatOverlay::GetFrameTimeMs()), ValueColor});
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Picking))
	{
		const FStatRecord& Picking = FStats::GetRecord(StatIds::PickingTotal());

		Lines.Add({"Picking", TitleColor});
		Lines.Add({std::format("  Last Pick {:.3f} ms", Picking.CurrentValue), ValueColor});
		Lines.Add({std::format("  Num Attempts {}", Picking.SampleCount), ValueColor});
		Lines.Add({std::format("  Accumulated Time {:.4f} ms", Picking.TotalValue), ValueColor});
	}

	if (Lines.Num() == 0)
		return;

	// 제어 위젯 한 줄 아래에서 시작해 Combo와 겹치지 않게 한다.
	const float LineHeight = ImGui::GetTextLineHeight();
	const ImVec2 Origin{
		ViewMin.x + StatOverlayMargin,
		ViewMin.y + StatOverlayMargin + ImGui::GetFrameHeight() + StatOverlayMargin};

	float MaxWidth = 0.0f;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
		MaxWidth = std::max(MaxWidth, ImGui::CalcTextSize(Lines[Index].Text.c_str()).x);

	const ImVec2 BackgroundMin{Origin.x - StatOverlayPadding, Origin.y - StatOverlayPadding};
	const ImVec2 BackgroundMax{
		Origin.x + MaxWidth + StatOverlayPadding,
		Origin.y + LineHeight * static_cast<float>(Lines.Num()) + StatOverlayPadding};
	DrawList->AddRectFilled(BackgroundMin, BackgroundMax, StatOverlayBackgroundColor, 4.0f);

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		DrawList->AddText(
			{Origin.x, Origin.y + LineHeight * static_cast<float>(Index)},
			Lines[Index].Color,
			Lines[Index].Text.c_str());
	}
}

// View 크기에 맞춰 Color·Depth Texture와 렌더 정보를 재생성한다.
void FViewportsPanel::ResizeSlot(FViewSlot& Slot, const uint32 Width, const uint32 Height)
{
	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	Slot.ColorTarget = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	Slot.DepthTarget = RenderCommand::CreateTexture2D(Desc);

	Slot.Width = Width;
	Slot.Height = Height;
	Slot.RenderingInfo.ColorRenderTargets.Reset();
	Slot.RenderingInfo.ViewportSetting.Width = Width;
	Slot.RenderingInfo.ViewportSetting.Height = Height;
	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = Slot.ColorTarget.get();
	Slot.RenderingInfo.ColorRenderTargets.Add(ColorDesc);
	Slot.RenderingInfo.DepthSteincil.Texture = Slot.DepthTarget.get();
}
