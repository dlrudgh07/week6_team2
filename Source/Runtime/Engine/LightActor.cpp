#include "EnginePCH.h"
#include "LightActor.h"

ALightActor::ALightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);

	// 아이콘용 메시/머티리얼. Plane은 FVertex 포맷이라 DefaultShader로 그대로 그려진다.
	// 라이트 아이콘 텍스처가 준비되면 Property 창에서 머티리얼 슬롯에 끼우면 된다.

	SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>("USpotLightComponent");
	SpotLightComponent->SetupAttachment(BillboardComponent);
}
