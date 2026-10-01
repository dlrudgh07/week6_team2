#include "EnginePCH.h"
#include "Class.h"

#include "Object.h"

UClass::UClass()
	:UObject(false)
{
	//LOG(Info, "Create RTTI UClass");
}

bool UClass::IsChildOf(const UClass* BaseClass) const
{
	if (!BaseClass) return false;
	for (const UClass* c = this; c; c = c->Super)
	{
		if (c == BaseClass) return true;
	}
	return false;
}

UObject* UClass::GetDefaultObject() 
{
	if (!DefaultObject)
	{
		DefaultObject = Constructor();
	}
	return DefaultObject;
}


