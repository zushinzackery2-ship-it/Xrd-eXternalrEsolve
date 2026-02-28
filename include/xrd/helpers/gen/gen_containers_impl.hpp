#pragma once
// Xrd-eXternalrEsolve - SDK 生成：UnrealContainers 容器实现核心
// 从 gen_containers.hpp 拆分，包含 TArray / FString / TSet / TMap 等
// 写入到 UnrealContainers.hpp 的 UC 命名空间内

#include <fstream>

namespace xrd
{
namespace gen
{

// 写入容器实现核心（TArray / FString / TSet / TMap 等）
// 必须在 GenerateUnrealContainers 之前定义，因为后者会调用它
inline void WriteContainerImpl(std::ofstream& f)
{
    f << R"(
	namespace ContainerImpl
	{
		template<int32 Size, uint32 Alignment>
		struct TAlignedBytes
		{
			alignas(Alignment) uint8 Pad[Size];
		};

		template<uint32 NumInlineElements>
		class TInlineAllocator
		{
		public:
			template<typename ElementType>
			class ForElementType
			{
			private:
				static constexpr int32 ElementSize = sizeof(ElementType);
				static constexpr int32 ElementAlign = alignof(ElementType);
			private:
				TAlignedBytes<ElementSize, ElementAlign> InlineData[NumInlineElements];
				ElementType* SecondaryData;
			public:
				ForElementType() : InlineData{ 0x0 }, SecondaryData(nullptr) {}
				ForElementType(ForElementType&&) = default;
				ForElementType(const ForElementType&) = default;
				ForElementType& operator=(ForElementType&&) = default;
				ForElementType& operator=(const ForElementType&) = default;
				inline const ElementType* GetAllocation() const { return SecondaryData ? SecondaryData : reinterpret_cast<const ElementType*>(&InlineData); }
				inline uint32 GetNumInlineBytes() const { return NumInlineElements; }
			};
		};

		class FBitArray
		{
		protected:
			static constexpr int32 NumBitsPerDWORD = 32;
			static constexpr int32 NumBitsPerDWORDLogTwo = 5;
		private:
			TInlineAllocator<4>::ForElementType<int32> Data;
			int32 NumBits;
			int32 MaxBits;
		public:
			FBitArray() : NumBits(0), MaxBits(Data.GetNumInlineBytes() * NumBitsPerDWORD) {}
			FBitArray(const FBitArray&) = default;
			FBitArray(FBitArray&&) = default;
			FBitArray& operator=(FBitArray&&) = default;
			FBitArray& operator=(const FBitArray& Other) = default;
			inline int32 Num() const { return NumBits; }
			inline int32 Max() const { return MaxBits; }
			inline const uint32* GetData() const { return reinterpret_cast<const uint32*>(Data.GetAllocation()); }
			inline bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < NumBits; }
			inline bool IsValid() const { return GetData() && NumBits > 0; }
			inline bool operator[](int32 Index) const { return GetData()[Index / NumBitsPerDWORD] & (1 << (Index & (NumBitsPerDWORD - 1))); }
		};

		template<typename SparseArrayType>
		union TSparseArrayElementOrFreeListLink
		{
			SparseArrayType ElementData;
			struct { int32 PrevFreeIndex; int32 NextFreeIndex; };
		};

		template<typename SetType>
		class SetElement
		{
			template<typename SetDataType> friend class TSet;
			SetType Value;
			int32 HashNextId;
			int32 HashIndex;
		};
	}

	template <typename KeyType, typename ValueType>
	class TPair
	{
	public:
		KeyType First;
		ValueType Second;
		TPair(KeyType Key, ValueType Value) : First(Key), Second(Value) {}
		inline KeyType& Key() { return First; }
		inline const KeyType& Key() const { return First; }
		inline ValueType& Value() { return Second; }
		inline const ValueType& Value() const { return Second; }
	};

	template<typename ArrayElementType>
	class TArray
	{
	protected:
		ArrayElementType* Data;
		int32 NumElements;
		int32 MaxElements;
	public:
		TArray() : Data(nullptr), NumElements(0), MaxElements(0) {}
		TArray(ArrayElementType* Data, int32 NumElements, int32 MaxElements)
			: Data(Data), NumElements(NumElements), MaxElements(MaxElements) {}
		TArray(const TArray&) = default;
		TArray(TArray&&) = default;
		TArray& operator=(TArray&&) = default;
		TArray& operator=(const TArray&) = default;
		inline int32 Num() const { return NumElements; }
		inline int32 Max() const { return MaxElements; }
		inline const ArrayElementType* GetDataPtr() const { return Data; }
		inline bool IsValidIndex(int32 Index) const { return Data && Index >= 0 && Index < NumElements; }
		inline bool IsValid() const { return Data && NumElements > 0 && MaxElements >= NumElements; }
		inline ArrayElementType& operator[](int32 Index) { return Data[Index]; }
		inline const ArrayElementType& operator[](int32 Index) const { return Data[Index]; }
		inline bool operator==(const TArray& Other) const { return Data == Other.Data; }
		inline bool operator!=(const TArray& Other) const { return Data != Other.Data; }
		inline explicit operator bool() const { return IsValid(); };
	};

	class FString : public TArray<wchar_t>
	{
	public:
		using TArray::TArray;
		FString(const wchar_t* Str)
		{
			const uint32 NullTerminatedLength = static_cast<uint32>(wcslen(Str) + 0x1);
			Data = const_cast<wchar_t*>(Str);
			NumElements = NullTerminatedLength;
			MaxElements = NullTerminatedLength;
		}
		FString(wchar_t* Str, int32 Num, int32 Max)
		{
			Data = Str;
			NumElements = Num;
			MaxElements = Max;
		}
		inline std::string ToString() const
		{
			if (*this)
				return UtfN::Utf16StringToUtf8String<std::string>(Data, NumElements - 1);
			return "";
		}
		inline std::wstring ToWString() const
		{
			if (*this) return std::wstring(Data);
			return L"";
		}
		inline wchar_t* CStr() { return Data; }
		inline const wchar_t* CStr() const { return Data; }
	};

	template<typename SparseArrayElementType>
	class TSparseArray
	{
		static constexpr uint32 ElementAlign = alignof(SparseArrayElementType);
		static constexpr uint32 ElementSize = sizeof(SparseArrayElementType);
		using FElementOrFreeListLink = ContainerImpl::TSparseArrayElementOrFreeListLink<ContainerImpl::TAlignedBytes<ElementSize, ElementAlign>>;
		TArray<FElementOrFreeListLink> Data;
		ContainerImpl::FBitArray AllocationFlags;
		int32 FirstFreeIndex;
		int32 NumFreeIndices;
	public:
		TSparseArray() : FirstFreeIndex(-1), NumFreeIndices(0) {}
		inline int32 Num() const { return Data.Num() - NumFreeIndices; }
		inline int32 Max() const { return Data.Max(); }
		inline int32 NumAllocated() const { return Data.Num(); }
		inline bool IsValidIndex(int32 Index) const { return Data.IsValidIndex(Index) && AllocationFlags[Index]; }
		inline bool IsValid() const { return Data.IsValid() && AllocationFlags.IsValid(); }
		const ContainerImpl::FBitArray& GetAllocationFlags() const { return AllocationFlags; }
	};

	template<typename SetElementType>
	class TSet
	{
		using SetDataType = ContainerImpl::SetElement<SetElementType>;
		using HashType = ContainerImpl::TInlineAllocator<1>::ForElementType<int32>;
		TSparseArray<SetDataType> Elements;
		HashType Hash;
		int32 HashSize;
	public:
		TSet() : HashSize(0) {}
		inline int32 Num() const { return Elements.Num(); }
		inline int32 Max() const { return Elements.Max(); }
		inline int32 NumAllocated() const { return Elements.NumAllocated(); }
		inline bool IsValidIndex(int32 Index) const { return Elements.IsValidIndex(Index); }
		inline bool IsValid() const { return Elements.IsValid(); }
		const ContainerImpl::FBitArray& GetAllocationFlags() const { return Elements.GetAllocationFlags(); }
	};

	template<typename KeyElementType, typename ValueElementType>
	class TMap
	{
	public:
		using ElementType = TPair<KeyElementType, ValueElementType>;
	private:
		TSet<ElementType> Elements;
	public:
		inline int32 Num() const { return Elements.Num(); }
		inline int32 Max() const { return Elements.Max(); }
		inline int32 NumAllocated() const { return Elements.NumAllocated(); }
		inline bool IsValidIndex(int32 Index) const { return Elements.IsValidIndex(Index); }
		inline bool IsValid() const { return Elements.IsValid(); }
		const ContainerImpl::FBitArray& GetAllocationFlags() const { return Elements.GetAllocationFlags(); }
	};

#if defined(_WIN64)
	static_assert(sizeof(TArray<int32>) == 0x10, "TArray has a wrong size!");
	static_assert(sizeof(TSet<int32>) == 0x50, "TSet has a wrong size!");
	static_assert(sizeof(TMap<int32, int32>) == 0x50, "TMap has a wrong size!");
#endif
}

)";
}

} // namespace gen
} // namespace xrd
