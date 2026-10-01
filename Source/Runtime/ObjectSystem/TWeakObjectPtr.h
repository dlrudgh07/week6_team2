#pragma once

#include "ObjectSystem/Object.h"
#include <cstddef>
#include <concepts>

// 약한 참조 포인터
template<typename T>
class TWeakObjectPtr
{
private:
	uint32 ObjectIndex = 0;
	uint32 ObjectSerialNumber = 0;

public:
	TWeakObjectPtr() = default;

	template<typename U = T>
		requires std::derived_from<U, UObject>
	TWeakObjectPtr(T* InPtr)
		: ObjectIndex(InPtr ? InPtr->GetInternalIndex() : 0), ObjectSerialNumber(InPtr ? InPtr->GetSerialNumber() : 0)
	{
	}

	TWeakObjectPtr(std::nullptr_t)
		: ObjectIndex(0), ObjectSerialNumber(0)
	{
	}

	template<typename U = T>
		requires std::derived_from<U, UObject>
	TWeakObjectPtr& operator=(T* InPtr)
	{
		ObjectIndex = InPtr ? InPtr->GetInternalIndex() : 0;
		ObjectSerialNumber = InPtr ? InPtr->GetSerialNumber() : 0;
		return *this;
	}

	TWeakObjectPtr& operator=(std::nullptr_t)
	{
		ObjectIndex = 0;
		ObjectSerialNumber = 0;
		return *this;
	}

	T* Get() const
	{
		if (ObjectSerialNumber == 0)
			return nullptr;

		if (ObjectIndex >= static_cast<uint32>(GUObjectArray.Num()))
			return nullptr;

		UObject* Object = GUObjectArray[ObjectIndex];
		if (Object == nullptr || Object->GetSerialNumber() != ObjectSerialNumber)
			return nullptr;

		return static_cast<T*>(Object);
	}

	T* operator->() const { return Get(); }
	operator T*() const { return Get(); }
	explicit operator bool() const { return Get() != nullptr; }

	bool operator==(const TWeakObjectPtr& Other) const { return Get() == Other.Get(); }
	bool operator!=(const TWeakObjectPtr& Other) const { return Get() != Other.Get(); }
	bool operator==(const T* Other) const { return Get() == Other; }
	bool operator!=(const T* Other) const { return Get() != Other; }

	bool IsValid() const { return Get() != nullptr; }
	void Reset() { ObjectIndex = 0; ObjectSerialNumber = 0; }
};
