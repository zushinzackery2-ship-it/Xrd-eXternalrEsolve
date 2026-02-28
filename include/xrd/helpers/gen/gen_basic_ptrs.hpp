#pragma once
// Xrd-eXternalrEsolve - SDK 生成：Basic.hpp 第三部分
// FUniqueObjectGuid / TPersistentObjectPtr / TLazyObjectPtr
// FSoftObjectPtr / TSoftObjectPtr / TSoftClassPtr
// FScriptInterface / TScriptInterface / FFieldPath / TFieldPath
// TOptional / FScriptDelegate / TDelegate / TMulticastInlineDelegate

#include <fstream>

namespace xrd
{
namespace gen
{

// 写入智能指针和委托类型
inline void WritePtrAndDelegateTypes(std::ofstream& f)
{
    f << R"(// Predefined struct FUniqueObjectGuid
// 0x0010 (0x0010 - 0x0000)
class FUniqueObjectGuid final
{
public:
	uint32                                        A;                                                 // 0x0000(0x0004)(NOT AUTO-GENERATED PROPERTY)
	uint32                                        B;                                                 // 0x0004(0x0004)(NOT AUTO-GENERATED PROPERTY)
	uint32                                        C;                                                 // 0x0008(0x0004)(NOT AUTO-GENERATED PROPERTY)
	uint32                                        D;                                                 // 0x000C(0x0004)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FUniqueObjectGuid
DUMPER7_ASSERTS_FUniqueObjectGuid;
#endif

// Predefined struct TPersistentObjectPtr
// 0x0000 (0x0000 - 0x0000)
template<typename TObjectID>
class TPersistentObjectPtr
{
public:
	FWeakObjectPtr                                WeakPtr;                                           // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	int32                                         TagAtLastTest;                                     // 0x0008(0x0004)(NOT AUTO-GENERATED PROPERTY)
	TObjectID                                     ObjectID;                                          // 0x000C(0x0000)(NOT AUTO-GENERATED PROPERTY)

public:
	class UObject* Get() const
	{
		return WeakPtr.Get();
	}
	class UObject* operator->() const
	{
		return WeakPtr.Get();
	}
};

template<typename UEType>
class TLazyObjectPtr : public TPersistentObjectPtr<FUniqueObjectGuid>
{
public:
	UEType* Get() const
	{
		return static_cast<UEType*>(TPersistentObjectPtr::Get());
	}
	UEType* operator->() const
	{
		return static_cast<UEType*>(TPersistentObjectPtr::Get());
	}
};

namespace FakeSoftObjectPtr
{

// ScriptStruct CoreUObject.SoftObjectPath
// 0x0018 (0x0018 - 0x0000)
struct FSoftObjectPath
{
public:
	class FName                                   AssetPathName;                                     // 0x0000(0x0008)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
	class FString                                 SubPathString;                                     // 0x0008(0x0010)(ZeroConstructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
};
#ifdef DUMPER7_ASSERTS_FSoftObjectPath
DUMPER7_ASSERTS_FSoftObjectPath;
#endif

}

class FSoftObjectPtr : public TPersistentObjectPtr<FakeSoftObjectPtr::FSoftObjectPath>
{
};

template<typename UEType>
class TSoftObjectPtr : public FSoftObjectPtr
{
public:
	UEType* Get() const
	{
		return static_cast<UEType*>(TPersistentObjectPtr::Get());
	}
	UEType* operator->() const
	{
		return static_cast<UEType*>(TPersistentObjectPtr::Get());
	}
};

template<typename UEType>
class TSoftClassPtr : public FSoftObjectPtr
{
public:
	UEType* Get() const
	{
		return static_cast<UEType*>(TPersistentObjectPtr::Get());
	}
	UEType* operator->() const
	{
		return static_cast<UEType*>(TPersistentObjectPtr::Get());
	}
};

// Predefined struct FScriptInterface
// 0x0010 (0x0010 - 0x0000)
class FScriptInterface
{
public:
	UObject*                                      ObjectPointer;                                     // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	void*                                         InterfacePointer;                                  // 0x0008(0x0008)(NOT AUTO-GENERATED PROPERTY)

public:
	class UObject* GetObjectRef() const
	{
		return ObjectPointer;
	}
	
	void* GetInterfaceRef() const
	{
		return InterfacePointer;
	}
	
};
#ifdef DUMPER7_ASSERTS_FScriptInterface
DUMPER7_ASSERTS_FScriptInterface;
#endif

// Predefined struct TScriptInterface
// 0x0000 (0x0010 - 0x0010)
template<class InterfaceType>
class TScriptInterface final : public FScriptInterface
{
};

// Predefined struct FFieldPath
// 0x0020 (0x0020 - 0x0000)
class FFieldPath
{
public:
	class FField*                                 ResolvedField;                                     // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	TWeakObjectPtr<class UStruct>                 ResolvedOwner;                                     // 0x0008(0x0008)(NOT AUTO-GENERATED PROPERTY)
	TArray<FName>                                 Path;                                              // 0x0010(0x0010)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FFieldPath
DUMPER7_ASSERTS_FFieldPath;
#endif

// Predefined struct TFieldPath
// 0x0000 (0x0020 - 0x0020)
template<class PropertyType>
class TFieldPath final : public FFieldPath
{
};


template<typename OptionalType, bool bIsIntrusiveUnsetCheck = false>
class TOptional
{
private:
	template<int32 TypeSize>
	struct OptionalWithBool
	{
		static_assert(TypeSize > 0x0, "TOptional can not store an empty type!");

		uint8 Value[TypeSize];
		bool bIsSet;
	};

private:
	using ValueType = std::conditional_t<bIsIntrusiveUnsetCheck, uint8[sizeof(OptionalType)], OptionalWithBool<sizeof(OptionalType)>>;

private:
	alignas(OptionalType) ValueType StoredValue;

private:
	inline uint8* GetValueBytes()
	{
		if constexpr (!bIsIntrusiveUnsetCheck)
			return StoredValue.Value;

		return StoredValue;
	}

	inline const uint8* GetValueBytes() const
	{
		if constexpr (!bIsIntrusiveUnsetCheck)
			return StoredValue.Value;

		return StoredValue;
	}
public:

	inline OptionalType& GetValueRef()
	{
		return *reinterpret_cast<OptionalType*>(GetValueBytes());
	}

	inline const OptionalType& GetValueRef() const
	{
		return *reinterpret_cast<const OptionalType*>(GetValueBytes());
	}

	inline bool IsSet() const
	{
		if constexpr (!bIsIntrusiveUnsetCheck)
			return StoredValue.bIsSet;

		constexpr char ZeroBytes[sizeof(OptionalType)];

		return memcmp(GetValueBytes(), &ZeroBytes, sizeof(OptionalType)) == 0;
	}

	inline explicit operator bool() const
	{
		return IsSet();
	}
};


// Predefined struct FScriptDelegate
// 0x0010 (0x0010 - 0x0000)
struct FScriptDelegate
{
public:
	FWeakObjectPtr                                Object;                                            // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	FName                                         FunctionName;                                      // 0x0008(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FScriptDelegate
DUMPER7_ASSERTS_FScriptDelegate;
#endif

// Predefined struct TDelegate
// 0x0010 (0x0010 - 0x0000)
template<typename FunctionSignature>
class TDelegate
{
public:
	struct InvalidUseOfTDelegate                  TemplateParamIsNotAFunctionSignature;              // 0x0000(0x0000)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         Pad_0[0x10];                                       // 0x0000(0x0010)(Fixing Struct Size After Last Property [ Rei-SdkDumper ])
};

// Predefined struct TDelegate<Ret(Args...)>
// 0x0010 (0x0010 - 0x0000)
template<typename Ret, typename... Args>
class TDelegate<Ret(Args...)>
{
public:
	FScriptDelegate                               BoundFunction;                                     // 0x0000(0x0010)(NOT AUTO-GENERATED PROPERTY)
};

// Predefined struct TMulticastInlineDelegate
// 0x0010 (0x0010 - 0x0000)
template<typename FunctionSignature>
class TMulticastInlineDelegate
{
public:
	struct InvalidUseOfTMulticastInlineDelegate   TemplateParamIsNotAFunctionSignature;              // 0x0000(0x0010)(NOT AUTO-GENERATED PROPERTY)
};

// Predefined struct TMulticastInlineDelegate<Ret(Args...)>
// 0x0000 (0x0000 - 0x0000)
template<typename Ret, typename... Args>
class TMulticastInlineDelegate<Ret(Args...)>
{
public:
	TArray<FScriptDelegate>                       InvocationList;                                    // 0x0000(0x0010)(NOT AUTO-GENERATED PROPERTY)
};

)";
}

} // namespace gen
} // namespace xrd
