#include "EnginePCH.h"
#include "RotatingActor.h"

ARotatingActor::ARotatingActor()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("MeshComponent");
	SetRootComponent(MeshComponent);
}

void ARotatingActor::BeginPlay()
{
	Super::BeginPlay();
	// PIE 시작 시 액터마다 정확히 한 번 찍혀야 한다.
	LOG(Info, "ARotatingActor::BeginPlay {}", GetName());
}

void ARotatingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!MeshComponent)
		return;

	FRotator Rotation = MeshComponent->GetRelativeRotation();
	Rotation.Yaw += DegreesPerSecond * DeltaTime;
	MeshComponent->SetRelativeRotation(Rotation);
}

void ARotatingActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	MeshComponent = FindComponentByClass<UStaticMeshComponent>();
}
