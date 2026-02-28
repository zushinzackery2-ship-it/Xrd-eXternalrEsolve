#pragma once
// Xrd-eXternalrEsolve - StructProperty::Struct 偏移发现
// 从 scan_property_offsets.hpp 拆分，保持单文件 300 行以内

#include "scan_property_offsets.hpp"

namespace xrd
{
namespace resolve
{

// 发现 StructProperty::Struct 偏移
// StructProperty::Struct 和 ObjectProperty::Class 在所有 UE 版本中偏移相同
// 因为它们都是 FProperty 子类，在基础布局之后的第一个指针字段
// 如果 ObjectProperty::Class 已发现，直接复用；否则独立搜索
inline bool DiscoverStructPropertyStructOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    // 优先复用 ObjectProperty::Class（两者偏移相同）
    if (off.ObjectProperty_Class != -1)
    {
        off.StructProperty_Struct = off.ObjectProperty_Class;
        std::cerr << "[xrd] StructProperty::Struct +0x"
                  << std::hex << off.StructProperty_Struct
                  << std::dec << " (同 ObjectProperty::Class)\n";
        return true;
    }

    // 回退：独立搜索（使用宽松验证，和 ObjectProperty 一致）
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
                if (ReadValue(mem,
                    fieldCls + off.FFieldClass_Name, fn))
                {
                    fcName = ResolveNameDirect(
                        mem, off,
                        fn.ComparisonIndex, fn.Number);
                }

                if (fcName == "StructProperty")
                {
                    for (i32 testOff = baseSize;
                         testOff <= baseSize + 0x20;
                         testOff += 8)
                    {
                        uptr structPtr = 0;
                        if (ReadPtr(mem, prop + testOff,
                            structPtr) &&
                            IsCanonicalUserPtr(structPtr))
                        {
                            // 验证：元类检查
                            uptr sc = 0;
                            if (ReadPtr(mem,
                                structPtr + off.UObject_Class,
                                sc) &&
                                IsCanonicalUserPtr(sc))
                            {
                                uptr metaSc = 0;
                                if (ReadPtr(mem,
                                    sc + off.UObject_Class,
                                    metaSc) &&
                                    metaSc == sc)
                                {
                                    // 验证目标名字可解析
                                    FName tn{};
                                    if (ReadValue(mem,
                                        structPtr +
                                        off.UObject_Name, tn))
                                    {
                                        std::string name =
                                            ResolveNameDirect(
                                                mem, off,
                                                tn.ComparisonIndex,
                                                tn.Number);
                                        if (!name.empty() &&
                                            name.size() < 256)
                                        {
                                            off.StructProperty_Struct
                                                = testOff;
                                            std::cerr
                                                << "[xrd] Struct"
                                                << "Property::Struct"
                                                << " +0x"
                                                << std::hex
                                                << testOff
                                                << std::dec
                                                << "\n";
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ReadPtr(mem, prop + off.FField_Next, prop);
        }

        if (off.StructProperty_Struct != -1)
        {
            break;
        }
    }

    return off.StructProperty_Struct != -1;
}

} // namespace resolve
} // namespace xrd
