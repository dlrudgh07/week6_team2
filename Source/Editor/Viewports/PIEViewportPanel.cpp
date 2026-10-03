#include "EnginePCH.h"
#include "PIEViewportPanel.h"

#include "Rendering/RenderCommand.h"

#include <algorithm>
#include <cassert>
#include <format>
#include "Stats/StatOverlay.h"
#include "Input/InputSystem.h"
// 네 View의 렌더 타깃을 최소 크기로 초기화한다.
float FPIEViewportPanel::DeltaX = 0.0f;
float FPIEViewportPanel::DeltaY = 0.0f;
bool FPIEViewportPanel::bFocus = false;
bool FPIEViewportPanel::Init()
{
	Resize(1, 1);
	return true;
}

// 패널의 프레임 갱신 인터페이스이며 별도 계산은 하지 않는다.
void FPIEViewportPanel::Tick(float DeltaTime)
{
	(void)DeltaTime;
}

void FPIEViewportPanel::OnRender()
{
	if (!bActive)
		return;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});

	ImGui::Begin("PIE", nullptr, ImGuiWindowFlags_NoScrollbar);

	ImGui::BeginChild("PIEViewport", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove);

	ContentOrigin = ImGui::GetCursorScreenPos();
	ContentSize = ImGui::GetContentRegionAvail();

	ContentSize.x = std::max(1.0f, ContentSize.x);
	ContentSize.y = std::max(1.0f, ContentSize.y);

	bHovered = ImGui::IsWindowHovered();
	ImGuiIO& io = ImGui::GetIO();
	// 창이 활성화되어 있고, 사용자가 이 창을 클릭했을 때 가두기

	int centerX = ContentOrigin.x + Width / 2.0f;
	int centerY = ContentOrigin.y + Height / 2.0f;

	if (ImGui::IsMouseClicked(0) && bHovered)
	{
		RECT rect = {ContentOrigin.x + Rect.X, ContentOrigin.y + Rect.Y, ContentOrigin.x + Rect.X + Rect.Width, ContentOrigin.y + Rect.Y + Rect.Height};
		
		ShowCursor(FALSE);
		bFocus = true;

		SetCursorPos(centerX, centerY);

	}

	if (bFocus)
	{
		POINT currentPos;
		GetCursorPos(&currentPos);

		DeltaX = currentPos.x - centerX;
		DeltaY = currentPos.y - centerY;

		SetCursorPos(centerX, centerY);
	}

	// 특정 조건(예: F8 키)이나 창이 포커스를 잃으면 해제
	if (ImGui::IsKeyPressed(ImGuiKey_F8))
	{
		ClipCursor(NULL); // 제한 해제
		ShowCursor(TRUE);
		bFocus = false;
		DeltaX = 0;
		DeltaY = 0;
	}

	ImGui::Dummy(ContentSize);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	DrawList->PushClipRect(ContentOrigin, {ContentOrigin.x + ContentSize.x, ContentOrigin.y + ContentSize.y}, true);

	if (bActive && ColorTarget)
	{
		const ImVec2 ViewMin = ContentOrigin;
		const ImVec2 ViewMax = {ContentOrigin.x + ContentSize.x, ContentOrigin.y + ContentSize.y};

		DrawList->AddImage(ColorTarget->GetSRV(), ViewMin, ViewMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 255));

		DrawStatOverlay(DrawList, ViewMin);
	}

	DrawList->PopClipRect();
	ImGui::EndChild();
	ImGui::End();
	ImGui::PopStyleVar();
}

// Core에서 전달된 PIE View Rect와 활성 상태를 반영하고,
// 크기가 변경되면 렌더 타깃을 다시 만든다.
void FPIEViewportPanel::SetView(const FRect& InRect)
{
	Rect = InRect;
	bool bInActive = Rect.Width > 0.0f && Rect.Height > 0.0f;

	if (!bInActive)
	{
		bActive = false;
		return;
	}

	const uint32 NewWidth = static_cast<uint32>(std::max(1.0f, Rect.Width));

	const uint32 NewHeight = static_cast<uint32>(std::max(1.0f, Rect.Height));

	if (NewWidth != Width || NewHeight != Height)
	{
		Resize(NewWidth, NewHeight);
	}
}

const FRenderingInfo& FPIEViewportPanel::GetRenderingInfo() const
{
	return RenderingInfo;
}

FVector2D FPIEViewportPanel::GetLocalMousePosition() const
{
	const ImVec2 Mouse = ImGui::GetMousePos();

	return {Mouse.x - ContentOrigin.x, Mouse.y - ContentOrigin.y};
}

void FPIEViewportPanel::Resize(uint32 NewWidth, uint32 NewHeight)
{
	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = NewWidth;
	Desc.Height = NewHeight;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ColorTarget = FRenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	DepthTarget = FRenderCommand::CreateTexture2D(Desc);

	Width = NewWidth;
	Height = NewHeight;

	RenderingInfo.ColorRenderTargets.Reset();

	RenderingInfo.ViewportSetting.Width = Width;
	RenderingInfo.ViewportSetting.Height = Height;

	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = ColorTarget.get();

	RenderingInfo.ColorRenderTargets.Add(ColorDesc);

	RenderingInfo.DepthSteincil.Texture = DepthTarget.get();
}

void FPIEViewportPanel::DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin) const
{
	if (!DrawList || !FStatOverlay::IsAnyEnabled()) return;
}
