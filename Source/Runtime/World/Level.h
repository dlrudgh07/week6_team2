// 액터 목록을 소유하고 관리하는 클래스
#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"
#include "Container/Array.h"

class UWorld;
class AActor;

class ULevel : public UObject
{
	DECLARE_CLASS(ULevel, UObject)

  public:
	ULevel() = default;
	virtual ~ULevel() = default;

	UWorld* GetWorld() const;
	const TArray<AActor*>& GetActors() const;
	uint32 GetActorNum() const;
	void AddActor(AActor* Actor);
	void ClearActors();
	//virtual void Serialize(FArchive& Ar) override; // Save Level 구현예정

  private:
	friend class UWorld;

	UWorld* OwningWorld = nullptr;
	TArray<AActor*> Actors;
	// AWorldSettings* WorldSettings = nullptr;
	bool bIsVisible = true;
};
