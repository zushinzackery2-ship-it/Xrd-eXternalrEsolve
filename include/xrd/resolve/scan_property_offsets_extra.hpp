#pragma once
// Xrd-eXternalrEsolve - 类型化属性偏移发现（续）
// ArrayProperty::Inner, MapProperty::Base 以及汇总入口
// 从 scan_property_offsets.hpp 拆分，保持单文件 300 行以内

#include "scan_property_offsets.hpp"
#include "scan_property_offsets_struct.hpp"
#include "scan_property_offsets_typed.hpp"

namespace xrd
{
namespace resolve
{

// 发现 ArrayProperty::Inner 偏移
inline bool DiscoverArrayPropertyInnerOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    i32 baseSize = EstimateBasePropertySize(mem, off);
    i32 total = GetObjectCount(mem, off);

    for (i32 i = 0; i < std::min(total, 2000); ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        uptr cls = 0;
        if (!ReadPtr(mem, obj + off.UObject_Class, cls))
        {
            continue;
        }
        uptr metaCls = 0;
        if (!ReadPtr(mem, cls + off.UObject_Class, metaCls) ||
            metaCls != cls)
        {
            continue;
        }

        uptr prop = 0;
        if (off.UStruct_ChildProperties != -1)
        {
            ReadPtr(mem, obj + off.UStruct_ChildProperties, prop);
        }

        while (IsCanonicalUserPtr(prop))
        {
            uptr fieldCls = 0;
            ReadPtr(mem, prop + off.FField_Class, fieldCls);
            if (IsCanonicalUserPtr(fieldCls))
            {
                FName fn{};
                std::string fcName;
                if (ReadValue(mem, fieldCls + off.FFieldClass_Name, fn))
                {
                    fcName = ResolveNameDirect(
                        mem, off, fn.ComparisonIndex, fn.Number);
                }

                if (fcName == "ArrayProperty")
                {
                    for (i32 testOff = baseSize;
                         testOff <= baseSize + 0x20; testOff += 8)
                    {
                        uptr innerPtr = 0;
                        if (ReadPtr(mem, prop + testOff, innerPtr) &&
                            IsCanonicalUserPtr(innerPtr))
                        {
                            // 严格验证：Inner 指向一个 FField
                            // FField::Class (+0x08) 应指向 FFieldClass
                            // FFieldClass::Name (+0x00) 应可解析为属性类名
                            uptr innerCls = 0;
                            if (ReadPtr(mem,
                                innerPtr + off.FField_Class,
                                innerCls) &&
                                IsCanonicalUserPtr(innerCls))
                            {
                                FName icn{};
                                if (ReadValue(mem,
                                    innerCls + off.FFieldClass_Name,
                                    icn))
                                {
                                    std::string icName = ResolveNameDirect(
                                        mem, off,
                                        icn.ComparisonIndex,
                                        icn.Number);
                                    // 内部属性类名应包含 "Property"
                                    if (icName.find("Property") !=
                                        std::string::npos)
                                    {
                                        off.ArrayProperty_Inner = testOff;
                                        std::cerr
                                            << "[xrd] ArrayProperty::Inner +0x"
                                            << std::hex << testOff
                                            << std::dec << "\n";
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ReadPtr(mem, prop + off.FField_Next, prop);
        }

        if (off.ArrayProperty_Inner != -1)
        {
            break;
        }
    }

    return off.ArrayProperty_Inner != -1;
}

// 发现 MapProperty 的 Key/Value 偏移
inline bool DiscoverMapPropertyBaseOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    i32 baseSize = EstimateBasePropertySize(mem, off);
    i32 total = GetObjectCount(mem, off);

    for (i32 i = 0; i < std::min(total, 2000); ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        uptr cls = 0;
        if (!ReadPtr(mem, obj + off.UObject_Class, cls))
        {
            continue;
        }
        uptr metaCls = 0;
        if (!ReadPtr(mem, cls + off.UObject_Class, metaCls) ||
            metaCls != cls)
        {
            continue;
        }

        uptr prop = 0;
        if (off.UStruct_ChildProperties != -1)
        {
            ReadPtr(mem, obj + off.UStruct_ChildProperties, prop);
        }

        while (IsCanonicalUserPtr(prop))
        {
            uptr fieldCls = 0;
            ReadPtr(mem, prop + off.FField_Class, fieldCls);
            if (IsCanonicalUserPtr(fieldCls))
            {
                FName fn{};
                std::string fcName;
                if (ReadValue(mem, fieldCls + off.FFieldClass_Name, fn))
                {
                    fcName = ResolveNameDirect(
                        mem, off, fn.ComparisonIndex, fn.Number);
                }

                if (fcName == "MapProperty")
                {
                    for (i32 testOff = baseSize;
                         testOff <= baseSize + 0x20; testOff += 8)
                    {
                        uptr keyPtr = 0, valPtr = 0;
                        if (ReadPtr(mem, prop + testOff, keyPtr) &&
                            IsCanonicalUserPtr(keyPtr) &&
                            ReadPtr(mem, prop + testOff + 8, valPtr) &&
                            IsCanonicalUserPtr(valPtr))
                        {
                            // 严格验证：Key/Value 都是 FField
                            uptr kc = 0, vc = 0;
                            bool keyOk = false, valOk = false;
                            if (ReadPtr(mem,
                                keyPtr + off.FField_Class, kc) &&
                                IsCanonicalUserPtr(kc))
                            {
                                FName kcn{};
                                if (ReadValue(mem,
                                    kc + off.FFieldClass_Name, kcn))
                                {
                                    std::string kn = ResolveNameDirect(
                                        mem, off,
                                        kcn.ComparisonIndex,
                                        kcn.Number);
                                    keyOk = kn.find("Property") !=
                                        std::string::npos;
                                }
                            }
                            if (ReadPtr(mem,
                                valPtr + off.FField_Class, vc) &&
                                IsCanonicalUserPtr(vc))
                            {
                                FName vcn{};
                                if (ReadValue(mem,
                                    vc + off.FFieldClass_Name, vcn))
                                {
                                    std::string vn = ResolveNameDirect(
                                        mem, off,
                                        vcn.ComparisonIndex,
                                        vcn.Number);
                                    valOk = vn.find("Property") !=
                                        std::string::npos;
                                }
                            }
                            if (keyOk && valOk)
                            {
                                off.MapProperty_Base = testOff;
                                std::cerr
                                    << "[xrd] MapProperty::Base +0x"
                                    << std::hex << testOff
                                    << std::dec << "\n";
                                return true;
                            }
                        }
                    }
                }
            }
            ReadPtr(mem, prop + off.FField_Next, prop);
        }

        if (off.MapProperty_Base != -1)
        {
            break;
        }
    }

    return off.MapProperty_Base != -1;
}

// 一次性发现所有类型化属性偏移
inline void DiscoverAllPropertyOffsets(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    std::cerr << "[xrd] 开始发现类型化属性偏移...\n";
    DiscoverObjectPropertyClassOffset(mem, off);
    DiscoverStructPropertyStructOffset(mem, off);
    DiscoverArrayPropertyInnerOffset(mem, off);
    DiscoverMapPropertyBaseOffset(mem, off);
    DiscoverBytePropertyEnumOffset(mem, off);
    DiscoverDelegatePropertySigOffset(mem, off);

    // SetProperty::ElementProp 通常和 ArrayProperty::Inner 在同一偏移
    if (off.ArrayProperty_Inner != -1 &&
        off.SetProperty_ElementProp == -1)
    {
        off.SetProperty_ElementProp = off.ArrayProperty_Inner;
    }

    // EnumProperty::Base 通常和 StructProperty::Struct 在同一偏移
    if (off.StructProperty_Struct != -1 &&
        off.EnumProperty_Base == -1)
    {
        off.EnumProperty_Base = off.StructProperty_Struct;
    }

    // ClassProperty::MetaClass 紧跟 ObjectProperty::Class 之后
    if (off.ObjectProperty_Class != -1 &&
        off.ClassProperty_MetaClass == -1)
    {
        off.ClassProperty_MetaClass = off.ObjectProperty_Class + 8;
    }

    // ByteProperty::Enum 回退到 PropertySize
    if (off.ByteProperty_Enum == -1)
    {
        off.ByteProperty_Enum = EstimateBasePropertySize(mem, off);
    }

    // DelegateProperty::Sig 回退到 PropertySize
    if (off.DelegateProperty_Sig == -1)
    {
        off.DelegateProperty_Sig = EstimateBasePropertySize(mem, off);
    }

    std::cerr << "[xrd] 类型化属性偏移发现完成\n";
}

} // namespace resolve
} // namespace xrd
