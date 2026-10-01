#pragma once

#include "Container/Array.h"
#include "Core/EngineString.h"

class UObject;
class UClass;

void RegisterClass(UClass* Class);

UClass* FindClass(const FString& Name);

void GetObjectsOfClass(const UClass* ClassToLookFor, TArray<UObject*>& Results, bool bIsIncludeDerivedClass = true);

void HashObject(UObject* Obj, const UClass* Class); 
void UnhashObject(UObject* Obj, const UClass* Class);
