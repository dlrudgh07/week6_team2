#pragma once

#include "Core/Types.h"
#include "Core/EngineStatics.h"
#include "Core/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectHash.h"
#include "Serialization/Archive.h"

class UClass;
// Property Reflection

#define REFLECT_START(ClassName) \
public: \
	inline static void RegisterProperties(UClass* InClass) \
	{

#define PROPERTY(PropertyName) \
    InClass->AddProperty<decltype(ThisClass::PropertyName)>(#PropertyName, offsetof(ThisClass, PropertyName));

#define PROPERTY_TYPE(PropertyName, PropertyType) \
    InClass->AddProperty<decltype(ThisClass::PropertyName)>(#PropertyName, offsetof(ThisClass, PropertyName), EPropertyType::##PropertyType);

#define REFLECT_END()\
	};\
private:

#define DECLARE_CLASS(ClassName, SuperClassName)                        \
public:                                                                 \
    using Super = SuperClassName;                                       \
    using ThisClass = ClassName;		                                \
    static UClass* StaticClass()                                        \
    {                                                                   \
        static UClass c;                                                \
        static bool bIsInit = false;                                    \
        if (!bIsInit)                                                   \
        {                                                               \
            c.Name  = #ClassName;                                       \
            c.Super = Super::StaticClass();								\
			if constexpr(std::is_abstract_v<ClassName>)					\
			{															\
				c.Constructor = nullptr;								\
			}															\
			else														\
			{															\
				c.Constructor = []() -> UObject* { return new ClassName(); };\
			}															\
			if (&ClassName::RegisterProperties != &Super::RegisterProperties) \
			{															\
				ClassName::RegisterProperties(&c);						\
			}															\
			RegisterClass(&c);											\
            bIsInit = true;                                             \
        }                                                               \
        return &c;                                                      \
    }                                                                   \
    struct FAutoRegister {FAutoRegister() { ThisClass::StaticClass();}};\
    inline static FAutoRegister AutoRegister;							\
private:																

class UObject
{
	friend class FObjectFactory;
public:
	UObject();
	UObject(bool bRegister);
	virtual ~UObject();

	static UClass* StaticClass();
	UClass* GetClass() const { return ClassPrivate; }

	template <typename T>
	bool IsA() { return IsA(T::StaticClass()); }
	bool IsA(const UClass* Class);

	uint32 GetUUID() const { return ObjectUUID; }
	void SetUUID(uint32 Uid) { ObjectUUID = Uid; }
	uint32 GetInternalIndex() const { return InternalIndex; }
	uint32 GetSerialNumber() const { return InternalSerialNumber; }

	const FName& GetFName() const { return Name; }         // FName 비교용
	FString GetName() const { return Name.ToString(); }    // Name 출력용

	UObject* GetOuter() const { return Outer; }
	void SetOuter(UObject* InOuter) { Outer = InOuter; }

	void SetName(const FName& InName) { Name = FName(InName); }

	inline static void RegisterProperties(UClass* InClass) {};

	inline void SetFlags(EObjectFlags NewFlags) { Flags |= NewFlags; }
	inline void ClearFlags(EObjectFlags FlagsToClear) { Flags &= ~FlagsToClear; }
	inline bool HasAnyFlags(EObjectFlags FlagsToCheck) const { return HasFlag(Flags, FlagsToCheck); }
	inline bool HasAllFlags(EObjectFlags FlagsToCheck) const { return (Flags & FlagsToCheck) == FlagsToCheck; }
	inline EObjectFlags GetFlags() const { return Flags; }

	virtual void Serialize(json& Handle, bool bIsLoading);

	void* operator new(uint64 Size)
	{
		void* Ptr = malloc(Size);
		if (!Ptr)
			throw std::bad_alloc();

		FEngineStatics::TotalAllocationBytes += static_cast<uint64>(Size);
		FEngineStatics::TotalAllocationCount += 1;
		return Ptr;
	}

	void operator delete(void* Ptr, uint64 Size)
	{
		FEngineStatics::TotalAllocationBytes -= static_cast<uint64>(Size);
		FEngineStatics::TotalAllocationCount -= 1;
		free(Ptr);
	}

private:
	uint32 ObjectUUID = 0;
	uint32 InternalIndex = 0;
	uint32 InternalSerialNumber = 0;

	FName Name = FName("None");
	UObject* Outer = nullptr;
	UClass* ClassPrivate = nullptr;

	EObjectFlags Flags = EObjectFlags::RF_NoFlags;

	bool bIsRegistered = true;
};

extern TArray<UObject*> GUObjectArray;
