#pragma once

#include "Editor/EditorUI/EditorPanel.h"

class UWorld;
class USceneComponent;

class FDetailsPanel : public IEditorPanel
{
public:
	FDetailsPanel() = default;
	~FDetailsPanel();

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Details"; }

	void SetTarget(USceneComponent* InTargetOrNull) { Target = InTargetOrNull; }

	void SetWorld(UWorld* InWorld) { World = InWorld; }

	ImFont* GetCustomFont() { return CustomFont; }

private:
	UWorld* World = nullptr;
	USceneComponent* Target = nullptr;
	ImFont* CustomFont = nullptr;
};

