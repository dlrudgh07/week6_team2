#pragma once

#include "Object.h"

class FObjectFactory
{
public:
	static UObject* ConstructObject(UClass* Class, UObject* Outer = nullptr, FName Name = NAME_None);

	template <typename T>
	static T* ConstructObject()
	{
		return CastChecked<T>(ConstructObject(T::StaticClass()));
	}

	static FName MakeUniqueObjectName(const UClass* Class, UObject* Outer = nullptr, FName BaseName = NAME_None);
private:

};