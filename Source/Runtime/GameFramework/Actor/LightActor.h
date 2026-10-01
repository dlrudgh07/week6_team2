#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/SpotLightComponent.h"

// 에디터에서 아이콘(빌보드)으로 보이고, 선택하면 스포트라이트 원뿔이 라인으로 그려진다.
class ALightActor : public AActor
{
	DECLARE_CLASS(ALightActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	ALightActor();
	virtual ~ALightActor() override = default;

	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	USpotLightComponent* GetSpotLightComponent() const { return SpotLightComponent; }

private:
	// 클릭해서 고를 수 있어야 하므로 프리미티브인 빌보드를 루트로 둔다
	UBillboardComponent* BillboardComponent = nullptr;

	USpotLightComponent* SpotLightComponent = nullptr;
};
