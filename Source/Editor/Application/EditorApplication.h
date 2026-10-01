#pragma once

#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Types.h"

#include "World/World.h"
#include "Rendering/Renderer.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/Swapchain.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Editor/Rendering/GridRenderer.h"
#include "Editor/Gizmo/GizmoRenderer.h"
#include "Rendering/LineBatcher.h"

#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/LoadingScreen.h"
#include "Editor/Viewports/MultipleViewportsAdapter.h"

#include "Editor/Rendering/Outline.h"
#include "Editor/Rendering/OutlineRenderer.h"

#include "Rendering/SkyboxRenderer.h"

//Temp
#include "Text/Font.h"
#include "Text/TextRenderer.h"

class FOutputLogPanel;
class FDetailsPanel;
class FEditorControlsPanel;
class FSettingsPanel;
class FViewportsPanel;
class FContentDrawerPanel;
class FOutlinerPanel;
class FLoadingScreen;

struct FWindowContext
{
	// 창 하나와 그 창에 연결된 Swapchain의 소유권을 함께 담는다.
	TUniquePtr<FWindow> Window;
	TUniquePtr<FSwapchain> Swapchain;
};

class FEditorApplication : public FApplication
{
public:
	bool Init(HINSTANCE hInstance) override;
	void Run() override;
	void Shutdown() override;

	// Active View의 입력과 Picking 결과만 Gizmo 및 선택 상태에 반영한다.
	void UpdateGizmoAndPicking();
	// View 하나의 Scene·Grid·Gizmo·텍스트를 해당 ViewProjection으로 렌더한다.
	void RenderFrame(int32 ViewIndex, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward, TArray<FRenderPacket>& RenderPackets);
	// 네 View 결과와 ImGui를 메인 Swapchain에 합성해 화면에 표시한다.
	void PresentFrame();
	void DeleteActor(AActor* Actor);

	//윈도우 크기 변경 처리
	void HandleMainWindow();

private:
	// 입력과 창 이벤트를 처리하고 이번 프레임 DeltaTime을 계산한다.
	bool BeginFrame(float& OutDeltaTime);
	// 패널 요청과 입력을 Core Adapter에 전달해 레이아웃·카메라 상태를 갱신한다.
	void UpdateMultipleViewportState(float DeltaTime);
	// 월드를 정확히 한 번 Tick·Capture한 뒤 에디터 상호작용을 갱신한다.
	void TickWorldAndEditor(float DeltaTime);
	// 한 번 캡처한 월드 결과를 재사용해 현재 레이아웃의 각 View를 렌더한다.
	void RenderMultipleViewports();
	// 화면 합성과 View 설정 보관으로 프레임을 마무리한다.
	void EndFrame();

	bool bIsRunning = false;
	bool bIsResized = false;

	TUniquePtr<FRenderDevice> RenderDevice;

	TArray<FWindowContext> Windows;
	FWindow* MainWindow;
	FSwapchain* MainWindowSC;

	UWorld* World;

	TUniquePtr<FEditorUI> EditorUI;

	TUniquePtr<FRenderer> Renderer;
	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
	TUniquePtr<FGridRenderer> GridRenderer;
	TUniquePtr<FGizmoRenderer> GizmoRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	TUniquePtr<FLineBatcher> LineBatcher;
	TUniquePtr<FGizmo> Gizmo;
	TUniquePtr<FOutline> Outline;
	TUniquePtr<FOutlineRenderer> OutlineRenderer;
	TUniquePtr<FLoadingScreen> LoadingScreen;


	

	UFont* SystemFont;

	// UE_LOG 매크로용 전역 콘솔
	FOutputLogPanel* OutputLogPanel = nullptr;

	FDetailsPanel* DetailsPanel = nullptr;
	FEditorControlsPanel* EditorControlsPanel = nullptr;
	FSettingsPanel* SettingsPanel = nullptr;
	FViewportsPanel* ViewportsPanel = nullptr;
	FMultipleViewportsAdapter MultipleViewportsAdapter;
	// 프레임마다 Reset해 기존 capacity를 재사용하는 연속 RenderPacket 버퍼.
	TArray<FRenderPacket> SceneRenderPackets;
	FOutlinerPanel* OutlinerPanel = nullptr;
	FContentDrawerPanel* ContentDrawerPanel = nullptr;

	void ResetSceneSelection();

	void CreateNewScene();
	void OpenScene();
	void OpenCompetitionScene();
	void SaveCurrentScene();
	void SaveSceneAs();
};
