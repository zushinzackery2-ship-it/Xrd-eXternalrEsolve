#pragma once
// Xrd-eXternalrEsolve - SDK 生成：Basic.hpp 第二部分
// FUObjectItem / TUObjectArray / TUObjectArrayWrapper / FName / TSubclassOf
// FText / FWeakObjectPtr / TWeakObjectPtr 等核心运行时类型

#include "../../core/context.hpp"
#include <fstream>
#include <string>
#include <format>

namespace xrd
{
namespace gen
{

// 写入 FUObjectItem / TUObjectArray（根据运行时扫描到的偏移动态生成）
inline void WriteFUObjectItemAndArray(std::ofstream& f)
{
    auto& ctx = Ctx();
    i32 itemSize = ctx.off.FUObjectItemSize > 0 ? ctx.off.FUObjectItemSize : 0x18;
    i32 chunkSize = ctx.off.ChunkSize > 0 ? ctx.off.ChunkSize : 0x10000;
    // Object 指针后的 padding
    i32 itemPad = itemSize - 8;

    f << std::format(R"(// Predefined struct FUObjectItem
// 0x{0:04X} (0x{0:04X} - 0x0000)
struct FUObjectItem final
{{
public:
	class UObject*                                Object;                                            // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         Pad_8[0x{1:X}];                                       // 0x0008(0x{1:04X})(Fixing Struct Size After Last Property [ Rei-SdkDumper ])
}};
#ifdef DUMPER7_ASSERTS_FUObjectItem
DUMPER7_ASSERTS_FUObjectItem;
#endif

)", itemSize, itemPad);

    f << std::format(R"(// Predefined struct TUObjectArray
// 0x0020 (0x0020 - 0x0000)
class TUObjectArray final
{{
public:
	static constexpr auto DecryptPtr = [](void* ObjPtr) -> uint8*
	{{
		return reinterpret_cast<uint8*>(ObjPtr);
	}};

	static constexpr int32                        ElementsPerChunk = 0x{0:X};                        // 0x0000(0x0004)(NOT AUTO-GENERATED PROPERTY)

	struct FUObjectItem**                         Objects;                                           // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         Pad_8[0x8];                                        // 0x0008(0x0008)(Fixing Size After Last Property [ Rei-SdkDumper ])
	int32                                         MaxElements;                                       // 0x0010(0x0004)(NOT AUTO-GENERATED PROPERTY)
	int32                                         NumElements;                                       // 0x0014(0x0004)(NOT AUTO-GENERATED PROPERTY)
	int32                                         MaxChunks;                                         // 0x0018(0x0004)(NOT AUTO-GENERATED PROPERTY)
	int32                                         NumChunks;                                         // 0x001C(0x0004)(NOT AUTO-GENERATED PROPERTY)

public:
	inline int32 Num() const
	{{
		return NumElements;
	}}
	
	FUObjectItem** GetDecrytedObjPtr() const
	{{
		return reinterpret_cast<FUObjectItem**>(DecryptPtr(Objects));
	}}
	
	inline class UObject* GetByIndex(const int32 Index) const
	{{
		const int32 ChunkIndex = Index / ElementsPerChunk;
		const int32 InChunkIdx = Index % ElementsPerChunk;
		
		if (Index < 0 || ChunkIndex >= NumChunks || Index >= NumElements)
		    return nullptr;
		
		FUObjectItem* ChunkPtr = GetDecrytedObjPtr()[ChunkIndex];
		if (!ChunkPtr) return nullptr;
		
		return ChunkPtr[InChunkIdx].Object;
	}}
}};
#ifdef DUMPER7_ASSERTS_TUObjectArray
DUMPER7_ASSERTS_TUObjectArray;
#endif

)", chunkSize);
}

// 写入 TUObjectArrayWrapper
inline void WriteTUObjectArrayWrapper(std::ofstream& f)
{
    f << R"(class TUObjectArrayWrapper
{
private:
	friend class UObject;

private:
	void* GObjectsAddress = nullptr;

private:
	TUObjectArrayWrapper() = default;

public:
	TUObjectArrayWrapper(TUObjectArrayWrapper&&) = delete;
	TUObjectArrayWrapper(const TUObjectArrayWrapper&) = delete;

	TUObjectArrayWrapper& operator=(TUObjectArrayWrapper&&) = delete;
	TUObjectArrayWrapper& operator=(const TUObjectArrayWrapper&) = delete;

private:
	inline void InitGObjects()
	{
		GObjectsAddress = reinterpret_cast<void*>(InSDKUtils::GetImageBase() + Offsets::GObjects);
	}

public:
	inline void InitManually(void* GObjectsAddressParameter)
	{
		GObjectsAddress = GObjectsAddressParameter;
	}

	inline class TUObjectArray* operator->()
	{
		if (!GObjectsAddress) [[unlikely]]
			InitGObjects();

		return reinterpret_cast<class TUObjectArray*>(GObjectsAddress);
	}

	inline TUObjectArray& operator*() const
	{
		return *reinterpret_cast<class TUObjectArray*>(GObjectsAddress);
	}

	inline operator const void* ()
	{
		if (!GObjectsAddress) [[unlikely]]
			InitGObjects();

		return GObjectsAddress;
	}

	inline class TUObjectArray* GetTypedPtr()
	{
		if (!GObjectsAddress) [[unlikely]]
			InitGObjects();

		return reinterpret_cast<class TUObjectArray*>(GObjectsAddress);
	}
};

)";
}

// 写入 FName 类
inline void WriteFNameClass(std::ofstream& f)
{
    f << R"(// Predefined struct FName
// 0x0008 (0x0008 - 0x0000)
class FName final
{
public:
	static inline void*                           AppendString = nullptr;                            // 0x0000(0x0004)(NOT AUTO-GENERATED PROPERTY)

	int32                                         ComparisonIndex;                                   // 0x0000(0x0004)(NOT AUTO-GENERATED PROPERTY)
	uint32                                        Number;                                            // 0x0004(0x0004)(NOT AUTO-GENERATED PROPERTY)

public:
	constexpr FName(int32 ComparisonIndex = 0, uint32 Number = 0)
		: ComparisonIndex(ComparisonIndex), Number(Number)
	{
	}

	static void InitManually(void* Location)
	{
		AppendString = reinterpret_cast<void*>(Location);
	}

	constexpr FName(const FName& other)
		: ComparisonIndex(other.ComparisonIndex), Number(other.Number)
	{
	}

	static void InitInternal()
	{
		AppendString = reinterpret_cast<void*>(InSDKUtils::GetImageBase() + Offsets::AppendString);
	}

	bool IsNone() const
	{
		return !ComparisonIndex&& !Number;
	}
	
	int32 GetDisplayIndex() const
	{
		return ComparisonIndex;
	}
	
	std::string GetRawString() const
	{
		wchar_t buffer[1024];
	    FString TempString(buffer, 0, 1024);
	
		if (!AppendString)
			InitInternal();
	
		InSDKUtils::CallGameFunction(reinterpret_cast<void(*)(const FName*, FString&)>(AppendString), this, TempString);
	
		return TempString.ToString();
	}
	
	std::string ToString() const
	{
		std::string OutputString = GetRawString();
	
		size_t pos = OutputString.rfind('/');
	
		if (pos == std::string::npos)
			return OutputString;
	
		return OutputString.substr(pos + 1);
	}
	

	FName& operator=(const FName& Other)
	{
		ComparisonIndex = Other.ComparisonIndex;
		Number = Other.Number;
	
		return *this;
	}

	bool operator==(const FName& Other) const
	{
		return ComparisonIndex == Other.ComparisonIndex && Number == Other.Number;
	}
	bool operator!=(const FName& Other) const
	{
		return ComparisonIndex != Other.ComparisonIndex || Number != Other.Number;
	}
};
#ifdef DUMPER7_ASSERTS_FName
DUMPER7_ASSERTS_FName;
#endif

)";
}

} // namespace gen
} // namespace xrd
