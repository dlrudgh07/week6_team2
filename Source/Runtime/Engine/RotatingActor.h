#pragma once

#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"

// PIE 검증용 테스트 액터: 게임 월드에서만 Tick되어 Yaw 방향으로 계속 회전한다.
// bTickInEditor가 기본값(false)이므로 에디터 월드에서는 멈춰 있어야 한다.
class ARotatingActor : public AActor
{
	DECLARE_CLASS(ARotatingActor, AActor)
public:
	ARotatingActor();
	virtual ~ARotatingActor() = default;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void DuplicateSubObjects() override;

private:
	UStaticMeshComponent* MeshComponent = nullptr;
	float DegreesPerSecond = 90.0f;
};
