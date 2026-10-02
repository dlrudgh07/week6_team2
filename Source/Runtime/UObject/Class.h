#pragma once

#include "Property.h"
#include "Object.h"

using ClassConstructor = UObject * (*)();

class UClass : public UObject
{
public:
	UClass();

	FString Name;
	UClass* Super = nullptr;
	ClassConstructor Constructor = nullptr;

	TArray<FProperty> Properties;
	UObject* DefaultObject = nullptr;

	bool IsChildOf(const UClass* BaseClass) const;
	UObject* GetDefaultObject();

	inline const TArray<FProperty>& GetProperties() const { return Properties; }

	template<class T>
	T* GetDefaultObject()
	{
		return CastChecked<T>(GetDefaultObject());
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset)
	{
		UClass* ObjClass = nullptr;
		if constexpr (std::is_pointer_v<T> &&
			std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
		{
			ObjClass = std::remove_pointer_t<T>::StaticClass();
		}

		Properties.Add({ InName, GetPropertyType<T>(), InOffset, sizeof(T), ObjClass });
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset, EPropertyType InType)
	{
		Properties.Add({ InName, InType, InOffset, sizeof(T) });
	}
};

//보류
inline void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass)
{
	for (UClass* c = FromClass; c; c = c->Super)
		for (const FProperty& p : c->Properties)
			memcpy((char*)Dst + p.Offset, (char*)Src + p.Offset, p.Size);
}