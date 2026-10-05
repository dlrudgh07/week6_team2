#pragma once

#include "MovementComponent.h"

class URotatingMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

  public:
	virtual void TickComponent(float DeltaTime) override;

  private:
	FRotator RotationRate;
	FVector PivotTranslation;
	bool bRotationInLocalSpace = true;
};
