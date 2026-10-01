#include "EnginePCH.h"

#include "Editor/Application/EditorApplication.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/OutputLog/OutputLogPanel.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Viewports/ViewportsPanel.h"

#include "Core/EngineStatics.h"
#include "Core/EngineTimer.h"
#include "Core/StatOverlay.h"
#include "Input/InputSystem.h"

#include "ObjectSystem/ObjectFactory.h"

#include "Rendering/GeometryGenerator.h"

#include "World/Level.h"
#include "World/World.h"

#include "Rendering/Renderer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor/LightActor.h"

#include "Asset/AssetManager.h"
#include "Rendering/RenderResourceManager.h"

#include "Editor/Application/EditorFileUtils.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/Stats/StatsPanel.h"
#include "ObjectSystem/UObjectIterator.h"
#include "Rendering/RenderCommand.h"
#include "Rendering/GPUProfiler.h"

#include "Core/EngineLog.h"
#include "Core/Stats.h"
#include "Core/StatDefinitions.h"

#include "Serialization/DefaultSceneLoader.h"
#include "Serialization/JsonArchive.h"

#include "Tasks/Tasks.h"

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
bool FEditorApplication::Init(HINSTANCE hInstance) {
  // 신규 태스크 스케줄러 초기화
  Tasks::FTaskScheduler::Get().Initialize(4);

  EditorUI = MakeUnique<FEditorUI>();
  EditorUI->Init();

  StatIds::RegisterAll();

  EditorUI->SetNewSceneCallback([this]() { CreateNewScene(); });
  EditorUI->SetOpenSceneCallback([this]() { OpenScene(); });
  EditorUI->SetOpenCompetitionSceneCallback([this]() { OpenCompetitionScene(); });
  EditorUI->SetSaveSceneCallback([this]() { SaveCurrentScene(); });
  EditorUI->SetSaveSceneAsCallback([this]() { SaveSceneAs(); });

  OutputLogPanel = EditorUI->AddEditorPanel<FOutputLogPanel>();
  FLog::AddSink(OutputLogPanel);
  LOG(Info, "Engine Initialize...");

  LOG(Info, "Initialize Renderer...");
  Renderer = MakeUnique<FRenderer>();
  RenderDevice = MakeUnique<FRenderDevice>();
  RenderCommand::Init(RenderDevice.get());
  Renderer->Init();

  // Create Main Window & Swapchain
  FWindowContext MainWindowCtx;
  LOG(Info, "Create Main Window...");
  MainWindowCtx.Window = MakeUnique<FWindow>();
  const int32 ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
  const int32 ScreenHeight = GetSystemMetrics(SM_CYSCREEN);

  if(!MainWindowCtx.Window->Create(hInstance, ScreenWidth, ScreenHeight, L"Hitori Engine", false))
  {
    LOG(Error, "Failed To Create Main Window!");
    return false;
  }
  MainWindowCtx.Swapchain =
      MakeUnique<FSwapchain>(RenderDevice.get(), MainWindowCtx.Window.get());
  MainWindow = MainWindowCtx.Window.get();
  MainWindowSC = MainWindowCtx.Swapchain.get();
  Windows.Add(std::move(MainWindowCtx));

  FRenderResourceManager::Init();

  LOG(Info, "Initialize ImGui...");
  ImGuiRenderer = MakeUnique<FImGuiRenderer>();
  if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(),
                           RenderDevice->GetContext())) {
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
  UAssetManager::Get().Init([this](float Ratio, const FString& AssetName) {
    if (LoadingScreen) {
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
  EditorUI->AddEditorPanel<FStatsPanel>();

  OutlineRenderer = MakeUnique<FOutlineRenderer>();
  OutlineRenderer->Init(Renderer.get());

  Outline = MakeUnique<FOutline>();

  SystemFont = UAssetManager::GetAssetByKey<UFont>("Assets/Fonts/Pretendard.json");

  TextRenderer = MakeUnique<FTextRenderer>();
  TextRenderer->Init();

  // Scene
  World = FObjectFactory::ConstructObject<UWorld>();
  World->Init();

  // 공식 씬 파일 고속 로드
  //if (!FDefaultSceneLoader::LoadScene(World, "Scenes/Default.scene", [this](float Ratio) {
  //  if (LoadingScreen) {
  //    LoadingScreen->SetProgress(0.1f + Ratio * 0.75f);
  //    LoadingScreen->Tick(0.016f);
  //    PresentFrame();
  //    MainWindow->ProcessMessage(bIsRunning);
  //  }
  //})) {
  //  LOG(Warning, "Failed to load Scenes/Default.scene");
  //}

  // 투영 행렬 생성
  MultipleViewportsAdapter.InitializeFromWorld(*World);
  MultipleViewportsAdapter.SetLayoutMode(ELayoutMode::Single);
  MultipleViewportsAdapter.SetSingleViewIndex(0);

  if (LoadingScreen) {
    // 씬 적재 완료 상태 설정
    LoadingScreen->SetSceneLoaded(true);
  }

  World->GetMainCamera()->GetCameraComponent()->SetExternalInputManaged(true);

  OutlinerPanel = EditorUI->AddEditorPanel<FOutlinerPanel>();
  OutlinerPanel->SetWorld(World);
  OutlinerPanel->SetSelectionCallback([this](UPrimitiveComponent *Primitive) {
    Gizmo->SetTarget(Primitive);
    Outline->SetTarget(Primitive);
    DetailsPanel->SetTarget(Primitive);
  });

  OutlinerPanel->SetDeleteActorCallback(
      [this](AActor *Actor) { DeleteActor(Actor); });

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
  bIsRunning = true;
  return true;
}

// 프레임 시작·View 상태·월드 갱신·렌더·종료를 순차 반복한다.
void FEditorApplication::Run() {
  EngineTimer::Init();

  LOG(Info, "{}", "Hello, World!");
  LOG(Info, "{}", FName().ToString());

  while (bIsRunning) {
    float DeltaTime = 0.0f;
    if (!BeginFrame(DeltaTime))
      break;

    // 로딩 화면 처리
    if (LoadingScreen && !LoadingScreen->IsFinished()) {
      LoadingScreen->Tick(DeltaTime);
      PresentFrame();
      continue;
    }

    UpdateMultipleViewportState(DeltaTime);
    TickWorldAndEditor(DeltaTime);
    FGPUProfiler::Get().BeginFrame(RenderDevice->GetDevice(), RenderDevice->GetContext());
    {
      FGPUStatScope Scope(StatIds::GpuFrame(), L"Viewport Render");
      RenderMultipleViewports();
    }
    FGPUProfiler::Get().EndFrame();
    EndFrame();
  }
}

// 창 이벤트·입력을 갱신하고 DeltaTime을 계산한다.
bool FEditorApplication::BeginFrame(float &OutDeltaTime) {

  EngineTimer::Tick();
  OutDeltaTime = EngineTimer::GetDeltaTime();
  FStats::BeginFrame();
  Tasks::FTaskScheduler::Get().BeginFrame();
  FStatOverlay::Tick(OutDeltaTime);
  EditorControlsPanel->FEditorControlsPanel::DeltaTime = OutDeltaTime;
  FInputSystem::UpdateInputStates();

  MainWindow->ProcessMessage(bIsRunning);
  if (!bIsRunning)
    return false;

  if (!ImGui::GetIO().WantTextInput &&
      FInputSystem::IsKeyPressed(EKeyCode::Delete))
    DeleteActor(OutlinerPanel->GetSelectedActor());

  HandleMainWindow();
  return true;
}

// 패널의 Layout·Preset 요청과 입력을 Adapter에 반영한다.
void FEditorApplication::UpdateMultipleViewportState(const float DeltaTime) {
  const FVector2 ViewportSize = ViewportsPanel->GetContentSize();
  const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

  ELayoutMode RequestedLayout{};
  int32 RequestedSingleViewIndex =
      MultipleViewportsAdapter.GetSingleViewIndex();
  if (ViewportsPanel->ConsumeLayoutRequest(RequestedLayout,
                                           RequestedSingleViewIndex)) {
    if (RequestedLayout == ELayoutMode::Single)
      MultipleViewportsAdapter.SetSingleViewIndex(RequestedSingleViewIndex);
    MultipleViewportsAdapter.SetLayoutMode(RequestedLayout);
  }

  int32 PresetViewIndex = InvalidViewIndex;
  EMultipleViewportsCameraPreset RequestedPreset =
      EMultipleViewportsCameraPreset::Perspective;
  if (ViewportsPanel->ConsumeCameraPresetRequest(PresetViewIndex,
                                                 RequestedPreset))
    MultipleViewportsAdapter.ApplyCameraPreset(PresetViewIndex,
                                               RequestedPreset);
  MultipleViewportsAdapter.UpdateLayout(ViewportSize, LocalMousePosition);

  const float HorizontalDrag = ViewportsPanel->ConsumeHorizontalDrag();
  const float VerticalDrag = ViewportsPanel->ConsumeVerticalDrag();
  if (HorizontalDrag != 0.0f)
    MultipleViewportsAdapter.ApplySplitterDrag(EDragAxis::Horizontal,
                                               HorizontalDrag, ViewportSize);
  if (VerticalDrag != 0.0f)
    MultipleViewportsAdapter.ApplySplitterDrag(EDragAxis::Vertical,
                                               VerticalDrag, ViewportSize);
  if (HorizontalDrag != 0.0f || VerticalDrag != 0.0f) {
    MultipleViewportsAdapter.UpdateLayout(ViewportSize, LocalMousePosition);
    const FSplitRatio Ratio = MultipleViewportsAdapter.GetSplitRatio();
  }

  float MoveSpeed =
      EditorControlsPanel ? EditorControlsPanel->CameraSpeed : 20.0f;

  MultipleViewportsAdapter.UpdateInput(DeltaTime, LocalMousePosition, MoveSpeed,
                                       0.1f);
  const int32 ActiveViewIndex = MultipleViewportsAdapter.GetActiveViewIndex();
  if (ActiveViewIndex != InvalidViewIndex &&
      (ViewportsPanel->IsHovered() ||
       MultipleViewportsAdapter.GetCapturedViewIndex() != InvalidViewIndex))
    MultipleViewportsAdapter.SetEditorViewIndex(ActiveViewIndex);
}

// 월드를 한 번 Tick·Capture한 뒤 에디터와 피킹을 갱신한다.
void FEditorApplication::TickWorldAndEditor(const float DeltaTime) {
  // 월드 상태는 프레임마다 정확히 한 번 갱신하고 캡처한다.
  World->Tick(DeltaTime);
  EditorUI->Tick(DeltaTime);
  MultipleViewportsAdapter.CaptureWorld(*World);
  //culling
  UpdateGizmoAndPicking();
}

// 공유 월드 캡처로 활성 View별 렌더 큐를 만들고 렌더한다.
void FEditorApplication::RenderMultipleViewports() {
  EMultipleViewportsCameraPreset CameraPresets[4]{};
  for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex) {
    const bool bActive = MultipleViewportsAdapter.IsViewActive(ViewIndex);
    ViewportsPanel->SetView(
        ViewIndex, MultipleViewportsAdapter.GetViewRect(ViewIndex), bActive);
    CameraPresets[ViewIndex] =
        MultipleViewportsAdapter.GetCameraPreset(ViewIndex);

    if (!bActive)
      continue;

    MultipleViewportsAdapter.BuildRenderPackets(ViewIndex, SceneRenderPackets);
    RenderFrame(ViewIndex, ViewportsPanel->GetRenderingInfo(ViewIndex),
                MultipleViewportsAdapter.GetEngineViewProjection(ViewIndex),
                MultipleViewportsAdapter.GetEngineCameraLocation(ViewIndex),
                MultipleViewportsAdapter.GetEngineCameraForward(ViewIndex),
                SceneRenderPackets);
  }

  ViewportsPanel->SetControlState(MultipleViewportsAdapter.GetLayoutMode(),
                                  MultipleViewportsAdapter.GetSingleViewIndex(),
                                  CameraPresets);
}

// 화면을 표시하고 UI 변경 후 View 설정을 보관한다.
void FEditorApplication::EndFrame() {
  PresentFrame();
  // UI 변경 후 설정을 복사해 종료 시 카메라 수명에 의존하지 않는다.
  SettingsPanel->CaptureViewportSettings();
}

// 입력 View의 Ray와 피킹으로 Gizmo·공유 선택을 갱신한다.
void FEditorApplication::UpdateGizmoAndPicking() {
  // Delete는 BeginFrame에서 한 번만 처리하고 여기서는 View 입력만 다룬다.
  const int32 ViewIndex = MultipleViewportsAdapter.GetActiveViewIndex();
  if (ViewIndex == InvalidViewIndex || (!ViewportsPanel->IsHovered() && !Gizmo->IsUsing()))
    return;

  const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();
  FRay Ray{};
  if (!MultipleViewportsAdapter.TryGetActiveViewRay(LocalMousePosition, Ray))
    return;

  const FRect &Rect = MultipleViewportsAdapter.GetViewRect(ViewIndex);
  const FVector2 ViewLocalMouse(LocalMousePosition.X - Rect.X,
                                LocalMousePosition.Y - Rect.Y);
  const FMatrix ViewProjection =
      MultipleViewportsAdapter.GetEngineViewProjection(ViewIndex);
  bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

  Gizmo->Update(Ray, ViewLocalMouse, ViewProjection,
                static_cast<int>(Rect.Width), static_cast<int>(Rect.Height),
                bMouseDown,
                MultipleViewportsAdapter.GetEngineCameraLocation(ViewIndex),
                MultipleViewportsAdapter.IsOrthographic(ViewIndex));

  if (FInputSystem::IsMousePressed(EMouseButton::Left) && !Gizmo->IsUsing() &&
      Gizmo->GetHoveredAxis() < 0) {
    MultipleViewportsAdapter.PickActiveView(LocalMousePosition, *World);
    MultipleViewportsAdapter.ApplyLastPickToOutliner(*OutlinerPanel);
  }
}

// TArray 기반 렌더 프레임
void FEditorApplication::RenderFrame(const int32 ViewIndex,
                                     const FRenderingInfo &ViewRenderingInfo,
                                     const FMatrix &ViewProjection,
                                     const FVector &ViewCameraLocation,
                                     const FVector &ViewCameraForward,
                                     TArray<FRenderPacket> &RenderPackets) {
  RenderCommand::BeginRenderPass(ViewRenderingInfo);

  const FEditorSettings& EditorSettings = SettingsPanel ? SettingsPanel->GetSettings() : FEditorSettings{};
  const float FarClip = MultipleViewportsAdapter.GetViewCamera(ViewIndex).Projection.FarClip;
  {
    FGPUStatScope GridScope(StatIds::GpuGrid(), L"Grid");
  GridRenderer->OnRenderPSGrid(ViewProjection, ViewCameraLocation,
                               EditorSettings,
                               ViewRenderingInfo.ViewportSetting, FarClip);
  }

  const bool bDrawPrimitives = EditorSettings.bDrawPrimitives;
  if (bDrawPrimitives) {
    const bool bWireframe = MultipleViewportsAdapter.IsViewWireframe(ViewIndex);
    Renderer->RenderOpaque(RenderPackets, ViewProjection, bWireframe);
  }

  MultipleViewportsAdapter.PostRenderOpaque(ViewIndex, ViewRenderingInfo.DepthSteincil.Texture);

  {
    FGPUStatScope EditorScope(StatIds::GpuEditor(), L"Editor Overlays");
  if (MultipleViewportsAdapter.GetSoftwareOcclusionSettings().bDebugBounds) {
    LineBatcher->BeginFrame();
    MultipleViewportsAdapter.AppendSoftwareOcclusionDebugBounds(*LineBatcher);
    LineBatcher->OnRender(ViewProjection);
  }

  if (Outline && Outline->GetTarget() && OutlineRenderer) {
    OutlineRenderer->OnRender(*Outline, ViewProjection,
                              ViewRenderingInfo.ViewportSetting);
  }

  if (Gizmo->GetTarget()) {
    auto Target = Cast<UPrimitiveComponent>(Gizmo->GetTarget());
    FBox box = Target->CalcBounds();
    RenderCommand::ClearDepthStencil(ViewRenderingInfo.DepthSteincil.Texture);
    GizmoRenderer->OnRender(*Gizmo, ViewProjection, ViewCameraLocation,
                            MultipleViewportsAdapter.IsOrthographic(ViewIndex));
  }

  RenderCommand::ClearDepthStencil(ViewRenderingInfo.DepthSteincil.Texture);

  if (Gizmo->GetTarget() && SystemFont) {
    if (UPrimitiveComponent *Primitive =
            Cast<UPrimitiveComponent>(Gizmo->GetTarget())) {
      if (AActor *SelectedActor = Primitive->GetOwner()) {
        FBox Box = Primitive->CalcBounds();
        FVector UUIDLocation;
        UUIDLocation.X = (Box.Min.X + Box.Max.X) * 0.5f;
        UUIDLocation.Y = (Box.Min.Y + Box.Max.Y) * 0.5f;
        UUIDLocation.Z = Box.Max.Z + 0.5f;

        FString Text = "UUID : " + std::to_string(SelectedActor->GetUUID());
        TextRenderer->BuildTextMesh(Text, 0.5f, *SystemFont);

        const FMatrix BillboardWorld =
            MultipleViewportsAdapter.BuildEngineBillboardMatrix(
                ViewIndex, UUIDLocation, 1.0f, 1.0f);
        TextRenderer->OnRender(Text, BillboardWorld, 0.5f, *SystemFont,
                               ViewProjection);
      }
    }
  }

  }
  RenderCommand::EndRenderPass(ViewRenderingInfo);
}

// View Texture가 포함된 UI를 Swapchain에 합성해 표시한다.
void FEditorApplication::PresentFrame() {
  // Swapchain 렌더링
  RenderCommand::BeginRenderPass(MainWindowSC->GetRenderingInfo());

  ImGuiRenderer->Begin();

  // 로딩 화면 또는 에디터 UI 렌더링
  if (LoadingScreen && !LoadingScreen->IsFinished()) {
    LoadingScreen->Draw();
  } else {
    EditorUI->OnRender();
  }

  ImGuiRenderer->End();

  RenderCommand::EndRenderPass(MainWindowSC->GetRenderingInfo());

  // 버퍼 갱신
  MainWindowSC->SwapBuffers(0, 0);
}

// 엔진 종료에 필요한 자원 정리를 수행한다.
void FEditorApplication::Shutdown() {
  FGPUProfiler::Get().Shutdown();
  UAssetManager::Get().Shutdown();
  FRenderResourceManager::Shutdown();

  while (GUObjectArray.Num() > 0) {
    delete GUObjectArray.Last();
  }

  ImGuiRenderer->Shutdown();
  RenderDevice->Shutdown();

  // 신규 태스크 스케줄러 종료
  Tasks::FTaskScheduler::Get().Shutdown();
}

// 메인 창 크기에 맞춰 Swapchain을 갱신한다.
void FEditorApplication::HandleMainWindow() {
  if (MainWindow->CheckResized()) {
    MainWindowSC->Resize(MainWindow->GetWidth(), MainWindow->GetHeight());
  }
}

// 선택과 Gizmo 참조를 정리한 뒤 Actor를 삭제한다.
void FEditorApplication::DeleteActor(AActor *Actor) {
  if (!Actor)
    return;

  OutlinerPanel->SelectActor(nullptr);

  Actor->Destroy();
}

// 씬 변경으로 무효화된 에디터의 선택 참조를 모두 해제한다.
void FEditorApplication::ResetSceneSelection() {
  Gizmo->SetTarget(nullptr);
  Outline->SetTarget(nullptr);
  DetailsPanel->SetTarget(nullptr);
  OutlinerPanel->SelectActor(nullptr);
}

// 새 씬 생성이 성공하면 에디터 선택 상태를 초기화한다.
void FEditorApplication::CreateNewScene() {
  if (!FEditorFileUtils::NewScene(World))
    return;

  ResetSceneSelection();
  MultipleViewportsAdapter.ResetSoftwareOcclusionScene();
}

// 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void FEditorApplication::OpenScene() {
  if (!FEditorFileUtils::LoadScene(World))
    return;

  ResetSceneSelection();
  MultipleViewportsAdapter.ResetSoftwareOcclusionScene();
}

// 대회용 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void FEditorApplication::OpenCompetitionScene() {
  if (!FEditorFileUtils::LoadCompetitionScene(World))
    return;

  ResetSceneSelection();
  MultipleViewportsAdapter.ResetSoftwareOcclusionScene();
}

// 공통 파일 유틸리티로 현재 씬을 저장한다.
void FEditorApplication::SaveCurrentScene() {
  FEditorFileUtils::SaveScene(World);
}

// 공통 파일 유틸리티로 새 경로에 씬을 저장한다.
void FEditorApplication::SaveSceneAs() { FEditorFileUtils::SaveSceneAs(World); }
