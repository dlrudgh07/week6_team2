#include "EnginePCH.h"
#include "JsonArchive.h"

#include "World/World.h"
#include "World/Level.h"
#include "Core/EngineStatics.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "ObjectSystem/UObjectHash.h"

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Sphere:
			return "Sphere";

		case EPrimitiveType::Cube:
			return "Cube";

		case EPrimitiveType::Cone:
			return "Cone";

		case EPrimitiveType::Plane:
			return "Plane";

		default:
			return "";
		}
	}

	EPrimitiveType FStringToPrimitiveType(const FString& String)
	{
		if (String == "Sphere")
			return EPrimitiveType::Sphere;

		if (String == "Cube")
			return EPrimitiveType::Cube;

		if (String == "Cone")
			return EPrimitiveType::Cone;

		if (String == "Plane")
			return EPrimitiveType::Plane;

		return EPrimitiveType::Cube;
	}

}

bool FJsonArchive::SaveWorld(UWorld* World, const FString& Path)
{
	if (!World)
		return false;

	ULevel* Level = World->GetPersistentLevel();

	if (!Level)
		return false;

	json Json;

	Json["Version"] = 2;
	Json["Actors"] = json::array();

	for (AActor* Actor : Level->GetActors())
	{
		if (!Actor || Actor->HasAnyFlags(EObjectFlags::RF_Transient))
			continue;

		json ActorJson;
		ActorJson["Class"] = Actor->GetClass()->Name;
		Actor->Serialize(ActorJson["Properties"], false);

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component) continue;
			json ComponentJson;
			ComponentJson["Name"] = Component->GetName();
			ComponentJson["Class"] = Component->GetClass()->Name;
			Component->Serialize(ComponentJson["Properties"], false);

			ActorJson["Components"].push_back(ComponentJson);
		}

		Json["Actors"].push_back(ActorJson);
	}

	std::ofstream File(Path);

	if (!File.is_open())
		return false;

	File << Json.dump(4);

	File.close();

	return !File.fail();
}

bool FJsonArchive::LoadWorld(UWorld* World, const FString& Path)
{
	if (!World)
		return false;

	if (!std::filesystem::exists(Path))
	{
		LOG(Warning, "{} is Not Exist!", Path);
		return false;
	}

	std::ifstream File(Path);

	if (!File.is_open())
		return false;

	json Json;

	try
	{
		File >> Json;
	}
	catch (const json::parse_error&)
	{
		return false;
	}

	if (!Json.contains("Version") || Json["Version"] != 2)
		return false;

	// 1. 모든 액터의 기본 정보를 먼저 검사
	for (const json& ActorJson : Json["Actors"])
	{
		if (!ActorJson.is_object())
			return false;

		if (!ActorJson.contains("Class") ||
			!ActorJson["Class"].is_string())
			return false;

		UClass* Class =
			FindClass(ActorJson["Class"].get<FString>());

		if (!Class || !Class->IsChildOf(AActor::StaticClass()))
			return false;
	}

	if (!Json.contains("Actors") || !Json["Actors"].is_array())
		return false;

	World->ClearWorld();

	// 3. 실제 생성
	for (json& ActorJson : Json["Actors"])
	{
		if (!ActorJson.is_object())
			return false;
		if (!ActorJson.contains("Class") || !ActorJson["Class"].is_string())
			return false;

		UClass* Class = FindClass(ActorJson["Class"].get<FString>());
		if (!Class || !Class->IsChildOf(AActor::StaticClass()))
			return false;

		AActor* Actor = World->SpawnActor(Class);
		if (!Actor)
		{
			LOG(Warning, "Load: failed to spawn {}", ActorJson["Class"].get<FString>());
			continue;
		}
		Actor->Serialize(ActorJson["Properties"], true);

		for (json& ComponentJson : ActorJson["Components"])       // json → json&
		{
			const FName Name(ComponentJson["Name"].get<FString>());

			// 이름으로 컴포넌트 찾기
			UActorComponent* Component = nullptr;
			for (UActorComponent* C : Actor->GetComponents())
			{
				if (C && C->GetFName() == Name)
				{
					Component = C;
					break;
				}
			}

			if (!Component)
			{
				LOG(Warning, "Load: {} has no component {}", Class->Name, Name.ToString());
				continue;
			}

			if (Component->GetClass()->Name != ComponentJson["Class"].get<FString>())
			{
				LOG(Warning, "Load: component {} class mismatch", Name.ToString());
				continue;
			}

			Component->Serialize(ComponentJson["Properties"], true);
		}
	}

	//if (!Json.contains("NextUUID"))
	//	return false;

	//// 파일 검증이 끝난 뒤 Clear
	//uint64 SavedNextUUID = Json["NextUUID"].get<uint64>();

	//World->ClearScene();

	//FEngineStatics::NextUUID = SavedNextUUID;

	//if (!Json.contains("Primitives"))
	//{
	//	return true;
	//}

	//for (auto& [UUIDString, PrimitiveJson] : Json["Primitives"].items())
	//{
	//	uint64 UUID = std::stoull(UUIDString);

	//	FTransform Transform;

	//	if (PrimitiveJson.contains("Location"))
	//	{
	//		Transform.Location.X = PrimitiveJson["Location"][0].get<float>();
	//		Transform.Location.Y = PrimitiveJson["Location"][1].get<float>();
	//		Transform.Location.Z = PrimitiveJson["Location"][2].get<float>();
	//	}

	//	if (PrimitiveJson.contains("Rotation"))
	//	{
	//		Transform.Rotation.Roll = PrimitiveJson["Rotation"][0].get<float>();
	//		Transform.Rotation.Pitch = PrimitiveJson["Rotation"][1].get<float>();
	//		Transform.Rotation.Yaw = PrimitiveJson["Rotation"][2].get<float>();
	//	}

	//	if (PrimitiveJson.contains("Scale"))
	//	{
	//		Transform.Scale.X = PrimitiveJson["Scale"][0].get<float>();
	//		Transform.Scale.Y = PrimitiveJson["Scale"][1].get<float>();
	//		Transform.Scale.Z = PrimitiveJson["Scale"][2].get<float>();
	//	}

	//	if (!PrimitiveJson.contains("Type"))
	//		continue;

	//	FString TypeString = PrimitiveJson["Type"].get<FString>();

	//	if (TypeString == "Other")
	//		continue;

	//	EPrimitiveType Type = FStringToPrimitiveType(TypeString);

	//	AStaticMeshActor* Actor =
	//		World->SpawnActor<AStaticMeshActor>("Test", &Transform);

	//	if (!Actor)
	//		continue;

	//	Actor->SetPrimitiveType(Type);
	//	Actor->SetUUID(UUID);
	//}

	//// SpawnActor하면서 증가했을 UUID를 저장 당시 값으로 복원
	//FEngineStatics::NextUUID = SavedNextUUID;

	return true;
}