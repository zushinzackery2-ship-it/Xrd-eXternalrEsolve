#pragma once
// Xrd-eXternalrEsolve - ByteProperty/DelegateProperty 偏移发现
// 从 scan_property_offsets_extra.hpp 拆分

#include "scan_property_offsets.hpp"

namespace xrd
{
namespace resolve
{

// 发现 ByteProperty::Enum 偏移
// ByteProperty 关联一个 UEnum 指针，位于基础 Property 大小之后
inline bool DiscoverBytePropertyEnumOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    i32 baseSize = EstimateBasePropertySize(mem, off);
    i32 total = GetObjectCount(mem, off);

    for (i32 i = 0; i < std::min(total, 3000); ++i)
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

                if (fcName == "ByteProperty")
                {
                    // 搜索指向 UEnum 的指针
                    for (i32 t = baseSize; t <= baseSize + 0x20; t += 8)
                    {
                        uptr enumPtr = 0;
                        if (ReadPtr(mem, prop + t, enumPtr) &&
                            IsCanonicalUserPtr(enumPtr))
                        {
                            uptr ec = 0;
                            if (ReadPtr(mem,
                                enumPtr + off.UObject_Class, ec) &&
                                IsCanonicalUserPtr(ec))
                            {
                                FName ecn{};
                                if (ReadValue(mem,
                                    ec + off.UObject_Name, ecn))
                                {
                                    std::string cn = ResolveNameDirect(
                                        mem, off,
                                        ecn.ComparisonIndex,
                                        ecn.Number);
                                    if (cn == "Enum" ||
                                        cn == "UserDefinedEnum")
                                    {
                                        off.ByteProperty_Enum = t;
                                        std::cerr
                                            << "[xrd] ByteProperty"
                                            << "::Enum +0x"
                                            << std::hex << t
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
        if (off.ByteProperty_Enum != -1)
        {
            break;
        }
    }
    return off.ByteProperty_Enum != -1;
}

// 发现 DelegateProperty::SignatureFunction 偏移
// 指向一个 UFunction 对象
inline bool DiscoverDelegatePropertySigOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    i32 baseSize = EstimateBasePropertySize(mem, off);
    i32 total = GetObjectCount(mem, off);

    for (i32 i = 0; i < std::min(total, 3000); ++i)
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

                if (fcName == "DelegateProperty")
                {
                    for (i32 t = baseSize; t <= baseSize + 0x20; t += 8)
                    {
                        uptr funcPtr = 0;
                        if (ReadPtr(mem, prop + t, funcPtr) &&
                            IsCanonicalUserPtr(funcPtr))
                        {
                            uptr fc = 0;
                            if (ReadPtr(mem,
                                funcPtr + off.UObject_Class, fc) &&
                                IsCanonicalUserPtr(fc))
                            {
                                FName fcn{};
                                if (ReadValue(mem,
                                    fc + off.UObject_Name, fcn))
                                {
                                    std::string cn = ResolveNameDirect(
                                        mem, off,
                                        fcn.ComparisonIndex,
                                        fcn.Number);
                                    if (cn == "Function")
                                    {
                                        off.DelegateProperty_Sig = t;
                                        std::cerr
                                            << "[xrd] DelegateProperty"
                                            << "::Sig +0x"
                                            << std::hex << t
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
        if (off.DelegateProperty_Sig != -1)
        {
            break;
        }
    }
    return off.DelegateProperty_Sig != -1;
}

} // namespace resolve
} // namespace xrd
