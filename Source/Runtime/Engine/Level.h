// 액터 목록을 소유하고 관리하는 클래스
#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Containers/Array.h"

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
	uint32 GetActorCount() const;
	void AddActor(AActor* Actor);
	void ClearActors();
	//virtual void Serialize(FArchive& Ar) override; // Save Level 구현예정

	virtual void DuplicateSubObjects() override;

  private:
	friend class UWorld;

	UWorld* OwningWorld = nullptr;
	TArray<AActor*> Actors;
	// AWorldSettings* WorldSettings = nullptr;
	bool bIsVisible = true;
};
