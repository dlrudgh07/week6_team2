#pragma once

#include <format>
#include "Editor/EditorUI/EditorPanel.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
class FMultipleViewportsAdapter;

class UWorld;

struct FEditorSettings
{
	// 에디터 렌더 옵션과 Multiple Viewports 레이아웃의 영구 설정을 한곳에 담는다.
	// Toggles
	bool bDrawPrimitives = true;
	bool bDrawGrid = true;
	bool bDrawAxis = true;

	int32 SoftwareOcclusionMode = 0;
	int32 SoftwareOccluderGeometry = 0;
	int32 SoftwareOcclusionTileSize = 8;
	int32 SoftwareOcclusionMinimumTiles = 16;
	int32 SoftwareOcclusionTriangleBudget = 500000;
	float SoftwareOcclusionCpuBudgetMs = 4.0f;
	bool bSoftwareOcclusionDebugBounds = false;
	float SoftwareOcclusionBoxDistanceThreshold = 10.0f;

	// Values
	float CameraSpeed = 1.0f;
	float MouseSensitivity = 1.0f;
	int32 GridSpacing = 1;
	float MultipleViewportsHorizontal = 0.5f;
	float MultipleViewportsVertical = 0.5f;
	bool bMultipleViewportsSingle = false;
	int32 MultipleViewportsSingleViewIndex = 0;
	// 미저장 표식은 초기 카메라 값을 유지한다. 위치·회전은 저장하지 않는다.
	float ViewFov[4]{};
	float ViewOrthoWidth[4]{};
	int32 ViewPreset[4]{-1, -1, -1, -1};
	int32 ViewWireframe[4]{-1, -1, -1, -1};
	// View별 카메라 Transform은 Quaternion 그대로 저장해 재실행 후 같은 시점을 복원한다.
	// 구형 editor.ini에 키가 없으면 Saved가 false로 남아 Adapter의 초기 Transform을 유지한다.
	FVector ViewLocation[4]{};
	FQuat ViewRotation[4]{};
	bool bViewLocationSaved[4]{};
	bool bViewRotationSaved[4]{};
};

class FSettingsPanel : public IEditorPanel
{
  public:
	FSettingsPanel() = default;
	~FSettingsPanel();

	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override
	{
		return "Settings";
	}

	void SetWorld(UWorld* InWorld)
	{
		World = InWorld;
	}
	void SetTearingSupported(bool bValue)
	{
		bTearingSupported = bValue;
	}

	const FEditorSettings& GetSettings() const
	{
		return Settings;
	}
	// 실행 중 레이아웃 변경을 editor.ini 저장 대상과 동일한 설정 객체에 반영한다.
	FEditorSettings& GetMutableSettings()
	{
		return Settings;
	}

	// 알려진 모든 설정 섹션을 한 번에 다시 써서 editor.ini 값이 서로 덮어쓰이지 않게 저장한다.
	bool SaveSettings() const;

	// editor.ini의 기존 렌더·에디터·Multiple Viewports 값을 같은 설정 객체로 복원한다.
	bool LoadSettings();

	// 초기화된 Adapter에 저장 설정을 적용한다.
	void SetViewportAdapter(FMultipleViewportsAdapter* Value);

	// 종료 저장은 마지막 프레임의 복사본을 사용해 Adapter 수명과 분리한다.
	void CaptureViewportSettings();

  private:
	static constexpr float SectionGap = 10.0f;
	static constexpr float SubsectionGap = 4.0f;

	// View별 Transform·투영·표시 설정을 읽어 저장용 스냅샷에 반영한다.
	void ReadViewportSettings(FEditorSettings& Out) const;
	void ApplyViewportSettings();
	FMultipleViewportsAdapter* ViewportAdapter = nullptr;
	bool bTearingSupported = false;
	UWorld* World;
	FEditorSettings Settings;
};
