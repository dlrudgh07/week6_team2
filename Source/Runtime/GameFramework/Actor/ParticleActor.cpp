#include "EnginePCH.h"
#include "ParticleActor.h"

// Todo: subuv
AParticleActor::AParticleActor()
{
	ParticleComponent = CreateDefaultSubobject<UParticleSubUVComponent>("UParticleSubUVComponent");
	SetRootComponent(ParticleComponent);

	ParticleComponent->SetSubUVSize(8, 8);
	ParticleComponent->SetFrameRate(12.0f);
}

UParticleSubUVComponent* AParticleActor::GetParticleComponent() const
{
	return static_cast<UParticleSubUVComponent*>(RootComponent);
}


