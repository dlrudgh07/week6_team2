#include "EnginePCH.h"

#include "Editor/Application/EditorApplication.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/OutputLog/OutputLogPanel.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/Viewports/PIEViewportPanel.h"

#include "Core/EngineStatics.h"
#include "Misc/App.h"
#include "Stats/StatOverlay.h"
#include "Input/InputSystem.h"

#include "UObject/UObjectGlobals.h"

#include "Rendering/GeometryGenerator.h"

#include "Engine/Level.h"
#include "Engine/World.h"

#include "Rendering/Renderer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/SpotLight.h"

#include "Engine/AssetManager.h"
#include "Rendering/RenderResourceManager.h"

#include "Editor/Application/EditorFileUtils.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/Stats/StatsPanel.h"
#include "UObject/UObjectIterator.h"
#include "Rendering/RenderCommand.h"
#include "RHI/GPUProfiler.h"

#include "Logging/LogMacros.h"
#include "Stats/Stats.h"
#include "Stats/StatDefinitions.h"

#include "Serialization/JsonArchive.h"

#include "Tasks/Tasks.h"
#include "FScene.h"

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
bool FEditorApplication::Init(HINSTANCE hInstance)
{
	// 신규 태스크 스케줄러 초기화
	Tasks::FTaskScheduler::Get().Initialize(4);

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	StatIds::RegisterAll();

	EditorUI->SetNewSceneCallback(
		[this]()
		{
			CreateNewScene();
		});
	EditorUI->SetOpenSceneCallback(
		[this]()
		{
			OpenScene();
		});
	EditorUI->SetSaveSceneCallback(
		[this]()
		{
			SaveCurrentScene();
		});
	EditorUI->SetSaveSceneAsCallback(
		[this]()
		{
			SaveSceneAs();
		});

	OutputLogPanel = EditorUI->AddEditorPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);
	LOG(Info, "Engine Initialize...");

	LOG(Info, "Initialize Renderer...");
	Renderer = MakeUnique<FRenderer>();
	RenderDevice = MakeUnique<FDynamicRHI>();
	FRenderCommand::Init(RenderDevice.get());
	Renderer->Init();

	// Create Main Window & Swapchain
	FWindowContext MainWindowCtx;
	LOG(Info, "Create Main Window...");
	MainWindowCtx.Window = MakeUnique<FWindowsWindow>();
	const int32 ScreenWidth = 1600;
	const int32 ScreenHeight = 900;

	if (!MainWindowCtx.Window->Create(hInstance, ScreenWidth, ScreenHeight, L"Hitori Engine", false))
	{
		LOG(Error, "Failed To Create Main Window!");
		return false;
	}
	MainWindowCtx.Swapchain = MakeUnique<FSwapchain>(RenderDevice.get(), MainWindowCtx.Window.get());
	MainWindow = MainWindowCtx.Window.get();
	MainWindowSC = MainWindowCtx.Swapchain.get();
	Windows.Add(std::move(MainWindowCtx));

	// 로딩 중에도 메시지를 처리하므로 창이 생긴 시점부터 실행 상태로 둔다.
	// 로딩 도중 창을 닫으면 WM_QUIT가 이 값을 false로 바꾸고, Run은 바로 종료된다.
	bIsRunning = true;

	FRenderResourceManager::Init();

	LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(), RenderDevice->GetContext()))
	{
		LOG(Error, "Failed To Initialize ImGui!");
	}
	LOG(Info, "Initialize ImGui Success!");

	LoadingScreen = MakeUnique<FLoadingScreen>();
	LoadingScreen->Init();
	LoadingScreen->SetProgress(0.0f);
	LoadingScreen->SetStatusText("Scanning assets...");
	LoadingScreen->Tick(0.016f);
	// 초기 로딩 화면 출력 및 창 표시
	PresentFrame();
	MainWindow->Show();

	// 애셋 초기화 및 진행률 연동
	LOG(Info, "Initialize AssetManager...");
	UAssetManager::Get().Init(
		[this](float Ratio, const FString& AssetName)
		{
			// 창이 이미 닫혔으면 파괴된 창에 그리지 않는다.
			if (LoadingScreen && bIsRunning)
			{
				LoadingScreen->SetProgress(Ratio);
				LoadingScreen->SetStatusText(AssetName);
				LoadingScreen->Tick(0.016f);
				PresentFrame();
				MainWindow->ProcessMessage(bIsRunning);
			}
		});
	LOG(Info, "Initialize AssetManager Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer.get());

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer.get());

	Gizmo = MakeUnique<FGizmo>();

	// 필요한 패널 추가
	DetailsPanel = EditorUI->AddEditorPanel<FDetailsPanel>();
	EditorControlsPanel = EditorUI->AddEditorPanel<FEditorControlsPanel>();
	SettingsPanel = EditorUI->AddEditorPanel<FSettingsPanel>();
	ViewportsPanel = EditorUI->AddEditorPanel<FViewportsPanel>();
	PIEPanel = EditorUI->AddEditorPanel<FPIEViewportPanel>();
	ViewportsPanel->SetPIEViewportPanel(PIEPanel);
	EditorControlsPanel->SetPIEViewportPanel(PIEPanel);
	EditorUI->AddEditorPanel<FStatsPanel>();
	ContentDrawerPanel = EditorUI->AddEditorPanel<FContentDrawerPanel>();
	
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer.get());

	Outline = MakeUnique<FOutline>();

	SystemFont = UAssetManager::GetAssetByKey<UFont>("Assets/Fonts/Pretendard.json");
	
	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	FogRenderer = MakeUnique<FFogRenderer>();
	FogRenderer->Init();

	FireballRenderer = MakeUnique<FFireballRenderer>();
	FireballRenderer->Init();

	DepthSceneRenderer = MakeUnique<FDepthSceneRenderer>();
	DepthSceneRenderer->Init();

	// Scene
	UWorld* World = FObjectFactory::ConstructObject<UWorld>();
	World->Init();

	FWorldContext WorldContext = {World, EWorldType::Editor};
	WorldContexts.Add(WorldContext);

	// 투영 행렬 생성
	MultipleViewportsAdapter.InitializeFromWorld(*World);
	MultipleViewportsAdapter.SetLayoutMode(ELayoutMode::Single);
	MultipleViewportsAdapter.SetSingleViewIndex(0);

	PIEViewAdapter.InitializeFromWorld(*World);
	if (LoadingScreen)
	{
		// 씬 적재 완료 상태 설정
		LoadingScreen->SetSceneLoaded(true);
	}

	World->GetMainCamera()->GetCameraComponent()->SetExternalInputManaged(true);

	OutlinerPanel = EditorUI->AddEditorPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(World);
	OutlinerPanel->SetSelectionCallback(
		[this](AActor* Actor)
		{
			UPrimitiveComponent* Primitive = Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
			USceneComponent* SceneComp = Actor ? Cast<USceneComponent>(Actor->GetRootComponent()) : nullptr;
			Gizmo->SetTarget(SceneComp);
			Outline->SetTarget(Primitive);
			DetailsPanel->SetTarget(Actor);
		});

	OutlinerPanel->SetDeleteActorCallback(
		[this](AActor* Actor)
		{
			DeleteActor(Actor);
		});

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer.get(), World);

	DetailsPanel->SetWorld(World);

	EditorControlsPanel->SetWorld(World);
	EditorControlsPanel->SetGizmo(Gizmo.get());
	EditorControlsPanel->SetViewportAdapter(&MultipleViewportsAdapter);

	SettingsPanel->SetWorld(World);
	SettingsPanel->SetTearingSupported(MainWindowSC->IsTearingSupported());
	SettingsPanel->SetViewportAdapter(&MultipleViewportsAdapter);

	ViewportsPanel->SetViewportAdapter(&MultipleViewportsAdapter);
	return true;
}

// 프레임 시작·View 상태·월드 갱신·렌더·종료를 순차 반복한다.
void FEditorApplication::Run()
{
	FApp::Init();

	LOG(Info, "{}", "Hello, World!");
	LOG(Info, "{}", FName().ToString());

	while (bIsRunning)
	{
		float DeltaTime = 0.0f;
		if (!BeginFrame(DeltaTime))
			break;

		// 로딩 화면 처리
		if (LoadingScreen && !LoadingScreen->IsFinished())
		{
			LoadingScreen->Tick(DeltaTime);
			PresentFrame();
			continue;
		}

		UpdateMultipleViewportState(DeltaTime);
		UpdatePIEViewportState(DeltaTime);
		TickWorldAndEditor(DeltaTime);
		FGPUProfiler::Get().BeginFrame(RenderDevice->GetDevice(), RenderDevice->GetContext());
		{
			FGPUStatScope Scope(StatIds::GpuFrame(), L"Viewport Render");
			RenderMultipleViewports();
			if (PIEPanel->GetMode() != ETypePIEMode::Selected)
			{
				RenderPIEViewport();
			}
		}
		FGPUProfiler::Get().EndFrame();
		EndFrame();
	}
}

// 창 이벤트·입력을 갱신하고 DeltaTime을 계산한다.
bool FEditorApplication::BeginFrame(float& OutDeltaTime)
{

	FApp::Tick();
	OutDeltaTime = FApp::GetDeltaTime();
	FStats::BeginFrame();
	Tasks::FTaskScheduler::Get().BeginFrame();
	FStatOverlay::Tick(OutDeltaTime);
	EditorControlsPanel->FEditorControlsPanel::DeltaTime = OutDeltaTime;
	FInputSystem::UpdateInputStates();

	MainWindow->ProcessMessage(bIsRunning);
	if (!bIsRunning)
		return false;

	if (!ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::Delete))
		DeleteActor(OutlinerPanel->GetSelectedActor());

	HandleMainWindow();
	return true;
}

// 패널의 Layout·Preset 요청과 입력을 Adapter에 반영한다.
void FEditorApplication::UpdateMultipleViewportState(const float DeltaTime)
{
	const FVector2D ViewportSize = ViewportsPanel->GetContentSize();
	const FVector2D LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

	ELayoutMode RequestedLayout{};
	int32 RequestedSingleViewIndex = MultipleViewportsAdapter.GetSingleViewIndex();
	if (ViewportsPanel->ConsumeLayoutRequest(RequestedLayout, RequestedSingleViewIndex))
	{
		if (RequestedLayout == ELayoutMode::Single)
			MultipleViewportsAdapter.SetSingleViewIndex(RequestedSingleViewIndex);
		MultipleViewportsAdapter.SetLayoutMode(RequestedLayout);
	}

	int32 PresetViewIndex = InvalidViewIndex;
	EMultipleViewportsCameraPreset RequestedPreset = EMultipleViewportsCameraPreset::Perspective;
	if (ViewportsPanel->ConsumeCameraPresetRequest(PresetViewIndex, RequestedPreset))
		MultipleViewportsAdapter.ApplyCameraPreset(PresetViewIndex, RequestedPreset);
	MultipleViewportsAdapter.UpdateLayout(ViewportSize, LocalMousePosition);

	const float HorizontalDrag = ViewportsPanel->ConsumeHorizontalDrag();
	const float VerticalDrag = ViewportsPanel->ConsumeVerticalDrag();
	if (HorizontalDrag != 0.0f)
		MultipleViewportsAdapter.ApplySplitterDrag(EDragAxis::Horizontal, HorizontalDrag, ViewportSize);
	if (VerticalDrag != 0.0f)
		MultipleViewportsAdapter.ApplySplitterDrag(EDragAxis::Vertical, VerticalDrag, ViewportSize);
	if (HorizontalDrag != 0.0f || VerticalDrag != 0.0f)
	{
		MultipleViewportsAdapter.UpdateLayout(ViewportSize, LocalMousePosition);
		const FSplitRatio Ratio = MultipleViewportsAdapter.GetSplitRatio();
	}

	float MoveSpeed = EditorControlsPanel ? EditorControlsPanel->CameraSpeed : 20.0f;

	const bool bCurrentViewportPIE = ViewportsPanel->GetViewportMode() == EViewportMode::PIE;

	const bool bPIEFocused = bCurrentViewportPIE && ViewportsPanel->IsPIEFocused();

	if (!bPIEFocused)
	{
		MultipleViewportsAdapter.UpdateInput(DeltaTime, LocalMousePosition, MoveSpeed, 0.1f);
	}

	const int32 ActiveViewIndex = MultipleViewportsAdapter.GetActiveViewIndex();

	if (ActiveViewIndex != InvalidViewIndex && (ViewportsPanel->IsHovered() || MultipleViewportsAdapter.GetCapturedViewIndex() != InvalidViewIndex))
	{
		MultipleViewportsAdapter.SetEditorViewIndex(ActiveViewIndex);
	}
}

void FEditorApplication::UpdatePIEViewportState(const float DeltaTime)
{
	if (!PIEPanel || !PIEPanel->IsPlay() || PIEPanel->IsPause())
		return;

	const float MoveSpeed = 20.0f;

	if (PIEPanel->GetMode() == ETypePIEMode::NewEditor)
	{
		const FVector2D ViewportSize = PIEPanel->GetContentSize();
		const FVector2D LocalMousePosition = PIEPanel->GetLocalMousePosition();

		if (FPIEViewportPanel::bFocus)
		{
			PIEViewAdapter.UpdateInput(DeltaTime, LocalMousePosition, MoveSpeed, 0.1f);
		}

		const FRect ViewRect{0.0f, 0.0f, std::max(1.0f, ViewportSize.X), std::max(1.0f, ViewportSize.Y)};

		PIEPanel->SetView(ViewRect);
		PIEViewAdapter.SetViewRect(0, ViewRect);

		return;
	}

	const FVector2D ViewportSize = ViewportsPanel->GetContentSize();

	const FVector2D LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

	ViewportsPanel->UpdatePIEInput();

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;

	ViewportsPanel->GetPIEMouseDelta(MouseDeltaX, MouseDeltaY);

	FPIEViewportPanel::DeltaX = MouseDeltaX;
	FPIEViewportPanel::DeltaY = MouseDeltaY;

	if (ViewportsPanel->IsPIEFocused())
	{
		PIEViewAdapter.UpdateInput(DeltaTime, LocalMousePosition, MoveSpeed, 0.1f);
	}

	const FRect ViewRect{0.0f, 0.0f, std::max(1.0f, ViewportSize.X), std::max(1.0f, ViewportSize.Y)};

	PIEPanel->SetView(ViewRect);

	ViewportsPanel->SetView(0, ViewRect, true);

	ViewportsPanel->SetView(1, FRect{}, false);

	ViewportsPanel->SetView(2, FRect{}, false);

	ViewportsPanel->SetView(3, FRect{}, false);

	PIEViewAdapter.SetViewRect(0, ViewRect);
}

// 월드를 한 번 Tick·Capture한 뒤 에디터와 피킹을 갱신한다.
void FEditorApplication::TickWorldAndEditor(const float DeltaTime)
{
	// 월드 상태는 프레임마다 정확히 한 번 갱신하고 캡처한다.
	for (const FWorldContext& WorldContext : WorldContexts)
	{
		if (!WorldContext.World)
			continue;
		// 일시정지 중에는 PIE 월드를 멈춘다.
		if (WorldContext.WorldType == EWorldType::PIE && PIEPanel->IsPause())
			continue;
		WorldContext.World->Tick(DeltaTime);
	}
	EditorUI->Tick(DeltaTime);

	if (PIEPanel->IsActive())
	{
		if (!PIEPanel->IsPlay())
			StartPIE();
		if (UWorld* PIEWorld = GetPIEWorld())
			PIEViewAdapter.CaptureWorld(*PIEWorld);
	}
	else
	{
		if (PIEPanel->IsPlay())
			EndPIE();
	}

	if (ViewportsPanel->IsPIEMode())
	{
		MultipleViewportsAdapter.CaptureWorld(*GetPIEWorld());
		if (!ViewportsPanel->IsPIEFocused())
		{
			ViewportsPanel->SetViewportAdapter(&MultipleViewportsAdapter);
		}
		else
		{
			ViewportsPanel->SetViewportAdapter(&PIEViewAdapter);
			MultipleViewportsAdapter.SetViewCameraTransform(0, PIEViewAdapter.GetViewCamera(0).Transform);
		}
	}
	else
	{
		MultipleViewportsAdapter.CaptureWorld(*GetEditorWorld());
	}

	//culling
	UpdateGizmoAndPicking();
}

// 공유 월드 캡처로 활성 View별 렌더 큐를 만들고 렌더한다.
void FEditorApplication::RenderMultipleViewports()
{
	EMultipleViewportsCameraPreset CameraPresets[4]{};
	FScene SceneData;
	if (ViewportsPanel->IsPIEMode())
	{
		SceneData = GetPIEWorld()->GetScene();
	}
	else
	{
		SceneData = GetEditorWorld()->GetScene();
	}
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		const bool bActive = MultipleViewportsAdapter.IsViewActive(ViewIndex);
		ViewportsPanel->SetView(ViewIndex, MultipleViewportsAdapter.GetViewRect(ViewIndex), bActive);
		CameraPresets[ViewIndex] = MultipleViewportsAdapter.GetCameraPreset(ViewIndex);

		if (!bActive)
			continue;
		
		ViewportsPanel->GetViewportAdapter()->BuildRenderPackets(ViewIndex, SceneRenderPackets);
		RenderFrame(ViewIndex,
			ViewportsPanel->GetRenderingInfo(ViewIndex),
			ViewportsPanel->GetViewportAdapter()->GetEngineViewProjection(ViewIndex),
			ViewportsPanel->GetViewportAdapter()->GetEnginePerspectiveProjection(),
			ViewportsPanel->GetViewportAdapter()->GetEngineCameraLocation(ViewIndex),
			ViewportsPanel->GetViewportAdapter()->GetEngineCameraForward(ViewIndex),
			SceneRenderPackets,
			SceneData);
	}

	ViewportsPanel->SetControlState(MultipleViewportsAdapter.GetLayoutMode(), MultipleViewportsAdapter.GetSingleViewIndex(), CameraPresets);
}

void FEditorApplication::RenderPIEViewport()
{
	if (PIEPanel && PIEPanel->IsPlay() && !PIEPanel->IsPause())
	{
		const FRect PIEViewRect{0.0f, 0.0f, PIEPanel->GetContentSize().X, PIEPanel->GetContentSize().Y};

		PIEPanel->SetView(PIEViewRect);

		PIEViewAdapter.BuildRenderPackets(0, SceneRenderPackets);

		RenderPIEFrame(PIEPanel->GetRenderingInfo(), PIEViewAdapter.GetEngineViewProjection(0), PIEViewAdapter.GetEngineCameraLocation(0), PIEViewAdapter.GetEngineCameraForward(0), SceneRenderPackets);
	}
}

void FEditorApplication::RenderWorldTexts(const UWorld* TargetWorld, const FMatrix& ViewProjection)
{
	if (!TargetWorld)
	{
		return;
	}

	for (TObjectIterator<UTextRenderComponent> TextComponent; TextComponent; ++TextComponent)
	{
		if (!TextComponent || !TextComponent->GetFont() || !TextComponent->IsVisible())
		{
			continue;
		}

		AActor* Owner = TextComponent->GetOwner();
		if (!Owner || Owner->GetWorld() != TargetWorld)
		{
			continue;
		}

		TextRenderer->OnRender(TextComponent->GetText(), TextComponent->GetWorldMatrix(), TextComponent->GetTextSize(), *TextComponent->GetFont(), ViewProjection);
	}
}

// 화면을 표시하고 UI 변경 후 View 설정을 보관한다.
void FEditorApplication::EndFrame()
{
	PresentFrame();
	// UI 변경 후 설정을 복사해 종료 시 카메라 수명에 의존하지 않는다.
	SettingsPanel->CaptureViewportSettings();
}

// 입력 View의 Ray와 피킹으로 Gizmo·공유 선택을 갱신한다.
void FEditorApplication::UpdateGizmoAndPicking()
{
	// Delete는 BeginFrame에서 한 번만 처리하고 여기서는 View 입력만 다룬다.
	const int32 ViewIndex = MultipleViewportsAdapter.GetActiveViewIndex();
	if (ViewIndex == InvalidViewIndex || (!ViewportsPanel->IsHovered() && !Gizmo->IsUsing()))
		return;

	const FVector2D LocalMousePosition = ViewportsPanel->GetLocalMousePosition();
	FRay Ray{};
	if (!MultipleViewportsAdapter.TryGetActiveViewRay(LocalMousePosition, Ray))
		return;

	const FRect& Rect = MultipleViewportsAdapter.GetViewRect(ViewIndex);
	const FVector2D ViewLocalMouse(LocalMousePosition.X - Rect.X, LocalMousePosition.Y - Rect.Y);
	const FMatrix ViewProjection = MultipleViewportsAdapter.GetEngineViewProjection(ViewIndex);
	bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

	Gizmo->Update(Ray,
		ViewLocalMouse,
		ViewProjection,
		static_cast<int>(Rect.Width),
		static_cast<int>(Rect.Height),
		bMouseDown,
		MultipleViewportsAdapter.GetEngineCameraLocation(ViewIndex),
		MultipleViewportsAdapter.IsOrthographic(ViewIndex));

	if (FInputSystem::IsMousePressed(EMouseButton::Left) && !Gizmo->IsUsing() && Gizmo->GetHoveredAxis() < 0)
	{
		if (ViewportsPanel->IsPIEMode())
		{
			if (!ViewportsPanel->IsPIEFocused())
			{
				MultipleViewportsAdapter.PickActiveView(LocalMousePosition, *GetPIEWorld());
				MultipleViewportsAdapter.ApplyLastPickToOutliner(*OutlinerPanel);
				if (!MultipleViewportsAdapter.GetLastPick().bHit)
				{
					ViewportsPanel->SetPIE();
					OutlinerPanel->SelectActor(nullptr);
				}
			}
		}
		else
		{
			MultipleViewportsAdapter.PickActiveView(LocalMousePosition, *GetEditorWorld());
			MultipleViewportsAdapter.ApplyLastPickToOutliner(*OutlinerPanel);
		}
	}
}

// TArray 기반 렌더 프레임
void FEditorApplication::RenderFrame(const int32 ViewIndex,
	const FRenderingInfo& ViewRenderingInfo,
	const FMatrix& ViewProjection,
	const FMatrix& Projection,
	const FVector& ViewCameraLocation,
	const FVector& ViewCameraForward,
	TArray<FRenderPacket>& RenderPackets,
	const FScene& SceneData)
{
	//Render 초기화
	FRenderCommand::BeginRenderPass(ViewRenderingInfo);

	const FEditorSettings& EditorSettings = SettingsPanel ? SettingsPanel->GetSettings() : FEditorSettings{};
	const float FarClip = MultipleViewportsAdapter.GetViewCamera(ViewIndex).Projection.FarClip;

	UWorld* CurrentWorld = ViewportsPanel->IsPIEMode() ? GetPIEWorld() : GetEditorWorld();

	//Grid 렌더링
	{
		FGPUStatScope GridScope(StatIds::GpuGrid(), L"Grid");
		GridRenderer->OnRenderPSGrid(ViewProjection, ViewCameraLocation, EditorSettings, ViewRenderingInfo.ViewportSetting, FarClip);
	}

	// Opaque(불투명) 렌더링
	{
		const bool bDrawPrimitives = EditorSettings.bDrawPrimitives;
		if (bDrawPrimitives)
		{
			const bool bWireframe = MultipleViewportsAdapter.IsViewWireframe(ViewIndex);
			Renderer->RenderOpaque(RenderPackets, ViewProjection, bWireframe);
		}
	}
	// HZB+ 렌더링
	{
		MultipleViewportsAdapter.PostRenderOpaque(ViewIndex, ViewRenderingInfo.DepthSteincil.Texture);
	}

	// SceneDepth Rendering
	{
		if (MultipleViewportsAdapter.IsViewSceneDepthMode(ViewIndex))
		{
			DepthSceneRenderer->OnRender(ViewRenderingInfo.DepthSteincil.Texture, Projection);
		}
	}

	// Multi pass 렌더링
	{
		// fireball 렌더링
		if (!SceneData.Fireballs.IsEmpty())
		{
			FireballRenderer->OnRender(ViewRenderingInfo.DepthSteincil.Texture, ViewProjection, SceneData.Fireballs, ViewRenderingInfo.ViewportSetting);
		}

		// fog 렌더링
		if (SceneData.FogSceneData.IsValid())
		{
			FogRenderer->OnRender(ViewRenderingInfo.DepthSteincil.Texture, ViewProjection, ViewCameraLocation, SceneData.FogSceneData);

		}

		// Anti Aliasing 처리
		{

		}
	}

	// Text 렌더링
	{
		RenderWorldTexts(GetEditorWorld(), ViewProjection);
	}

	// Line Batch 렌더링
	{
		FGPUStatScope EditorScope(StatIds::GpuEditor(), L"Editor Overlays");
		if (MultipleViewportsAdapter.GetSoftwareOcclusionSettings().bDebugBounds)
		{
			LineBatcher->BeginFrame();
			MultipleViewportsAdapter.AppendSoftwareOcclusionDebugBounds(*LineBatcher);
			LineBatcher->OnRender(ViewProjection);
		}
		if (Outline && Outline->GetTarget() && OutlineRenderer && CurrentWorld == Outline->GetTarget()->GetOwner()->GetWorld())
		{
			OutlineRenderer->OnRender(*Outline, ViewProjection, ViewRenderingInfo.ViewportSetting);
		}
	}
	// Gizmo 렌더링
	{
		if (Gizmo->GetTarget() && CurrentWorld == Gizmo->GetTarget()->GetOwner()->GetWorld())
		{
			auto Target = Cast<UPrimitiveComponent>(Gizmo->GetTarget());
			//FBox box = Target->CalcBounds();
			FRenderCommand::ClearDepthStencil(ViewRenderingInfo.DepthSteincil.Texture);
			GizmoRenderer->OnRender(*Gizmo, ViewProjection, ViewCameraLocation, MultipleViewportsAdapter.IsOrthographic(ViewIndex));
		}
	}




	FRenderCommand::ClearDepthStencil(ViewRenderingInfo.DepthSteincil.Texture);

	if (Gizmo->GetTarget() && CurrentWorld == Gizmo->GetTarget()->GetOwner()->GetWorld() && SystemFont)
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Gizmo->GetTarget()))
		{
			if (AActor* SelectedActor = Primitive->GetOwner())
			{
				FBox Box = Primitive->CalcBounds();
				FVector UUIDLocation;
				UUIDLocation.X = (Box.Min.X + Box.Max.X) * 0.5f;
				UUIDLocation.Y = (Box.Min.Y + Box.Max.Y) * 0.5f;
				UUIDLocation.Z = Box.Max.Z + 0.5f;

				FString Text = "UUID : " + std::to_string(SelectedActor->GetUUID());
				TextRenderer->BuildTextMesh(Text, 0.5f, *SystemFont);

				const FMatrix BillboardWorld = MultipleViewportsAdapter.BuildEngineBillboardMatrix(ViewIndex, UUIDLocation, 1.0f, 1.0f);
				TextRenderer->OnRender(Text, BillboardWorld, 0.5f, *SystemFont, ViewProjection);
			}
		}
	}

	FRenderCommand::EndRenderPass(ViewRenderingInfo);
}

void FEditorApplication::RenderPIEFrame(const FRenderingInfo& ViewRenderingInfo,
	const FMatrix& ViewProjection,
	const FVector& ViewCameraLocation,
	const FVector& ViewCameraForward,
	TArray<FRenderPacket>& RenderPackets)
{
	FRenderCommand::BeginRenderPass(ViewRenderingInfo);

	const FEditorSettings& EditorSettings = SettingsPanel ? SettingsPanel->GetSettings() : FEditorSettings{};
	const float FarClip = PIEViewAdapter.GetViewCamera(0).Projection.FarClip;
	{
		FGPUStatScope GridScope(StatIds::GpuGrid(), L"Grid");
		GridRenderer->OnRenderPSGrid(ViewProjection, ViewCameraLocation, EditorSettings, ViewRenderingInfo.ViewportSetting, FarClip);
	}

	const bool bDrawPrimitives = EditorSettings.bDrawPrimitives;
	if (bDrawPrimitives)
	{
		const bool bWireframe = false;
		Renderer->RenderOpaque(RenderPackets, ViewProjection, bWireframe);
	}

	PIEViewAdapter.PostRenderOpaque(0, ViewRenderingInfo.DepthSteincil.Texture);

	RenderWorldTexts(GetPIEWorld(), ViewProjection);

	FGPUStatScope EditorScope(StatIds::GpuEditor(), L"Editor Overlays");

	FRenderCommand::EndRenderPass(ViewRenderingInfo);
}

// View Texture가 포함된 UI를 Swapchain에 합성해 표시한다.
void FEditorApplication::PresentFrame()
{
	// Swapchain 렌더링
	FRenderCommand::BeginRenderPass(MainWindowSC->GetRenderingInfo());

	ImGuiRenderer->Begin();

	// 로딩 화면 또는 에디터 UI 렌더링
	if (LoadingScreen && !LoadingScreen->IsFinished())
	{
		LoadingScreen->Draw();
	}
	else
	{
		EditorUI->OnRender();
	}

	ImGuiRenderer->End();

	FRenderCommand::EndRenderPass(MainWindowSC->GetRenderingInfo());

	// 버퍼 갱신
	MainWindowSC->SwapBuffers(0, 0);
}

// 엔진 종료에 필요한 자원 정리를 수행한다.
void FEditorApplication::Shutdown()
{
	FGPUProfiler::Get().Shutdown();
	UAssetManager::Get().Shutdown();
	FRenderResourceManager::Shutdown();

	// 삭제된 슬롯은 nullptr로 남으므로 건너뛴다.
	// (액터를 지우면 컴포넌트도 같이 지워져 앞쪽 슬롯이 비워질 수 있어 매번 다시 확인한다)
	for (int32 Index = GUObjectArray.Num() - 1; Index >= 0; --Index)
	{
		if (GUObjectArray[Index])
		{
			delete GUObjectArray[Index];
		}
	}

	ImGuiRenderer->Shutdown();
	RenderDevice->Shutdown();

	// 신규 태스크 스케줄러 종료
	Tasks::FTaskScheduler::Get().Shutdown();
}

// 메인 창 크기에 맞춰 Swapchain을 갱신한다.
void FEditorApplication::HandleMainWindow()
{
	if (MainWindow->CheckResized())
	{
		MainWindowSC->Resize(MainWindow->GetWidth(), MainWindow->GetHeight());
	}
}

void FEditorApplication::StartPIE()
{
	PIEPanel->SetPlay(true);
	UpdatePIEViewportState(0);

	UWorld* EditorWorld = GetEditorWorld();
	UWorld* PIEWorld = EditorWorld ? Cast<UWorld>(EditorWorld->Duplicate()) : nullptr;
	if (!PIEWorld)
	{
		LOG(Error, "StartPIE : Failed to duplicate editor world");
		// 매 프레임 재시도하지 않도록 PIE 요청도 함께 취소한다.
		PIEPanel->SetPlay(false);
		PIEPanel->SetActive(false);
		return;
	}

	PIEWorld->SetWorldType(EWorldType::PIE);
	WorldContexts.Add({PIEWorld, EWorldType::PIE});

	PIEViewAdapter.InitializeFromWorld(*PIEWorld);
	if (PIEPanel->GetMode() == ETypePIEMode::Selected)
	{
		ViewportsPanel->SetViewportAdapter(&PIEViewAdapter);
		ViewportsPanel->SetViewportMode(EViewportMode::PIE);
		ViewportsPanel->SetPIE();
		OutlinerPanel->SelectActor(nullptr);
	}
	OutlinerPanel->SetWorld(PIEWorld);
	DetailsPanel->SetWorld(PIEWorld);
	SettingsPanel->SetWorld(PIEWorld);
	EditorControlsPanel->SetWorld(PIEWorld);
	PIEViewAdapter.SetViewCameraTransform(0, PIEPanel->GetPlayerStart());
}

void FEditorApplication::PausePIE() const
{
	if (!PIEPanel->IsPlay())
		return;
}

void FEditorApplication::EndPIE()
{
	PIEPanel->SetPlay(false);
	ResetSceneSelection();
	UWorld* PIEWorld = GetPIEWorld();
	RemovePIEWorld();
	if (PIEWorld)
	{
		PIEWorld->DestroyWorld();
		delete PIEWorld;
	}
	ViewportsPanel->SetViewportAdapter(&MultipleViewportsAdapter);
	ViewportsPanel->SetViewportMode(EViewportMode::Editor);
	UWorld* World = GetEditorWorld(); 
	OutlinerPanel->SetWorld(World);
	DetailsPanel->SetWorld(World);
	SettingsPanel->SetWorld(World);
	EditorControlsPanel->SetWorld(World);
}

FWorldContext FEditorApplication::FindWorldContext(EWorldType WorldType)
{
	for (const FWorldContext& WorldContext : WorldContexts)
	{
		if (WorldContext.WorldType == WorldType)
		{
			return WorldContext;
		}
	}

	return FWorldContext();
}

UWorld* FEditorApplication::GetEditorWorld()
{
	for (const FWorldContext& WorldContext : WorldContexts)
	{
		if (WorldContext.WorldType == EWorldType::Editor)
		{
			return WorldContext.World;
		}
	}

	return nullptr;
}

UWorld* FEditorApplication::GetPIEWorld()
{
	for (const FWorldContext& WorldContext : WorldContexts)
	{
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			return WorldContext.World;
		}
	}

	return nullptr;
}

void FEditorApplication::RemovePIEWorld()
{
	for (int32 I = 0; I < WorldContexts.Num(); I++)
	{
		const FWorldContext& WorldContext = WorldContexts[I];
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			WorldContexts.RemoveAtSwap(I);
			return;
		}
	}
}

// 선택과 Gizmo 참조를 정리한 뒤 Actor를 삭제한다.
void FEditorApplication::DeleteActor(AActor* Actor)
{
	if (!Actor)
		return;

	OutlinerPanel->SelectActor(nullptr);

	Actor->Destroy();
}

// 씬 변경으로 무효화된 에디터의 선택 참조를 모두 해제한다.
void FEditorApplication::ResetSceneSelection()
{
	Gizmo->SetTarget(nullptr);
	Outline->SetTarget(nullptr);
	DetailsPanel->SetTarget(nullptr);
	OutlinerPanel->SelectActor(nullptr);
}

// 새 씬 생성이 성공하면 에디터 선택 상태를 초기화한다.
void FEditorApplication::CreateNewScene()
{
	if (!FEditorFileUtils::NewScene(GetEditorWorld()))
		return;

	ResetSceneSelection();
	MultipleViewportsAdapter.ResetSoftwareOcclusionScene();
	PIEViewAdapter.ResetSoftwareOcclusionScene();
}

// 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void FEditorApplication::OpenScene()
{
	if (!FEditorFileUtils::LoadScene(GetEditorWorld()))
		return;

	ResetSceneSelection();
	MultipleViewportsAdapter.ResetSoftwareOcclusionScene();
	PIEViewAdapter.ResetSoftwareOcclusionScene();
}

// 공통 파일 유틸리티로 현재 씬을 저장한다.
void FEditorApplication::SaveCurrentScene()
{
	FEditorFileUtils::SaveScene(GetEditorWorld());
}

// 공통 파일 유틸리티로 새 경로에 씬을 저장한다.
void FEditorApplication::SaveSceneAs()
{
	FEditorFileUtils::SaveSceneAs(GetEditorWorld());
}
