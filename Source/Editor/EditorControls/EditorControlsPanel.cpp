#include "EnginePCH.h"

#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/Viewports/MultipleViewportsAdapter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "World/World.h"

#include "Input/InputSystem.h"

#include <cmath>

// 패널 초기화 성공을 반환한다.
bool FEditorControlsPanel::Init()
{
	return true;
}

// Space 입력으로 기즈모 편집 모드를 순환한다.
void FEditorControlsPanel::Tick(float DeltaTime)
{
	if (ImGui::GetIO().WantTextInput)
		return;

	if (Gizmo && FInputSystem::IsKeyPressed(EKeyCode::Space))
	{
		const int NextMode = (static_cast<int>(Gizmo->GetMode()) + 1) % 3;
		Gizmo->SetMode(static_cast<EGizmoMode>(NextMode));
	}
}

FVector FEditorControlsPanel::GetSpawnOrigin() const 
{
	const FViewCamera& Camera = ViewportAdapter->GetEditorViewCamera();

	FVector CameraLocation = Camera.Transform.Location;
	FQuat CameraRotation(Camera.Transform.Rotation.X, Camera.Transform.Rotation.Y, Camera.Transform.Rotation.Z, Camera.Transform.Rotation.W);
	FVector CameraForward = CameraRotation.GetForwardVector();

	float SpawnDistance = 5.0f;
	return CameraLocation + CameraForward * SpawnDistance;
}

// 선택한 클래스의 액터를 월드에 생성한다.
void FEditorControlsPanel::AddActor(uint32 Index)
{
	if (!World || !ViewportAdapter || Index >= static_cast<uint32>(Classes.Num()))
		return;

	FTransform SpawnTransform;
	SpawnTransform.Location = GetSpawnOrigin();
	World->SpawnActor(Classes[Index], NAME_None, &SpawnTransform);
}

void FEditorControlsPanel::AddActorsGrid(uint32 Index)
{
	if (!World || !ViewportAdapter || Index >= static_cast<uint32>(Classes.Num()))
		return;

	FVector GridOrigin = GetSpawnOrigin();

	for (int32 Z = 0; Z < GridCount[2]; ++Z)
	{
		for (int32 Y = 0; Y < GridCount[1]; ++Y)
		{
			for (int32 X = 0; X < GridCount[0]; ++X)
			{
				const FVector Offset(X * GridSpacing, Y * GridSpacing, Z * GridSpacing);

				FTransform SpawnTransform;
				SpawnTransform.Location = GridOrigin + Offset;

				World->SpawnActor(Classes[Index], NAME_None, &SpawnTransform);
			}
		}
	}
}

// 액터 생성·카메라 속성·기즈모·경로 추적 UI를 그린다.
void FEditorControlsPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
	ImGui::Begin("Editor Controls");
	//ImGui::Dummy(ImVec2(0.0f, SectionGap));


	// 5주차에서 Control Panel의 FPS 표시는 임시 삭제, 추후 복원 가능
	//char FpsText[64];
	//std::snprintf(FpsText, sizeof(FpsText), "FPS %.1f   %.1f ms", 1.0f / DeltaTime, DeltaTime * 1000.0f);
	//float TextWidth = ImGui::CalcTextSize(FpsText).x;
	//float CursorX = ImGui::GetCursorPosX();
	//float AvailableWidth = ImGui::GetContentRegionAvail().x;

	//ImGui::SetCursorPosX(CursorX + AvailableWidth - TextWidth);
	//ImGui::TextDisabled("%s", FpsText);

	//////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Actor Spawn");

	// Actor 종류 ComboBox로 골라 하나씩 스폰
	const float SpawnButtonWidth = 70.0f;

	char CountText[32];
	std::snprintf(CountText, sizeof(CountText), "%d Actors", World ? World->GetActorNum() : 0);

	const float CountWidth = ImGui::CalcTextSize(CountText).x;
	const float Available = ImGui::GetContentRegionAvail().x;
	const float ItemSpacing = ImGui::GetStyle().ItemSpacing.x;

	const float ComboWidth = Available - SpawnButtonWidth - CountWidth - ItemSpacing * 2.0f;

	ImGui::SetNextItemWidth(ComboWidth);
	ImGui::Combo("##ActorType", &SelectedIndex, Items, IM_ARRAYSIZE(Items));

	ImGui::SameLine();

	if (ImGui::Button("Spawn", ImVec2(SpawnButtonWidth, 0)))
	{
		AddActor(SelectedIndex);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s", CountText);

	// StaticMeshActor를 Grid 간격 맞춰서 한꺼번에 소환
	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));

	if (ImGui::CollapsingHeader("Grid Spawn"))
	{
		if (ImGui::BeginTable("GridSpawnSettings", 2))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Count");

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputInt3("##GridCount", GridCount);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Spacing");

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::DragFloat("##GridSpacing", &GridSpacing, 0.1f, 0.1f, 100.0f, "%.1f");

			const bool bValidGrid = GridCount[0] > 0 && GridCount[1] > 0 && GridCount[2] > 0 && GridSpacing > 0.0f;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Total");

			ImGui::TableSetColumnIndex(1);

			if (bValidGrid)
			{
				const int32 TotalCount = GridCount[0] * GridCount[1] * GridCount[2];

				ImGui::TextDisabled("%d Actors", TotalCount);
			}
			else
			{
				ImGui::TextDisabled("Invalid");
			}

			ImGui::EndTable();

			ImGui::Dummy(ImVec2(0.0f, SubsectionGap));

			ImGui::BeginDisabled(!bValidGrid);

			if (ImGui::Button("Spawn Grid", ImVec2(-1.0f, 0)))
			{
				AddActorsGrid(SelectedIndex);
			}
			ImGui::EndDisabled();
		}
	}

	//////////////////////////////////////////////////////

	DrawCameraProperties();

	//////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Gizmo");

	if (Gizmo && ImGui::BeginTable("GizmoControls", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Mode");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		GizmoSelectedIndex = static_cast<int32>(Gizmo->GetMode());
		if (ImGui::Combo("##GizmoMode", &GizmoSelectedIndex, GizmoItems, IM_ARRAYSIZE(GizmoItems)))
		{
			Gizmo->SetMode(static_cast<EGizmoMode>(GizmoSelectedIndex));
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Space");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		if (ImGui::Combo("##GizmoSpace", &SpaceSelectedIndex, SpaceItems, IM_ARRAYSIZE(SpaceItems)))
		{
			Gizmo->SetSpace(static_cast<EGizmoSpace>(SpaceSelectedIndex));
		}

		ImGui::EndTable();
	}

	//////////////////////////////////////////////////////

	//////////////////////////////////////////////////////

	ImGui::End();
}

// 실제 View 속성을 편집하며 패널 이동 후에도 마지막 View를 유지하고 변경된 값만 반영한다.
void FEditorControlsPanel::DrawCameraProperties()
{
	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Viewport");

	UCameraComponent* CamCom = World && World->GetMainCamera() ? World->GetMainCamera()->GetCameraComponent() : nullptr;
	if (!ViewportAdapter && !CamCom) return;

	int32 CameraViewIndex = 0;
	FViewCamera Camera{};

	if (ViewportAdapter)
	{
		const char* ViewNames[] = { "View 0", "View 1", "View 2", "View 3" };
		CameraViewIndex = ViewportAdapter->GetEditorViewIndex();

		const bool bSingle = ViewportAdapter->GetLayoutMode() == ELayoutMode::Single;
		if (bSingle) CameraViewIndex = ViewportAdapter->GetSingleViewIndex();

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::BeginDisabled(bSingle);

		if (ImGui::Combo("##CameraView", &CameraViewIndex, ViewNames, 4))
		{
			ViewportAdapter->SetEditorViewIndex(CameraViewIndex);
		}
		ImGui::EndDisabled();

		Camera = ViewportAdapter->GetViewCamera(CameraViewIndex);
	}

	bool bChanged = false;
	bool bOrthogonal = ViewportAdapter ? Camera.Projection.Mode == EProjectionMode::Orthographic : CamCom->GetIsOrthogonal();

	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));
	ImGui::TextDisabled("Projection");

	// Camera Projection Table
	if (ImGui::BeginTable("Camera Projection", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// FOV 또는 Ortho Width
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text(bOrthogonal ? "Ortho Width" : "FOV");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		if (bOrthogonal)
		{
			float OrthoWidth = ViewportAdapter ? Camera.Projection.OrthoWidth : CamCom->GetOrthoWidth();
			if (ImGui::DragFloat("##OrthoWidth", &OrthoWidth, 0.1f) && std::isfinite(OrthoWidth))
			{
				OrthoWidth = FMath::Clamp(OrthoWidth, 0.01f, 1000000.0f);

				if (ViewportAdapter)
				{
					Camera.Projection.OrthoWidth = OrthoWidth;
					bChanged = true;
				}
				else
				{
					CamCom->SetOrthoWidth(OrthoWidth);
				}
			}
		}
		else
		{
			float FOV = ViewportAdapter ? Camera.Projection.FovDegrees : CamCom->GetFieldOfView();
			if (ImGui::DragFloat("##FOV", &FOV, 0.1f) && std::isfinite(FOV))
			{
				FOV = FMath::Clamp(FOV, 1.0f, 179.0f);

				if (ViewportAdapter)
				{
					Camera.Projection.FovDegrees = FOV;
					bChanged = true;
				}
				else
				{
					CamCom->SetFieldOfView(FOV);
				}
			}
		}

		// Near / Far
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Near");

		ImGui::TableSetColumnIndex(1);
		float Width = ImGui::GetContentRegionAvail().x;
		float Near = ViewportAdapter ? Camera.Projection.NearClip : CamCom->GetNearZ();

		ImGui::SetNextItemWidth(-1.0f);
		bool Changed = ImGui::InputFloat("##Near", &Near);

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Far");

		ImGui::TableSetColumnIndex(1);
		float Far = ViewportAdapter ? Camera.Projection.FarClip : CamCom->GetFarZ();
		ImGui::SetNextItemWidth(-1.0f);
		Changed |= ImGui::InputFloat("##Far", &Far);

		if (Changed && std::isfinite(Near) && std::isfinite(Far) && Near > 0 && Far > Near)
		{
			if (ViewportAdapter)
			{
				Camera.Projection.NearClip = Near;
				Camera.Projection.FarClip = Far;
				bChanged = true;
			}
			else
			{
				CamCom->SetNearZ(Near);
				CamCom->SetFarZ(Far);
			}
		}
		ImGui::EndTable();
	}

	// Camera Transform Table
	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));
	ImGui::TextDisabled("Transform");

	bool bLocationChanged = false;
	bool bRotationChanged = false;

	if (ImGui::BeginTable("Camera Transform", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();

		// Camera Location
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Location");

		ImGui::TableSetColumnIndex(1);
		float Available = ImGui::GetContentRegionAvail().x;
		float ItemWidth = (Available - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

		FVector Location = ViewportAdapter ?
			FVector(Camera.Transform.Location.X, Camera.Transform.Location.Y, Camera.Transform.Location.Z) :
			CamCom->GetRelativeLocation();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationX", &Location.X, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationY", &Location.Y, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationZ", &Location.Z, 0.1f);

		if (bLocationChanged && std::isfinite(Location.X) && std::isfinite(Location.Y) && std::isfinite(Location.Z))
		{
			if (ViewportAdapter)
			{
				Camera.Transform.Location = { Location.X, Location.Y, Location.Z };
				bChanged = true;
			}
			else CamCom->SetRelativeLocation(Location);
		}

		ImGui::TableNextRow();

		// Camera Rotation
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Rotation");

		ImGui::TableSetColumnIndex(1);
		FRotator Rotation = ViewportAdapter ?
			FQuat(Camera.Transform.Rotation.X, Camera.Transform.Rotation.Y, Camera.Transform.Rotation.Z, Camera.Transform.Rotation.W).ToFRotator() :
			CamCom->GetRelativeRotation();

		ImGui::SetNextItemWidth(ItemWidth);
		bRotationChanged |= ImGui::DragFloat("##Pitch", &Rotation.Pitch, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bRotationChanged |= ImGui::DragFloat("##Yaw", &Rotation.Yaw, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		ImGui::BeginDisabled(!bOrthogonal);
		bRotationChanged |= ImGui::DragFloat("##Roll", &Rotation.Roll, 0.1f);
		ImGui::EndDisabled();

		if (bRotationChanged && std::isfinite(Rotation.Pitch) && std::isfinite(Rotation.Yaw) && std::isfinite(Rotation.Roll))
		{
			if (ViewportAdapter)
			{
				if (!bOrthogonal)
				{
					Rotation.Pitch = FMath::Clamp(Rotation.Pitch, -89.0f, 89.0f);
					Rotation.Roll = 0.0f;
				}
				const FQuat Q = Rotation.Quaternion();
				Camera.Transform.Rotation = { Q.X, Q.Y, Q.Z, Q.W };
				bChanged = true;
			}
			else CamCom->SetRelativeRotation(Rotation);
		}


		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Move Speed");
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::SliderFloat("##MoveSpeed", &CameraSpeed, 1.0f, 200.0f, "%.1f");


		ImGui::EndTable();
	}
	if (ViewportAdapter && bChanged)
	{
		ViewportAdapter->ApplyCameraProperties(CameraViewIndex, Camera, bRotationChanged);
	}
}
