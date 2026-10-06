#include "EnginePCH.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Object.h"

#include "Core/EngineStatics.h"
#include "UObject/Class.h"

#include "Serialization/TypeSerializer.h"
#include "Engine/AssetManager.h"
#include "UObject/UObjectGlobals.h"

TArray<UObject*> GUObjectArray;

// 삭제된 오브젝트가 비운 GUObjectArray 슬롯. 다음 생성 때 재사용한다.
// swap-remove로 다른 오브젝트를 옮기면 그 오브젝트를 가리키던 TWeakObjectPtr(Index + SerialNumber)가 끊어지므로,
// 살아 있는 오브젝트의 인덱스는 절대 바꾸지 않는다. 재사용된 슬롯은 SerialNumber 검사로 걸러진다.
static TArray<uint32> ObjAvailableList;

UObject::UObject()
{
	static uint32 NextSerialNumber = 1;
	InternalSerialNumber = NextSerialNumber++;
	ObjectUUID = FEngineStatics::GetUUID();

	if (ObjAvailableList.IsEmpty())
	{
		InternalIndex = GUObjectArray.Num();
		GUObjectArray.Add(this);
	}
	else
	{
		InternalIndex = ObjAvailableList.Last();
		ObjAvailableList.RemoveLast();
		GUObjectArray[InternalIndex] = this;
	}

	LOG(Info, "UUID : {}", ObjectUUID);
}

UObject::UObject(bool bRegister)
{
	static uint32 NextSerialNumber = 1;
	InternalSerialNumber = NextSerialNumber++;
	bIsRegistered = bRegister;
}

UObject::UObject(const UObject& Other) : UObject()
{
	Outer = Other.Outer;
	ClassPrivate = Other.ClassPrivate;
	Flags = Other.Flags;
}

UObject::~UObject()
{
	if (bIsRegistered)
	{
		if (ClassPrivate)
		{
			UnhashObject(this, ClassPrivate);
		}
		GUObjectArray[InternalIndex] = nullptr;
		ObjAvailableList.Add(InternalIndex);
	}
}

UClass* UObject::StaticClass()
{
	static UClass c;
	static bool bIsInit = false;
	if (!bIsInit)
	{
		c.Name = "Object";
		c.Super = nullptr;
		c.Constructor = []() -> UObject*
		{
			return new UObject();
		};
		c.CopyConstructor = [](const UObject* Src) -> UObject*
		{
			return new UObject(*Src);
		};
		bIsInit = true;
	}
	return &c;
}

bool UObject::IsA(const UClass* Class)
{
	const UClass* ThisClass = GetClass();
	return ThisClass && ThisClass->IsChildOf(Class);
}

void UObject::Serialize(json& Handle, bool bIsLoading)
{
	for (UClass* c = GetClass(); c; c = c->Super)
	{
		for (FProperty Property : c->GetProperties())
		{
			if (bIsLoading && !Handle.contains(Property.Name))
				continue;

			void* Ptr = reinterpret_cast<uint8*>(this) + Property.Offset;

			switch (Property.Type)
			{
			case EPropertyType::Float:
			{
				float& Value = *static_cast<float*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<float>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Int:
			{
				int32& Value = *static_cast<int32*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<int32>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::String:
			{
				FString& Value = *static_cast<FString*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<FString>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Bool:
			{
				bool& Value = *static_cast<bool*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<bool>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Vector:
			{
				FVector& Value = *static_cast<FVector*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<FVector>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Rotator:
			{
				FRotator& Value = *static_cast<FRotator*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<FRotator>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Vector4:
			{
				FVector4& Value = *static_cast<FVector4*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<FVector4>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Color:
			{
				FVector4& Value = *static_cast<FVector4*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<FVector4>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Transform:
			{
				FTransform& Value = *static_cast<FTransform*>(Ptr);
				if (bIsLoading)
					Value = Handle[Property.Name].get<FTransform>();
				else
					Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Object:
			{
				UObject*& Value = *static_cast<UObject**>(Ptr);

				if (bIsLoading)
				{
					// null로 저장된 건 "되찾을 수 없는 값"이었으므로 기본값을 유지한다.
					// 문자열이 아닌 값(예: Billboard의 Material 객체)은 하위 클래스 Serialize가 직접 처리한다.
					if (!Handle[Property.Name].is_string())
					{
						break;
					}

					const FString AssetPath = Handle[Property.Name].get<FString>();
					URenderAsset* Asset = nullptr;

					if (Property.Class && Property.Class->IsChildOf(UStaticMesh::StaticClass()))
					{
						Asset = UAssetManager::GetAssetByKey<UStaticMesh>(AssetPath);
					}
					else
					{
						Asset = UAssetManager::GetAssetByKey<URenderAsset>(AssetPath);
					}

					// 못 찾으면 생성자가 넣어둔 기본값을 그대로 둔다.
					// (파일 하나가 없어져도 씬은 열리고, 액터가 사라지지 않고 기본 모양으로 보인다)
					if (!Asset || (Property.Class && !Asset->IsA(Property.Class)))
					{
						LOG(Warning, "Load: asset '{}' not found for {}, keeping default", AssetPath, Property.Name);
						break;
					}

					Value = Asset;
				}
				else
				{
					// 경로가 없는 에셋(런타임에 만든 인스턴스 등)은 되찾을 방법이 없으니 null로 쓴다
					URenderAsset* Asset = Cast<URenderAsset>(Value);
					if (Asset && !Asset->GetPath().empty())
						Handle[Property.Name] = Asset->GetPath();
					else
						Handle[Property.Name] = nullptr;
				}
				break;
			}
			default:
				break;
			}
		}
	}
}

void UObject::DuplicateSubObjects()
{

}

UObject* UObject::Duplicate()
{
	// 복사 생성자 호출
	UObject* NewObject = FObjectFactory::DuplicateObject(this); // 얕은 복사
	if (NewObject)
	{
		NewObject->DuplicateSubObjects(); // 깊은 복사
	}

	return NewObject;
}
