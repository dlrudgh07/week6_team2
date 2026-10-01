#pragma once
#include "Editor/EditorUI/EditorPanel.h"

#include "Editor/Gizmo/Gizmo.h"

#include "GameFramework/Actor/ParticleActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Actor/LightActor.h"
#include "GameFramework/Actor/TextRenderActor.h"

class FMultipleViewportsAdapter;

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

	const char* Items[4] ={"StaticMesh","Particle","Text","Light"};

	FGizmo* Gizmo = nullptr;
	int32 GizmoSelectedIndex = 0;
	const char* GizmoItems[3] ={"Location","Rotation","Scale"};

	int32 SpaceSelectedIndex = 0;
	const char* SpaceItems[2] ={"Local","World"};

	TArray<UClass*> Classes
	{
		AStaticMeshActor::StaticClass(),
		AParticleActor::StaticClass(),
		ATextRenderActor::StaticClass(),
		ALightActor::StaticClass(),
	};

    void SetViewportAdapter(FMultipleViewportsAdapter* InAdapter) { ViewportAdapter = InAdapter; }

private:
	static constexpr float SectionGap = 10.0f;
	static constexpr float SubsectionGap = 4.0f;

	int32 GridCount[3] = {2, 2, 2};
	float GridSpacing = 2.0f;
	FVector GetSpawnOrigin() const;
	void AddActorsGrid(uint32 Index);

    void DrawCameraProperties();
    FMultipleViewportsAdapter* ViewportAdapter = nullptr;

};
