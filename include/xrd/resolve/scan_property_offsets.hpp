#pragma once
// Xrd-eXternalrEsolve - 类型化属性偏移发现
// 发现 ObjectProperty::Class, StructProperty::Struct, ArrayProperty::Inner 等偏移
// 这些偏移用于精确解析属性类型名

#include "../core/context.hpp"
#include "../engine/names.hpp"
#include "scan_offsets.hpp"
#include <iostream>
#include <algorithm>

namespace xrd
{
namespace resolve
{

// 在 AutoInit 过程中解析 FName（不检查 IsInited）
// 因为 AutoInit 尚未完成时 IsInited() 返回 false
inline std::string ResolveNameDirect(
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    i32 compIdx,
    i32 number = 0)
{
    if (compIdx <= 0 || off.GNames == 0)
    {
        return "";
    }

    std::string name;
    bool ok = false;
    if (off.bUseNamePool)
    {
        ok = ResolveName_NamePool(mem, off.GNames, compIdx, name);
    }
    else
    {
        ok = ResolveName_Array(mem, off.GNames, compIdx, name);
    }

    if (ok && !name.empty())
    {
        if (number > 0)
        {
            name += "_" + std::to_string(number - 1);
        }
        return name;
    }
    return "";
}

// 通用策略：类型化属性的额外字段紧跟在基础 Property 布局之后
// 基础 Property 大小 = BoolProperty::Base 或 EnumProperty::Base（取较小者）
// 各类型化属性在基础大小之后存放指向关联对象的指针

// 发现基础 Property 大小（所有类型化属性的公共前缀长度）
// 参考 Rei-Dumper: PropertySize = BoolProperty::Base 或 EnumProperty::Base
inline i32 EstimateBasePropertySize(const IMemoryAccessor& /*mem*/, const UEOffsets& off)
{
    // 优先使用 BoolProperty::Base（Rei-Dumper 的做法）
    if (off.BoolProperty_Base != -1)
    {
        return off.BoolProperty_Base;
    }
    // 回退：Property_Offset 之后还有大量链表指针
    // UE4.27 典型布局：Offset_Internal(+0x4C) 之后有
    // RepNotifyFunc(+0x50,8) + PropertyLinkNext(+0x58,8) +
    // NextRef(+0x60,8) + DestructorLinkNext(+0x68,8) +
    // PostConstructLinkNext(+0x70,8) = 总共 0x78
    if (off.Property_Offset != -1)
    {
        return off.Property_Offset + 0x2C; // 0x4C + 0x2C = 0x78
    }
    return 0x78;
}

// 在指定偏移范围内搜索指向合法 UObject 的指针
inline i32 FindPointerFieldInRange(
    const IMemoryAccessor& mem,
    uptr obj,
    i32 startOff,
    i32 endOff,
    bool mustBeValid)
{
    for (i32 off = startOff; off <= endOff; off += 8)
    {
        uptr ptr = 0;
        if (ReadPtr(mem, obj + off, ptr))
        {
            if (mustBeValid && IsCanonicalUserPtr(ptr))
            {
                return off;
            }
            if (!mustBeValid && (ptr == 0 || IsCanonicalUserPtr(ptr)))
            {
                return off;
            }
        }
    }
    return -1;
}

// 发现 ObjectProperty::PropertyClass 偏移
// ObjectProperty 在基础 Property 之后存放一个指向 UClass 的指针
inline bool DiscoverObjectPropertyClassOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    i32 baseSize = EstimateBasePropertySize(mem, off);
    i32 total = GetObjectCount(mem, off);

    // 搜索 ObjectProperty 类型的属性
    for (i32 i = 0; i < std::min(total, 3000); ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        // 检查是否为 ObjectProperty 的 FField
        if (off.bUseFProperty)
        {
            // 跳过：FField 不在 GObjects 中，需要从 struct 的属性链遍历
            continue;
        }

        // UProperty 模式：检查类名
        uptr cls = 0;
        if (!ReadPtr(mem, obj + off.UObject_Class, cls) || !IsCanonicalUserPtr(cls))
        {
            continue;
        }

        // 读取类名
        FName clsName{};
        if (!ReadValue(mem, cls + off.UObject_Name, clsName))
        {
            continue;
        }
    }

    // 对于 FField 模式，从已知的 Class 对象的属性链中搜索
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

        // 只处理 Class 类型的对象
        uptr metaCls = 0;
        if (!ReadPtr(mem, cls + off.UObject_Class, metaCls) || metaCls != cls)
        {
            continue;
        }

        // 遍历 ChildProperties 链
        uptr prop = 0;
        if (off.UStruct_ChildProperties != -1)
        {
            ReadPtr(mem, obj + off.UStruct_ChildProperties, prop);
        }

        while (IsCanonicalUserPtr(prop))
        {
            // 读取 FField 类名
            uptr fieldCls = 0;
            ReadPtr(mem, prop + off.FField_Class, fieldCls);
            if (IsCanonicalUserPtr(fieldCls))
            {
                std::string fcName;
                FName fn{};
                if (ReadValue(mem, fieldCls + off.FFieldClass_Name, fn))
                {
                    fcName = ResolveNameDirect(mem, off, fn.ComparisonIndex, fn.Number);
                }

                if (fcName == "ObjectProperty" || fcName == "ObjectPropertyBase")
                {
                    // 在 baseSize ~ baseSize+0x20 范围搜索指向 UClass 的指针
                    for (i32 testOff = baseSize; testOff <= baseSize + 0x20; testOff += 8)
                    {
                        uptr classPtr = 0;
                        if (ReadPtr(mem, prop + testOff, classPtr) &&
                            IsCanonicalUserPtr(classPtr))
                        {
                            // 严格验证：目标必须是 UClass（元类检查）
                            // UClass 的 Class 指针指向自身或另一个 Class
                            uptr cc = 0;
                            if (ReadPtr(mem, classPtr + off.UObject_Class, cc) &&
                                IsCanonicalUserPtr(cc))
                            {
                                // 元类的 Class 必须指向自身
                                uptr metaCc = 0;
                                if (ReadPtr(mem, cc + off.UObject_Class, metaCc) &&
                                    metaCc == cc)
                                {
                                    // 额外验证：读取目标的名字，确保可解析
                                    FName targetName{};
                                    if (ReadValue(mem, classPtr + off.UObject_Name, targetName))
                                    {
                                        std::string tn = ResolveNameDirect(
                                            mem, off, targetName.ComparisonIndex, targetName.Number);
                                        if (!tn.empty() && tn.size() < 256)
                                        {
                                            off.ObjectProperty_Class = testOff;
                                            std::cerr << "[xrd] ObjectProperty::Class +0x"
                                                      << std::hex << testOff << std::dec << "\n";
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 下一个 FField
            ReadPtr(mem, prop + off.FField_Next, prop);
        }

        if (off.ObjectProperty_Class != -1)
        {
            break;
        }
    }

    return off.ObjectProperty_Class != -1;
}

} // namespace resolve
} // namespace xrd
