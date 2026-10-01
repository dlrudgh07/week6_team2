#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"

class AActor;

class UActorComponent : public UObject
{
	DECLARE_CLASS(UActorComponent, UObject)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	virtual ~UActorComponent() override;

	virtual void BeginPlay() {};
	virtual void TickComponent(float DeltaTime) {};

	void SetOwner(AActor* InOwner) { Owner = InOwner; }
    AActor* GetOwner() const { return Owner; }

private:
	AActor* Owner = nullptr;
};