#pragma once

#include "GameFramework/Actor.h"
#include "Particles/ParticleSubUVComponent.h"

// Todo: subuv
class AEmitter : public AActor
{
	DECLARE_CLASS(AEmitter, AActor)

	REFLECT_START(ClassName)
		REFLECT_END()

public:
	AEmitter();
	UParticleSubUVComponent* GetParticleComponent() const;
	virtual void DuplicateSubObjects() override;

  private:
	UParticleSubUVComponent* ParticleComponent;
};
