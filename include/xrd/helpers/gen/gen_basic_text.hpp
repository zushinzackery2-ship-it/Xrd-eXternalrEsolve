#pragma once
// Xrd-eXternalrEsolve - SDK 生成：Basic.hpp TSubclassOf/FText/FWeakObjectPtr
// 从 gen_basic_core.hpp 拆分，保持单文件 300 行以内

#include <fstream>

namespace xrd
{
namespace gen
{

// 写入 TSubclassOf / FTextImpl / FText / FWeakObjectPtr / TWeakObjectPtr
inline void WriteSubclassAndText(std::ofstream& f)
{
    f << R"(template<typename ClassType>
class TSubclassOf
{
	class UClass* ClassPtr;

public:
	TSubclassOf() = default;

	inline TSubclassOf(UClass* Class)
		: ClassPtr(Class)
	{
	}

	inline UClass* Get()
	{
		return ClassPtr;
	}

	inline operator UClass*() const
	{
		return ClassPtr;
	}

	template<typename Target, typename = std::enable_if<std::is_base_of_v<Target, ClassType>, bool>::type>
	inline operator TSubclassOf<Target>() const
	{
		return ClassPtr;
	}

	inline UClass* operator->()
	{
		return ClassPtr;
	}

	inline TSubclassOf& operator=(UClass* Class)
	{
		ClassPtr = Class;

		return *this;
	}

	inline bool operator==(const TSubclassOf& Other) const
	{
		return ClassPtr == Other.ClassPtr;
	}

	inline bool operator!=(const TSubclassOf& Other) const
	{
		return ClassPtr != Other.ClassPtr;
	}

	inline bool operator==(UClass* Other) const
	{
		return ClassPtr == Other;
	}

	inline bool operator!=(UClass* Other) const
	{
		return ClassPtr != Other;
	}
};
namespace FTextImpl
{
// Predefined struct FTextData
// 0x0038 (0x0038 - 0x0000)
class FTextData final
{
public:
	uint8                                         Pad_0[0x28];                                       // 0x0000(0x0028)(Fixing Size After Last Property [ Rei-SdkDumper ])
	class FString                                 TextSource;                                        // 0x0028(0x0010)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FTextData
DUMPER7_ASSERTS_FTextData;
#endif
}

// Predefined struct FText
// 0x0018 (0x0018 - 0x0000)
class FText final
{
public:
	class FTextImpl::FTextData*                   TextData;                                          // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         Pad_8[0x10];                                       // 0x0008(0x0010)(Fixing Struct Size After Last Property [ Rei-SdkDumper ])

public:
	const class FString& GetStringRef() const
	{
		return TextData->TextSource;
	}
	std::string ToString() const
	{
		return TextData->TextSource.ToString();
	}
};
#ifdef DUMPER7_ASSERTS_FText
DUMPER7_ASSERTS_FText;
#endif

// Predefined struct FWeakObjectPtr
// 0x0008 (0x0008 - 0x0000)
class FWeakObjectPtr
{
public:
	int32                                         ObjectIndex;                                       // 0x0000(0x0004)(NOT AUTO-GENERATED PROPERTY)
	int32                                         ObjectSerialNumber;                                // 0x0004(0x0004)(NOT AUTO-GENERATED PROPERTY)

public:
	class UObject* Get() const;
	class UObject* operator->() const;
	bool operator==(const FWeakObjectPtr& Other) const;
	bool operator!=(const FWeakObjectPtr& Other) const;
	bool operator==(const class UObject* Other) const;
	bool operator!=(const class UObject* Other) const;
};
#ifdef DUMPER7_ASSERTS_FWeakObjectPtr
DUMPER7_ASSERTS_FWeakObjectPtr;
#endif

template<typename UEType>
class TWeakObjectPtr : public FWeakObjectPtr
{
public:
	UEType* Get() const
	{
		return static_cast<UEType*>(FWeakObjectPtr::Get());
	}

	UEType* operator->() const
	{
		return static_cast<UEType*>(FWeakObjectPtr::Get());
	}
};

)";
}

} // namespace gen
} // namespace xrd
