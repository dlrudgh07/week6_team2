#pragma once

#include "Collision/Ray.h"
#include "Math/Transform.h"
#include "Component/SceneComponent.h"

class UCameraComponent;

enum class EGizmoMode
{
	Location,
	Rotation,
	Scale,
};

enum class EGizmoSpace
{
	Local,
	World,
};

class FGizmo   // 상태 + 로직
{
public:
	// Gizmo 대상 설정
	void SetTarget(USceneComponent* InTarget) { Target = InTarget; }
	USceneComponent* GetTarget() const { return Target; }

	// Gizmo 모드(이동, 회전, 크기)
	void SetMode(EGizmoMode InMode) { Mode = InMode; }
	EGizmoMode GetMode() const { return Mode; }

	void SetSpace(EGizmoSpace InSpace) { Space = InSpace; }
	EGizmoSpace GetSpace() const { return Space; }

	// Active View의 Ray와 행렬만 사용해 Hover·Drag 상태를 갱신한다.
	void Update(const FRay& MouseRay, const FVector2& MousePos,
		const FMatrix& ViewProj, int ScreenW, int ScreenH,
		bool bMouseDown, const FVector& CameraLocation, bool bCameraOrthographic);

	bool IsUsing() const { return DraggingAxis >= 0; }
	int GetHoveredAxis() const { return HoveredAxis; }

	int PickAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH);

	int PickLinearAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH);
	int PickRotationAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH);


	void BeginDrag(int Axis, const FRay& MouseRay, const FVector2& MousePos);
	void UpdateDrag(const FRay& MouseRay, const FVector2& MousePos);
	void EndDrag();

	float ComputeAngleOnPlane(const FVector& Point, int Axis) const;

	inline FTransform GetTransform() const { return Target ? Target->GetTransform() : FTransform(); }
	inline FVector GetLocation() const { return Target ? Target->GetRelativeLocation() : FVector(0, 0, 0); }
	inline FRotator GetRotation() const { return Target ? Target->GetRelativeRotation() : FRotator(0, 0, 0); }
	inline FVector GetScale() const { return Target ? Target->GetRelativeScale3D() : FVector(0, 0, 0); }

	FVector GetRenderLocation() const;
	// 대상의 월드 위치를 표시 위치로 쓴다. 화면 크기 보정은 ComputeScreenScale이 맡는다.
	FVector GetRenderLocationForView(const FVector& CameraLocation, bool bCameraOrthographic) const;
	FVector GetCameraLocation() const;
	FVector GetAxisDirection(int Axis) const;

	// 카메라 거리·투영과 무관하게 화면상 크기가 일정하도록 Location에 적용할 월드 배율을 구한다.
	static float ComputeScreenScale(const FVector& Location, const FMatrix& ViewProj);
private:
	EGizmoMode Mode = EGizmoMode::Location;
	EGizmoSpace Space = EGizmoSpace::Local;
	FTransform Transform = FTransform();
	USceneComponent* Target = nullptr;

	int HoveredAxis = -1;
	int DraggingAxis = -1;

	FVector2 DragStartMousePos;
	FVector DragStartPoint;
	FVector DragStartLocation;
	FRotator DragStartRotation;
	FVector DragStartScale;
	float DragStartViewScale = 1.0f;
	FVector DragAxisDirection;
	FVector DragStartRenderLocation;

	FVector DragPlaneNormal;

	FVector ViewCameraLocation{};
	bool bViewCameraOrthographic = false;
	float ViewScale = 1.0f;

	float DragStartAngle;
	float RingRadius = 1.0f;
};
