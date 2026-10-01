#include "EnginePCH.h"
#include "StaticMeshActor.h"

#include "Component/PrimitiveComponent.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Asset/AssetManager.h"

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Cube:
			return "Cube";

		case EPrimitiveType::Sphere:
			return "Sphere";

		case EPrimitiveType::Plane:
			return "Plane";

		case EPrimitiveType::Cone:
			return "Cone";

		default:
			return "";
		}
	}
}

AStaticMeshActor::AStaticMeshActor()
{
	// 정적 메시는 빈 Tick을 시작하지 않지만, 자체 이동 로직이 생기면 런타임에 다시 켤 수 있다.
	SetActorTickEnabled(false);
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("UPrimitiveComponent");
	SetRootComponent(StaticMeshComponent);

}

void AStaticMeshActor::SetPrimitiveType(EPrimitiveType Type)
{
	//StaticMeshComponent->SetType(Type);
	//StaticMeshComponent->SetMesh(UAssetManager::GetAssetByKey<UStaticMesh>(PrimitiveTypeToString(Type)));
}

void AStaticMeshActor::BeginPlay()
{
	Super::BeginPlay();
}
