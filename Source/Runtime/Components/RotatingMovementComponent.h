#pragma once

#include "MovementComponent.h"

class URotatingMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

	REFLECT_START(ClassName)
		PROPERTY(RotationRate)
		PROPERTY(PivotTranslation)
		PROPERTY(bRotationInLocalSpace)
	REFLECT_END()

  public:
	virtual void TickComponent(float DeltaTime) override;

  private:
	FRotator RotationRate;
	FVector PivotTranslation;
	bool bRotationInLocalSpace = true;
};
