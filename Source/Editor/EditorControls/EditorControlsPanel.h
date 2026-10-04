#pragma once
#include "Editor/EditorUI/EditorPanel.h"

#include "Editor/Gizmo/Gizmo.h"

#include "Particles/Emitter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SpotLight.h"
#include "Engine/TextRenderActor.h"
#include "Components/ExponentialHeightFog.h"
#include "Engine/RotatingActor.h"
class FMultipleViewportsAdapter;
class FPIEViewportPanel;

// 액터 생성과 카메라·기즈모 편집에 필요한 패널 상태를 보관한다.
class FEditorControlsPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Editor Controls"; }
	inline void SetGizmo(FGizmo* InGizmo) { Gizmo = InGizmo; }
	inline void SetWorld(UWorld* InWorld) { World = InWorld; }

	float DeltaTime = 1.0f;
	float CameraSpeed = 20.0f; // 기본 이동 속도
	UWorld* World = nullptr; // 월드 포인터

	void AddActor(uint32 Index);

	int32 SelectedIndex = 0;

	const char* Items[6] = {"StaticMesh", "Particle", "Text", "Light", "Fog", "Rotating(Test)"};

	FGizmo* Gizmo = nullptr;
	int32 GizmoSelectedIndex = 0;
	const char* GizmoItems[3] ={"Location","Rotation","Scale"};

	int32 SpaceSelectedIndex = 0;
	const char* SpaceItems[2] ={"Local","World"};

	TArray<UClass*> Classes
	{
		AStaticMeshActor::StaticClass(),
		AEmitter::StaticClass(),
		ATextRenderActor::StaticClass(),
		ASpotLight::StaticClass(), 
		AExponentialHeightFog::StaticClass(),
		ARotatingActor::StaticClass(),
	};

    void SetViewportAdapter(FMultipleViewportsAdapter* InAdapter) { ViewportAdapter = InAdapter; }
	void SetPIEViewportPanel(FPIEViewportPanel* PIE);


  private:
	static constexpr float SectionGap = 10.0f;
	static constexpr float SubsectionGap = 4.0f;

	int32 GridCount[3] = {2, 2, 2};
	float GridSpacing = 2.0f;
	FVector GetSpawnOrigin() const;
	void AddActorsGrid(uint32 Index);

    void DrawCameraProperties();
    FMultipleViewportsAdapter* ViewportAdapter = nullptr;
	FPIEViewportPanel* PIEPanel = nullptr;

	bool DrawStartButton(const char* str_id, float size);
	bool DrawPauseButton(const char* str_id, float size);
	bool DrawStopButton(const char* str_id, float size);
};
