#pragma once

#include "ObjectSystem/Object.h"
#include "Component/PrimitiveComponent.h"
#include "ObjectSystem/Class.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Container/Set.h"

class ULevel;
class UWorld;

class AActor : public UObject
{
	DECLARE_CLASS(AActor, UObject)
	REFLECT_START(ClassName)
	REFLECT_END()
  public:
	AActor();
	virtual ~AActor();

	virtual void BeginPlay();           // xx World->AddPrimitive 책임이동 필요
	virtual void Tick(float DeltaTime); // xx component 호출
	bool CanEverTick() const;
	bool IsActorTickEnabled() const;
	void SetActorTickEnabled(bool bEnabled);

	UWorld* GetWorld() const;
	ULevel* GetLevel() const;

	const TArray<UActorComponent*>& GetComponents() const;
	USceneComponent* GetRootComponent() const;
	void SetRootComponent(USceneComponent* SceneComponent);
	void RemoveOwnedComponent(UActorComponent* Component);

	FVector GetActorLocation() const;
	FRotator GetActorRotation() const;
	FVector GetActorScale3D() const;
	FQuat GetActorQuat() const; //xx 타입명변경
	FTransform GetActorTransform() const;

	bool Destroy();

	// xx 삭제 예정
	friend class UWorld;

	template <typename T> T* CreateDefaultSubobject(FName Name)
	{
		T* Component = CastChecked<T>(FObjectFactory::ConstructObject(T::StaticClass(), this, Name));
		Component->SetOwner(this);
		Components.Add(Component);
		return Component;
	}

  protected:
	void SetCanEverTick(bool bEnabled);

	TArray<UActorComponent*> Components;
	USceneComponent* RootComponent = nullptr;

	UWorld* World = nullptr;
	ULevel* Level = nullptr;
	bool bCanEverTick = true;
	bool bTickEnabled = true;

  private:
};
