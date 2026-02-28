#pragma once
// Xrd-eXternalrEsolve - SDK 生成：Basic.hpp 第五部分
// FFieldClass / FFieldVariant / FField / FProperty 及其子类
// CyclicDependencyFixup 模板、命名空间闭合

#include "../../core/context.hpp"
#include <fstream>
#include <format>

namespace xrd
{
namespace gen
{

// 写入 FFieldClass / FFieldVariant / FField / FProperty 及所有子类
inline void WriteFFieldAndPropertyTypes(std::ofstream& f)
{
    f << R"(// Predefined struct FFieldClass
// 0x0028 (0x0028 - 0x0000)
class FFieldClass
{
public:
	FName                                         Name;                                              // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	uint64                                        Id;                                                // 0x0008(0x0008)(NOT AUTO-GENERATED PROPERTY)
	uint64                                        CastFlags;                                         // 0x0010(0x0008)(NOT AUTO-GENERATED PROPERTY)
	EClassFlags                                   ClassFlags;                                        // 0x0018(0x0004)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         Pad_1C[0x4];                                       // 0x001C(0x0004)(Fixing Size After Last Property [ Rei-SdkDumper ])
	class FFieldClass*                            SuperClass;                                        // 0x0020(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FFieldClass
DUMPER7_ASSERTS_FFieldClass;
#endif

// Predefined struct FFieldVariant
// 0x0010 (0x0010 - 0x0000)
class FFieldVariant
{
public:
	using ContainerType = union { class FField* Field; class UObject* Object; };

	ContainerType                                 Container;                                         // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	bool                                          bIsUObject;                                        // 0x0008(0x0001)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FFieldVariant
DUMPER7_ASSERTS_FFieldVariant;
#endif

// Predefined struct FField
// 0x0038 (0x0038 - 0x0000)
class FField
{
public:
	void*                                         VTable;                                            // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
	class FFieldClass*                            ClassPrivate;                                      // 0x0008(0x0008)(NOT AUTO-GENERATED PROPERTY)
	FFieldVariant                                 Owner;                                             // 0x0010(0x0010)(NOT AUTO-GENERATED PROPERTY)
	class FField*                                 Next;                                              // 0x0020(0x0008)(NOT AUTO-GENERATED PROPERTY)
	FName                                         Name;                                              // 0x0028(0x0008)(NOT AUTO-GENERATED PROPERTY)
	int32                                         ObjFlags;                                          // 0x0030(0x0004)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FField
DUMPER7_ASSERTS_FField;
#endif

// Predefined struct FProperty
// 0x0040 (0x0078 - 0x0038)
class FProperty : public FField
{
public:
	int32                                         ArrayDim;                                          // 0x0038(0x0004)(NOT AUTO-GENERATED PROPERTY)
	int32                                         ElementSize;                                       // 0x003C(0x0004)(NOT AUTO-GENERATED PROPERTY)
	uint64                                        PropertyFlags;                                     // 0x0040(0x0008)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         Pad_48[0x4];                                       // 0x0048(0x0004)(Fixing Size After Last Property [ Rei-SdkDumper ])
	int32                                         Offset;                                            // 0x004C(0x0004)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         Pad_50[0x28];                                      // 0x0050(0x0028)(Fixing Struct Size After Last Property [ Rei-SdkDumper ])
};
#ifdef DUMPER7_ASSERTS_FProperty
DUMPER7_ASSERTS_FProperty;
#endif

// Predefined struct FByteProperty
// 0x0008 (0x0080 - 0x0078)
class FByteProperty final : public FProperty
{
public:
	class UEnum*                                  Enum;                                              // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FByteProperty
DUMPER7_ASSERTS_FByteProperty;
#endif

// Predefined struct FBoolProperty
// 0x0008 (0x0080 - 0x0078)
class FBoolProperty final : public FProperty
{
public:
	uint8                                         FieldSize;                                         // 0x0078(0x0001)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         ByteOffset;                                        // 0x0079(0x0001)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         ByteMask;                                          // 0x007A(0x0001)(NOT AUTO-GENERATED PROPERTY)
	uint8                                         FieldMask;                                         // 0x007B(0x0001)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FBoolProperty
DUMPER7_ASSERTS_FBoolProperty;
#endif

// Predefined struct FObjectPropertyBase
// 0x0008 (0x0080 - 0x0078)
class FObjectPropertyBase : public FProperty
{
public:
	class UClass*                                 PropertyClass;                                     // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FObjectPropertyBase
DUMPER7_ASSERTS_FObjectPropertyBase;
#endif

// Predefined struct FClassProperty
// 0x0008 (0x0088 - 0x0080)
class FClassProperty final : public FObjectPropertyBase
{
public:
	class UClass*                                 MetaClass;                                         // 0x0080(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FClassProperty
DUMPER7_ASSERTS_FClassProperty;
#endif

// Predefined struct FStructProperty
// 0x0008 (0x0080 - 0x0078)
class FStructProperty final : public FProperty
{
public:
	class UStruct*                                Struct;                                            // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FStructProperty
DUMPER7_ASSERTS_FStructProperty;
#endif

// Predefined struct FArrayProperty
// 0x0008 (0x0080 - 0x0078)
class FArrayProperty final : public FProperty
{
public:
	class FProperty*                              InnerProperty;                                     // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FArrayProperty
DUMPER7_ASSERTS_FArrayProperty;
#endif

// Predefined struct FDelegateProperty
// 0x0008 (0x0080 - 0x0078)
class FDelegateProperty final : public FProperty
{
public:
	class UFunction*                              SignatureFunction;                                 // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FDelegateProperty
DUMPER7_ASSERTS_FDelegateProperty;
#endif

// Predefined struct FMapProperty
// 0x0010 (0x0088 - 0x0078)
class FMapProperty final : public FProperty
{
public:
	class FProperty*                              KeyProperty;                                       // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
	class FProperty*                              ValueProperty;                                     // 0x0080(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FMapProperty
DUMPER7_ASSERTS_FMapProperty;
#endif

// Predefined struct FSetProperty
// 0x0008 (0x0080 - 0x0078)
class FSetProperty final : public FProperty
{
public:
	class FProperty*                              ElementProperty;                                   // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FSetProperty
DUMPER7_ASSERTS_FSetProperty;
#endif

// Predefined struct FEnumProperty
// 0x0010 (0x0088 - 0x0078)
class FEnumProperty final : public FProperty
{
public:
	class FProperty*                              UnderlayingProperty;                               // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
	class UEnum*                                  Enum;                                              // 0x0080(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FEnumProperty
DUMPER7_ASSERTS_FEnumProperty;
#endif

// Predefined struct FFieldPathProperty
// 0x0008 (0x0080 - 0x0078)
class FFieldPathProperty final : public FProperty
{
public:
	class FFieldClass*                            FieldClass;                                        // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FFieldPathProperty
DUMPER7_ASSERTS_FFieldPathProperty;
#endif

// Predefined struct FOptionalProperty
// 0x0008 (0x0080 - 0x0078)
class FOptionalProperty final : public FProperty
{
public:
	class FProperty*                              ValueProperty;                                     // 0x0078(0x0008)(NOT AUTO-GENERATED PROPERTY)
};
#ifdef DUMPER7_ASSERTS_FOptionalProperty
DUMPER7_ASSERTS_FOptionalProperty;
#endif

)";
}

// 写入 CyclicDependencyFixup 模板和命名空间闭合
inline void WriteCyclicFixupAndClose(std::ofstream& f)
{
    f << R"(namespace CyclicDependencyFixupImpl
{

/*
* A wrapper for a Byte-Array of padding, that allows for casting to the actual underlaiyng type. Used for undefined structs in cylic headers.
*/
template<typename UnderlayingStructType, int32 Size, int32 Align>
struct alignas(Align) TCylicStructFixup
{
private:
	uint8 Pad[Size];

public:
	      UnderlayingStructType& GetTyped()       { return reinterpret_cast<      UnderlayingStructType&>(*this); }
	const UnderlayingStructType& GetTyped() const { return reinterpret_cast<const UnderlayingStructType&>(*this); }
};

/*
* A wrapper for a Byte-Array of padding, that inherited from UObject allows for casting to the actual underlaiyng type and access to basic UObject functionality. For cyclic classes.
*/
template<typename UnderlayingClassType, int32 Size, int32 Align = sizeof(void*), class BaseClassType = class UObject>
struct alignas(Align) TCyclicClassFixup : public BaseClassType
{
private:
	uint8 Pad[Size];

public:
	UnderlayingClassType*       GetTyped()       { return reinterpret_cast<      UnderlayingClassType*>(this); }
	const UnderlayingClassType* GetTyped() const { return reinterpret_cast<const UnderlayingClassType*>(this); }
};

}


template<typename UnderlayingStructType, int32 Size, int32 Align>
using TStructCycleFixup = CyclicDependencyFixupImpl::TCylicStructFixup<UnderlayingStructType, Size, Align>;


template<typename UnderlayingClassType, int32 Size, int32 Align = 0x8>
using TObjectBasedCycleFixup = CyclicDependencyFixupImpl::TCyclicClassFixup<UnderlayingClassType, Size, Align, class UObject>;

template<typename UnderlayingClassType, int32 Size, int32 Align = 0x8>
using TActorBasedCycleFixup = CyclicDependencyFixupImpl::TCyclicClassFixup<UnderlayingClassType, Size, Align, class AActor>;

}

)";
}

} // namespace gen
} // namespace xrd
