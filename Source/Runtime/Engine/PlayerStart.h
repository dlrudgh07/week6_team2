#pragma once

#include "GameFramework/Actor.h"

class APlayerStart :public AActor
{
	DECLARE_CLASS(APlayerStart, AActor)
  public:
	APlayerStart();
	virtual ~APlayerStart() = default;
	virtual void DuplicateSubObjects() override;
};

