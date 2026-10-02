#include "EnginePCH.h"
#include "ObjectFactory.h"
#include "Class.h"

UObject* FObjectFactory::ConstructObject(UClass* Class, UObject* Outer, FName Name)
{
    if (!Class || !Class->Constructor)
        return nullptr;
    
    UObject* Object = Class->Constructor();
    Object->ClassPrivate = Class;
    HashObject(Object, Class);

    Object->SetOuter(Outer);

    Name = MakeUniqueObjectName( Class, Outer, Name);

    Object->SetName(Name);

    // LOG(Info, "Create {}", Class->Name);
    //LOG(Info, "Total Allocation Bytes - {}", FEngineStatics::TotalAllocationBytes);
    //LOG(Info, "Total Allocation Count - {}", FEngineStatics::TotalAllocationCount);

    return Object;
}

FName FObjectFactory::MakeUniqueObjectName(const UClass* Class, UObject* Outer, FName BaseName)
{
    if (!Class)
        return FName();

    // 기본 이름 지정
    if (BaseName == NAME_None)
    {
        BaseName = FName(Class->Name);
    }

    // 전역 카운터로 즉시 고유 이름 생성
    static uint64 ObjectUniqueCounter = 0;
    return FName(BaseName.ToString() + "_" + std::to_string(++ObjectUniqueCounter));
}
