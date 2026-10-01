#include "EnginePCH.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Viewports/MultipleViewportsAdapter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"


#include "World/World.h"

// 종료 시 렌더·에디터·뷰포트 설정을 함께 저장한다.
FSettingsPanel::~FSettingsPanel()
{
	// 종료 시 다른 객체를 참조하지 않고 보존한 설정만 저장한다.
	ViewportAdapter = nullptr;
	SaveSettings();
}

// 설정 파일을 읽어 패널 상태를 초기화한다.
bool FSettingsPanel::Init()
{
	LoadSettings();
	return true;
}

// 설정 패널의 프레임 갱신 진입점이다.
void FSettingsPanel::Tick(float DeltaTime)
{
}

// ImGui 조작으로 렌더·카메라 설정과 저장·복원 요청을 처리한다.
void FSettingsPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("Settings");

	//////////////////////////////////////////////////////////

	ImGui::SeparatorText("Rendering");

	ImGui::TextDisabled("Editor VSync: Off | Tearing: %s", bTearingSupported ? "Supported" : "Unavailable");
	ImGui::TextDisabled("Fill mode is configured per viewport.");

	ImGui::Spacing();

	ImGui::Checkbox("Draw Primitives", &Settings.bDrawPrimitives);

	ImGui::Spacing();
	ImGui::SeparatorText("Viewport Guides");

	ImGui::Checkbox("Show Grid", &Settings.bDrawGrid);

	ImGui::SameLine(140.0f);
	ImGui::TextDisabled("Spacing");
	ImGui::SameLine();

	if (!Settings.bDrawGrid)
		ImGui::BeginDisabled();

	ImGui::SetNextItemWidth(100.0f);
	ImGui::SliderInt("##GridSpacing", &Settings.GridSpacing, 1, 100);

	if (!Settings.bDrawGrid)
		ImGui::EndDisabled();

	ImGui::Checkbox("Show Axis", &Settings.bDrawAxis);

	//////////////////////////////////////////////////////////

	if (ViewportAdapter)
	{
		ImGui::Spacing();
		ImGui::SeparatorText("Occlusion Culling");

		if (ViewportAdapter)
		{
			const char* OcclusionModes[]{"Disabled", "Linear Subcells", "Hierarchical Subcells", "Static BVH + HZB", "Static BVH Frustum Only", "GPU Compute (Frustum + HZB)"};

			ImGui::TextDisabled("Mode");
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::Combo("##OcclusionMode", &Settings.SoftwareOcclusionMode, OcclusionModes, 6);

			const bool bOcclusionEnabled = Settings.SoftwareOcclusionMode != 0;

			if (!bOcclusionEnabled)
				ImGui::BeginDisabled();

			ImGui::Spacing();
			ImGui::TextDisabled("Occluder");

			const char* OccluderGeometryModes[]{"Auto by Distance", "AABB", "Mesh Triangles"};

			ImGui::SetNextItemWidth(-1.0f);
			ImGui::Combo("##OccluderGeometry", &Settings.SoftwareOccluderGeometry, OccluderGeometryModes, 3);

			ImGui::Spacing();
			ImGui::TextDisabled("Rasterization");

			const int32 TileSizes[]{4, 8, 16};
			int32 TileSelection = Settings.SoftwareOcclusionTileSize == 4 ? 0 : Settings.SoftwareOcclusionTileSize == 16 ? 2 : 1;

			ImGui::SetNextItemWidth(140.0f);
			if (ImGui::Combo("Tile Size", &TileSelection, "4 px\0 8 px\0 16 px\0"))
				Settings.SoftwareOcclusionTileSize = TileSizes[TileSelection];

			ImGui::SameLine();

			ImGui::SetNextItemWidth(140.0f);
			ImGui::SliderInt("Min Tiles", &Settings.SoftwareOcclusionMinimumTiles, 1, 64);

			ImGui::Spacing();
			ImGui::TextDisabled("Budget");

			ImGui::SetNextItemWidth(220.0f);
			ImGui::SliderInt("Triangle Budget", &Settings.SoftwareOcclusionTriangleBudget, 10000, 1000000);

			ImGui::SetNextItemWidth(220.0f);
			ImGui::SliderFloat("CPU Budget", &Settings.SoftwareOcclusionCpuBudgetMs, 0.0f, 16.0f, "%.1f ms");

			ImGui::SetNextItemWidth(220.0f);
			ImGui::SliderFloat("Box Distance", &Settings.SoftwareOcclusionBoxDistanceThreshold, 0.0f, 100.0f, "%.1f m");

			ImGui::Checkbox("Debug Bounds", &Settings.bSoftwareOcclusionDebugBounds);

			if (!bOcclusionEnabled)
				ImGui::EndDisabled();

			FSoftwareOcclusionSettings Occlusion = ViewportAdapter->GetSoftwareOcclusionSettings();

			Occlusion.Mode = static_cast<ESoftwareOcclusionMode>(std::clamp(Settings.SoftwareOcclusionMode, 0, 5));

			Occlusion.OccluderGeometry = static_cast<ESoftwareOccluderGeometry>(std::clamp(Settings.SoftwareOccluderGeometry, 0, 2));

			Occlusion.TileSize = Settings.SoftwareOcclusionTileSize;
			Occlusion.MinimumOccluderTiles = Settings.SoftwareOcclusionMinimumTiles;
			Occlusion.TriangleBudget = static_cast<uint32>(std::max(0, Settings.SoftwareOcclusionTriangleBudget));
			Occlusion.CpuTimeBudgetMs = Settings.SoftwareOcclusionCpuBudgetMs;
			Occlusion.BoxOccluderDistanceThreshold = Settings.SoftwareOcclusionBoxDistanceThreshold;
			Occlusion.bDebugBounds = Settings.bSoftwareOcclusionDebugBounds;

			ViewportAdapter->SetSoftwareOcclusionSettings(Occlusion);
		}
	}

	//////////////////////////////////////////////////////////

	// 에디터 수치 설정 (Values)
	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Editor Settings");

	UCameraComponent* CamCom = World->GetMainCamera()->GetCameraComponent();

	ImGui::SetNextItemWidth(200.0f);
	ImGui::SliderFloat("Camera Rotate Sensitivity", &Settings.MouseSensitivity, 0.01f, 1.0f, "%.2f");
	CamCom->SetMouseSensitivity(Settings.MouseSensitivity);

	ImGui::SetNextItemWidth(200.0f);
	ImGui::SliderFloat("Camera Speed", &Settings.CameraSpeed, 0.1f, 10.0f, "%.2f");
	CamCom->SetMoveSpeed(Settings.CameraSpeed);

	//////////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Load Settings");

	if (ImGui::Button("Load Settings"))
	{
		LoadSettings();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Load settings from editor.ini");
	}

	ImGui::SameLine();

	if (ImGui::Button("Save Settings"))
	{
		SaveSettings();
	}

	//////////////////////////////////////////////////////////

	ImGui::End();
}

// 알려진 설정 섹션을 editor.ini에 함께 기록해 뷰포트 값 유실을 막는다.
bool FSettingsPanel::SaveSettings() const
{
	FEditorSettings Snapshot = Settings;
	ReadViewportSettings(Snapshot);
	std::ofstream File("editor.ini");

	if (!File.is_open())
	{
		LOG(Error, "Failed to open editor.ini for saving.");
		return false;
	}

	File << "[Rendering]\n";
	File << "DrawPrimitives=" << Settings.bDrawPrimitives << "\n";
	File << "DrawGrid=" << Settings.bDrawGrid << "\n";
	File << "DrawAxis=" << Settings.bDrawAxis << "\n";

	File << "SoftwareOcclusionMode=" << Snapshot.SoftwareOcclusionMode << "\n";
	File << "SoftwareOccluderGeometry=" << Snapshot.SoftwareOccluderGeometry << "\n";
	File << "SoftwareOcclusionTileSize=" << Snapshot.SoftwareOcclusionTileSize << "\n";
	File << "SoftwareOcclusionMinimumTiles=" << Snapshot.SoftwareOcclusionMinimumTiles << "\n";
	File << "SoftwareOcclusionTriangleBudget=" << Snapshot.SoftwareOcclusionTriangleBudget << "\n";
	File << "SoftwareOcclusionCpuBudgetMs=" << Snapshot.SoftwareOcclusionCpuBudgetMs << "\n";
	File << "SoftwareOcclusionBoxDistanceThreshold=" << Snapshot.SoftwareOcclusionBoxDistanceThreshold << "\n";
	File << "SoftwareOcclusionDebugBounds=" << Snapshot.bSoftwareOcclusionDebugBounds << "\n";
	File << "\n";

	File << "[Editor]\n";
	File << "CameraMoveSpeed=" << Settings.CameraSpeed << "\n";
	File << "CameraRotateSensitivity=" << Settings.MouseSensitivity << "\n";
	File << "GridSpacing=" << Settings.GridSpacing << "\n";
	File << "\n";

	File << "[MultipleViewports]\n";
	File << "Horizontal=" << Snapshot.MultipleViewportsHorizontal << "\n";
	File << "Vertical=" << Snapshot.MultipleViewportsVertical << "\n";
	File << "Layout=" << (Snapshot.bMultipleViewportsSingle ? "Single" : "Quad") << "\n";
	File << "SingleViewIndex=" << Snapshot.MultipleViewportsSingleViewIndex << "\n";

	for (int32 Index = 0; Index < 4; ++Index)
    {
        File << "View" << Index << "Fov=" << Snapshot.ViewFov[Index] << "\n";
        File << "View" << Index << "OrthoWidth=" << Snapshot.ViewOrthoWidth[Index] << "\n";
        File << "View" << Index << "Preset=" << Snapshot.ViewPreset[Index] << "\n";
        File << "View" << Index << "Wireframe=" << Snapshot.ViewWireframe[Index] << "\n";
        File << "View" << Index << "LocationX=" << Snapshot.ViewLocation[Index].X << "\n";
        File << "View" << Index << "LocationY=" << Snapshot.ViewLocation[Index].Y << "\n";
        File << "View" << Index << "LocationZ=" << Snapshot.ViewLocation[Index].Z << "\n";
        File << "View" << Index << "RotationX=" << Snapshot.ViewRotation[Index].X << "\n";
        File << "View" << Index << "RotationY=" << Snapshot.ViewRotation[Index].Y << "\n";
        File << "View" << Index << "RotationZ=" << Snapshot.ViewRotation[Index].Z << "\n";
        File << "View" << Index << "RotationW=" << Snapshot.ViewRotation[Index].W << "\n";
    }
    File.close();
	return true;
}

// 파일의 키를 파싱해 렌더·카메라·레이아웃 설정을 복원한다.
bool FSettingsPanel::LoadSettings()
{
	std::ifstream File("editor.ini");
	if (!File.is_open())
	{
		return false;
	}

	uint8 LocationComponentMasks[4]{};
	uint8 RotationComponentMasks[4]{};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Settings.bViewLocationSaved[Index] = false;
		Settings.bViewRotationSaved[Index] = false;
	}

	FString Line;
	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == ';' || Line[0] == '[') continue;

		std::istringstream Iss(Line);
		FString Key;
		if (std::getline(Iss, Key, '='))
		{
			FString ValueStr;
			if (std::getline(Iss, ValueStr))
			{
				// 새 View 키는 NaN·잘못된 숫자·범위 밖 값을 무시한다.
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    FString Prefix = "View";
                    Prefix += static_cast<char>('0' + Index);
                    std::istringstream Number(ValueStr);
                    float Value = 0;
                    if (!(Number >> Value) || !std::isfinite(Value)) continue;
                    if (Key == Prefix + "Fov" && Value >= 1 && Value <= 179)
                        Settings.ViewFov[Index] = Value;
                    else if (Key == Prefix + "OrthoWidth" && Value >= 0.01f)
                        // 파일 값은 절대 상한으로 먼저 제한하고, 실제 View 비율 상한은 Adapter가 적용한다.
                        Settings.ViewOrthoWidth[Index] = FMath::Clamp(Value, 0.01f, 1000000.0f);
                    else if (Key == Prefix + "Preset" && Value >= 0 && Value <= 7 && Value == static_cast<int32>(Value))
                        Settings.ViewPreset[Index] = static_cast<int32>(Value);
                    else if (Key == Prefix + "Wireframe" && (Value == 0 || Value == 1))
                        Settings.ViewWireframe[Index] = static_cast<int32>(Value);
                    else if (Key == Prefix + "LocationX")
                    {
                        Settings.ViewLocation[Index].X = Value;
                        LocationComponentMasks[Index] |= 1 << 0;
                    }
                    else if (Key == Prefix + "LocationY")
                    {
                        Settings.ViewLocation[Index].Y = Value;
                        LocationComponentMasks[Index] |= 1 << 1;
                    }
                    else if (Key == Prefix + "LocationZ")
                    {
                        Settings.ViewLocation[Index].Z = Value;
                        LocationComponentMasks[Index] |= 1 << 2;
                    }
                    else if (Key == Prefix + "RotationX")
                    {
                        Settings.ViewRotation[Index].X = Value;
                        RotationComponentMasks[Index] |= 1 << 0;
                    }
                    else if (Key == Prefix + "RotationY")
                    {
                        Settings.ViewRotation[Index].Y = Value;
                        RotationComponentMasks[Index] |= 1 << 1;
                    }
                    else if (Key == Prefix + "RotationZ")
                    {
                        Settings.ViewRotation[Index].Z = Value;
                        RotationComponentMasks[Index] |= 1 << 2;
                    }
                    else if (Key == Prefix + "RotationW")
                    {
                        Settings.ViewRotation[Index].W = Value;
                        RotationComponentMasks[Index] |= 1 << 3;
                    }
                }
				if (Key == "DrawPrimitives") Settings.bDrawPrimitives = std::stoi(ValueStr);
				else if (Key == "DrawGrid")	Settings.bDrawGrid = std::stoi(ValueStr);
				else if (Key == "DrawAxis")	Settings.bDrawAxis = std::stoi(ValueStr);

				else if (Key == "SoftwareOcclusionMode") Settings.SoftwareOcclusionMode = std::clamp(std::stoi(ValueStr), 0, 5);
				else if (Key == "SoftwareOccluderGeometry") Settings.SoftwareOccluderGeometry = std::clamp(std::stoi(ValueStr), 0, 2);
				else if (Key == "SoftwareOcclusionTileSize")
				{
					const int32 Value = std::stoi(ValueStr);
					Settings.SoftwareOcclusionTileSize = Value == 4 || Value == 16 ? Value : 8;
				}
				else if (Key == "SoftwareOcclusionMinimumTiles") Settings.SoftwareOcclusionMinimumTiles = std::max(1, std::stoi(ValueStr));
				else if (Key == "SoftwareOcclusionTriangleBudget") Settings.SoftwareOcclusionTriangleBudget = std::max(0, std::stoi(ValueStr));
				else if (Key == "SoftwareOcclusionCpuBudgetMs") Settings.SoftwareOcclusionCpuBudgetMs = std::max(0.0f, std::stof(ValueStr));
				else if (Key == "SoftwareOcclusionBoxDistanceThreshold") Settings.SoftwareOcclusionBoxDistanceThreshold = std::max(0.0f, std::stof(ValueStr));
				else if (Key == "SoftwareOcclusionDebugBounds") Settings.bSoftwareOcclusionDebugBounds = std::stoi(ValueStr) != 0;

				else if (Key == "CameraMoveSpeed") Settings.CameraSpeed = std::stof(ValueStr);
				else if (Key == "CameraRotateSensitivity") Settings.MouseSensitivity = std::stof(ValueStr);
				else if (Key == "GridSpacing") Settings.GridSpacing = std::stoi(ValueStr);
				else if (Key == "Horizontal") Settings.MultipleViewportsHorizontal = std::stof(ValueStr);
				else if (Key == "Vertical") Settings.MultipleViewportsVertical = std::stof(ValueStr);
				else if (Key == "Layout") Settings.bMultipleViewportsSingle = ValueStr == "Single";
				else if (Key == "SingleViewIndex")
				{
					const int32 ViewIndex = std::stoi(ValueStr);
					Settings.MultipleViewportsSingleViewIndex = ViewIndex >= 0 && ViewIndex < 4 ? ViewIndex : 0;
				}
			}
		}
	}

	File.close();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Settings.bViewLocationSaved[Index] = LocationComponentMasks[Index] == 0x7;
		if (RotationComponentMasks[Index] == 0xF)
		{
			const FQuat& Rotation = Settings.ViewRotation[Index];
			const float LengthSquared = Rotation.X * Rotation.X + Rotation.Y * Rotation.Y +
				Rotation.Z * Rotation.Z + Rotation.W * Rotation.W;
			if (LengthSquared > 1.0e-12f)
			{
				Settings.ViewRotation[Index] = Rotation.Normalized();
				Settings.bViewRotationSaved[Index] = true;
			}
		}
	}
	ApplyViewportSettings();
	return true;
}

// 초기화된 카메라에 파일 설정을 적용하고 미저장 값은 초기 설정으로 채운다.
void FSettingsPanel::SetViewportAdapter(FMultipleViewportsAdapter* Value)
{
    ViewportAdapter = Value;
    ApplyViewportSettings();
    CaptureViewportSettings();
}

// Transform·투영·표시·레이아웃을 저장용 설정에 복사한다.
void FSettingsPanel::ReadViewportSettings(FEditorSettings& Out) const
{
    if (!ViewportAdapter) return;
	const FSoftwareOcclusionSettings& Occlusion = ViewportAdapter->GetSoftwareOcclusionSettings();
	Out.SoftwareOcclusionMode = static_cast<int32>(Occlusion.Mode);
	Out.SoftwareOccluderGeometry = static_cast<int32>(Occlusion.OccluderGeometry);
	Out.SoftwareOcclusionTileSize = Occlusion.TileSize;
	Out.SoftwareOcclusionMinimumTiles = Occlusion.MinimumOccluderTiles;
	Out.SoftwareOcclusionTriangleBudget = static_cast<int32>(Occlusion.TriangleBudget);
	Out.SoftwareOcclusionCpuBudgetMs = Occlusion.CpuTimeBudgetMs;
	Out.SoftwareOcclusionBoxDistanceThreshold = Occlusion.BoxOccluderDistanceThreshold;
	Out.bSoftwareOcclusionDebugBounds = Occlusion.bDebugBounds;
    Out.MultipleViewportsHorizontal = ViewportAdapter->GetSplitRatio().Horizontal;
    Out.MultipleViewportsVertical = ViewportAdapter->GetSplitRatio().Vertical;
    Out.bMultipleViewportsSingle = ViewportAdapter->GetLayoutMode() == ELayoutMode::Single;
    Out.MultipleViewportsSingleViewIndex = ViewportAdapter->GetSingleViewIndex();
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const auto& Camera = ViewportAdapter->GetViewCamera(Index);
        Out.ViewFov[Index] = Camera.Projection.FovDegrees;
        Out.ViewOrthoWidth[Index] = Camera.Projection.OrthoWidth;
        Out.ViewPreset[Index] = static_cast<int32>(ViewportAdapter->GetCameraPreset(Index));
        Out.ViewWireframe[Index] = ViewportAdapter->IsViewWireframe(Index) ? 1 : 0;
        Out.ViewLocation[Index] = Camera.Transform.Location;
        Out.ViewRotation[Index] = Camera.Transform.Rotation;
        Out.bViewLocationSaved[Index] = true;
        Out.bViewRotationSaved[Index] = true;
    }
}

// 종료 시 Adapter를 읽지 않도록 살아 있는 동안 저장용 설정을 갱신한다.
void FSettingsPanel::CaptureViewportSettings() { ReadViewportSettings(Settings); }

void FSettingsPanel::ApplyViewportSettings()
{
    if (!ViewportAdapter) return;
	FSoftwareOcclusionSettings Occlusion = ViewportAdapter->GetSoftwareOcclusionSettings();
	Occlusion.Mode = static_cast<ESoftwareOcclusionMode>(std::clamp(Settings.SoftwareOcclusionMode, 0, 5));
	Occlusion.OccluderGeometry = static_cast<ESoftwareOccluderGeometry>(std::clamp(Settings.SoftwareOccluderGeometry, 0, 2));
	Occlusion.TileSize = Settings.SoftwareOcclusionTileSize;
	Occlusion.MinimumOccluderTiles = Settings.SoftwareOcclusionMinimumTiles;
	Occlusion.TriangleBudget = static_cast<uint32>(std::max(0, Settings.SoftwareOcclusionTriangleBudget));
	Occlusion.CpuTimeBudgetMs = Settings.SoftwareOcclusionCpuBudgetMs;
	Occlusion.BoxOccluderDistanceThreshold = Settings.SoftwareOcclusionBoxDistanceThreshold;
	Occlusion.bDebugBounds = Settings.bSoftwareOcclusionDebugBounds;
	ViewportAdapter->SetSoftwareOcclusionSettings(Occlusion);
    ViewportAdapter->SetSplitRatio({Settings.MultipleViewportsHorizontal, Settings.MultipleViewportsVertical});
    ViewportAdapter->SetSingleViewIndex(Settings.MultipleViewportsSingleViewIndex);
    ViewportAdapter->SetLayoutMode(Settings.bMultipleViewportsSingle ? ELayoutMode::Single : ELayoutMode::QuadSplit);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (Settings.ViewPreset[Index] >= 0 && Settings.ViewPreset[Index] <= 7)
            ViewportAdapter->ApplyCameraPreset(Index, static_cast<EMultipleViewportsCameraPreset>(Settings.ViewPreset[Index]));
        FViewCamera Camera = ViewportAdapter->GetViewCamera(Index);
        if (Settings.ViewFov[Index] > 0) Camera.Projection.FovDegrees = Settings.ViewFov[Index];
        if (Settings.ViewOrthoWidth[Index] > 0) Camera.Projection.OrthoWidth = Settings.ViewOrthoWidth[Index];
        if (Settings.bViewLocationSaved[Index]) Camera.Transform.Location = Settings.ViewLocation[Index];
        if (Settings.bViewRotationSaved[Index]) Camera.Transform.Rotation = Settings.ViewRotation[Index];
        ViewportAdapter->ApplyCameraProperties(Index, Camera, false);
    }
}
