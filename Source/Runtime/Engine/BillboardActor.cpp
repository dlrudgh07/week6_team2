#include "EnginePCH.h"
#include "BillboardActor.h"

ABillboardActor::ABillboardActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);
}

ABillboardActor::~ABillboardActor()
{
}

