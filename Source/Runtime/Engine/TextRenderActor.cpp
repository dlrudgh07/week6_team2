#include "EnginePCH.h"
#include "TextRenderActor.h"

ATextRenderActor::ATextRenderActor()
{
	TextRenderComponent = CreateDefaultSubobject<UTextRenderComponent>("TextRenderComponent");
	SetRootComponent(TextRenderComponent);
}

ATextRenderActor::~ATextRenderActor()
{
}

void ATextRenderActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	TextRenderComponent = FindComponentByClass<UTextRenderComponent>();
}
