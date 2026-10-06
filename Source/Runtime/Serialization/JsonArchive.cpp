#include "EnginePCH.h"
#include "JsonArchive.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Core/EngineStatics.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/UObjectHash.h"

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

	ULevel* Level = World->GetCurrentLevel();

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

		// 컴포넌트 이름 뒤 번호(_168)는 전역 카운터라 새로 Spawn한 액터와 맞지 않으므로 번호를 뗀 이름으로 비교한다.
		// 같은 이름이 여러 개일 수 있으니 한 번 매칭된 컴포넌트는 다시 쓰지 않고, 클래스도 반드시 같아야 한다.
		// 우선순위: 같은 순서 + 같은 이름 → 같은 이름 → 같은 클래스
		const TArray<UActorComponent*>& Components = Actor->GetComponents();
		TArray<uint8> bMatched;
		bMatched.Init(0, Components.Num());

		int32 SavedIndex = 0;
		for (json& ComponentJson : ActorJson["Components"])       // json → json&
		{
			const int32 Index = SavedIndex++;

			if (!ComponentJson.is_object() ||
				!ComponentJson.contains("Class") || !ComponentJson["Class"].is_string())
				continue;

			const FString ClassName = ComponentJson["Class"].get<FString>();
			const FString SavedName = ComponentJson.value("Name", FString());
			const FString PlainName = FName(SavedName).GetPlainNameString();

			auto IsCandidate = [&](int32 i, bool bRequireName)
			{
				UActorComponent* C = Components[i];
				return C && !bMatched[i] && C->GetClass()->Name == ClassName &&
					(!bRequireName || C->GetFName().GetPlainNameString() == PlainName);
			};

			int32 FoundIndex = -1;
			if (Components.IsValidIndex(Index) && IsCandidate(Index, true))
				FoundIndex = Index;
			for (int32 i = 0; FoundIndex < 0 && i < Components.Num(); ++i)
			{
				if (IsCandidate(i, true))
					FoundIndex = i;
			}
			for (int32 i = 0; FoundIndex < 0 && i < Components.Num(); ++i)
			{
				if (IsCandidate(i, false))
					FoundIndex = i;
			}
			// 생성자가 만들지 않은 컴포넌트(Details 패널에서 추가한 것 등)는 저장된 클래스로 새로 만든다.
			// AddComponentByClass가 Owner 설정, 루트 부착, 월드 등록까지 처리한다.
			if (FoundIndex < 0)
			{
				UClass* ComponentClass = FindClass(ClassName);
				if (ComponentClass && ComponentClass->IsChildOf(UActorComponent::StaticClass()))
				{
					if (Actor->AddComponentByClass(ComponentClass, FName(PlainName)))
					{
						FoundIndex = Components.Num() - 1;
						bMatched.Add(0);
					}
				}
			}

			if (FoundIndex < 0)
			{
				LOG(Warning, "Load: {} could not create component {} ({})", Class->Name, SavedName, ClassName);
				continue;
			}

			bMatched[FoundIndex] = 1;
			Components[FoundIndex]->Serialize(ComponentJson["Properties"], true);
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