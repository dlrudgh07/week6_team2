#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"

class ABillboardActor : public AActor
{
	DECLARE_CLASS(ABillboardActor, AActor);
public:
	ABillboardActor();
	virtual ~ABillboardActor();

	inline UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; };
private:
	UBillboardComponent* BillboardComponent;
};
