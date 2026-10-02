#include "EnginePCH.h"
#include "Emitter.h"

// Todo: subuv
AEmitter::AEmitter()
{
	ParticleComponent = CreateDefaultSubobject<UParticleSubUVComponent>("UParticleSubUVComponent");
	SetRootComponent(ParticleComponent);

	ParticleComponent->SetSubUVSize(8, 8);
	ParticleComponent->SetFrameRate(12.0f);
}

UParticleSubUVComponent* AEmitter::GetParticleComponent() const
{
	return static_cast<UParticleSubUVComponent*>(RootComponent);
}


