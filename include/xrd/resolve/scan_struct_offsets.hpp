#pragma once
// Xrd-eXternalrEsolve - UStruct 偏移发现
// 参考 Rei-Dumper: 通过已知继承关系精确定位 SuperStruct
// Class->Struct->Field 的继承链是 UE 固定的

#include "scan_offsets.hpp"
#include "scan_property_offsets.hpp"
#include <iostream>

namespace xrd
{
namespace resolve
{

// 在 GObjects 中按名字查找对象（仅用于偏移发现阶段）
inline uptr FindObjectByNameForResolve(
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    const std::string& targetName,
    const std::string& targetClassName = "")
{
    i32 total = GetObjectCount(mem, off);
    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        FName fn{};
        if (!ReadValue(mem, obj + off.UObject_Name, fn))
        {
            continue;
        }
        std::string name = ResolveNameDirect(
            mem, off, fn.ComparisonIndex, fn.Number);
        if (name != targetName)
        {
            continue;
        }

        // 如果指定了类名过滤
        if (!targetClassName.empty())
        {
            uptr cls = 0;
            if (!ReadPtr(mem, obj + off.UObject_Class, cls) ||
                !IsCanonicalUserPtr(cls))
            {
                continue;
            }
            FName clsFn{};
            if (!ReadValue(mem, cls + off.UObject_Name, clsFn))
            {
                continue;
            }
            std::string cn = ResolveNameDirect(
                mem, off, clsFn.ComparisonIndex, clsFn.Number);
            if (cn != targetClassName)
            {
                continue;
            }
        }

        return obj;
    }
    return 0;
}

// 在对象内存中搜索指向目标地址的指针偏移
inline i32 FindPointerOffset(
    const IMemoryAccessor& mem,
    uptr obj,
    uptr target,
    i32 searchStart,
    i32 searchEnd)
{
    for (i32 testOff = searchStart; testOff <= searchEnd; testOff += 8)
    {
        uptr val = 0;
        if (ReadPtr(mem, obj + testOff, val) && val == target)
        {
            return testOff;
        }
    }
    return -1;
}

// 通过已知继承关系发现 UStruct 偏移
// 参考 Rei-Dumper 的 FindSuperOffset:
//   Struct.Super == Field, Class.Super == Struct
inline bool DiscoverStructOffsets(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    // 查找关键类对象
    uptr classObj = FindObjectByNameForResolve(
        mem, off, "Class", "Class");
    uptr structObj = FindObjectByNameForResolve(
        mem, off, "Struct", "Class");
    uptr fieldObj = FindObjectByNameForResolve(
        mem, off, "Field", "Class");

    // UE5 中 Struct 可能叫 "struct"（小写）
    if (!structObj)
    {
        structObj = FindObjectByNameForResolve(
            mem, off, "struct", "Class");
    }

    if (!classObj || !structObj)
    {
        std::cerr << "[xrd] 未找到 Class/Struct 元类对象\n";
        return false;
    }

    // ─── UStruct::SuperStruct ───
    // Class.Super 应该指向 Struct
    i32 uobjectEnd = off.UObject_Outer + 8;
    i32 superOff = FindPointerOffset(
        mem, classObj, structObj, uobjectEnd, 0x80);

    if (superOff == -1)
    {
        // 回退：Struct.Super 应该指向 Field
        if (fieldObj)
        {
            superOff = FindPointerOffset(
                mem, structObj, fieldObj, uobjectEnd, 0x80);
        }
    }

    if (superOff == -1)
    {
        std::cerr << "[xrd] SuperStruct 偏移未找到\n";
        return false;
    }

    // 交叉验证：如果有 Field，验证 Struct.Super == Field
    if (fieldObj)
    {
        uptr structSuper = 0;
        ReadPtr(mem, structObj + superOff, structSuper);
        if (structSuper != fieldObj)
        {
            std::cerr << "[xrd] SuperStruct 交叉验证失败\n";
        }
    }

    off.UStruct_SuperStruct = superOff;
    std::cerr << "[xrd] UStruct::SuperStruct +0x"
              << std::hex << superOff << std::dec << "\n";

    // ─── UStruct::Children (UField* 链) ───
    // 参考 Rei-Dumper: 通过 PlayerController 的函数链来定位
    // 简化方案：在 SuperStruct 之后搜索有效指针或 null
    // Children 指向 UField 链（函数等），大部分类都有
    uptr vectorStruct = FindObjectByNameForResolve(
        mem, off, "Vector", "ScriptStruct");
    if (!vectorStruct)
    {
        vectorStruct = FindObjectByNameForResolve(
            mem, off, "Vector", "Struct");
    }

    // Vector 结构体的 Children 应该指向 X 属性（UField 链）
    // 但在 FProperty 模式下 Children 可能为 null
    // 搜索 SuperStruct 之后的第一个有效指针槽
    for (i32 childOff = superOff + 8;
         childOff <= superOff + 0x20;
         childOff += 8)
    {
        // 统计：大部分类的 Children 要么是 null 要么是有效指针
        int validCount = 0;
        int nonNullCount = 0;
        int sampleCount = 0;

        for (i32 i = 0; i < std::min((i32)GetObjectCount(mem, off), 2000) && sampleCount < 50; ++i)
        {
            uptr obj = ReadObjectAt(mem, off, i);
            if (!IsCanonicalUserPtr(obj)) continue;

            uptr cls = 0;
            ReadPtr(mem, obj + off.UObject_Class, cls);
            if (!IsCanonicalUserPtr(cls)) continue;

            uptr metaCls = 0;
            ReadPtr(mem, cls + off.UObject_Class, metaCls);
            if (metaCls != cls) continue;

            sampleCount++;
            uptr child = 0;
            ReadPtr(mem, obj + childOff, child);
            if (child == 0 || IsCanonicalUserPtr(child))
            {
                validCount++;
                if (child != 0) nonNullCount++;
            }
        }

        if (sampleCount > 10 &&
            validCount > sampleCount * 80 / 100)
        {
            off.UStruct_Children = childOff;
            std::cerr << "[xrd] UStruct::Children +0x"
                      << std::hex << childOff << std::dec << "\n";
            break;
        }
    }

    // ─── UStruct::ChildProperties (FField*) ───
    // 紧跟 Children 之后
    if (off.UStruct_Children != -1)
    {
        i32 cpOff = off.UStruct_Children + 8;

        // 验证：用已知有属性的结构体（如 Vector）
        // Vector 的 ChildProperties 应该非 null
        bool verified = false;
        if (vectorStruct)
        {
            uptr cp = 0;
            ReadPtr(mem, vectorStruct + cpOff, cp);
            if (IsCanonicalUserPtr(cp))
            {
                verified = true;
            }
        }

        if (verified)
        {
            off.UStruct_ChildProperties = cpOff;
            off.bUseFProperty = true;
            std::cerr << "[xrd] UStruct::ChildProperties +0x"
                      << std::hex << cpOff << std::dec << "\n";
        }
        else
        {
            // 回退：不使用 FProperty
            std::cerr << "[xrd] ChildProperties 未验证，"
                      << "回退到 UField 模式\n";
        }
    }

    // ─── UStruct::Size ───
    // 参考 Rei-Dumper: Color.Size==4, Guid.Size==0x10
    uptr colorStruct = FindObjectByNameForResolve(
        mem, off, "Color", "ScriptStruct");
    if (!colorStruct)
    {
        colorStruct = FindObjectByNameForResolve(
            mem, off, "Color", "Struct");
    }
    uptr guidStruct = FindObjectByNameForResolve(
        mem, off, "Guid", "ScriptStruct");
    if (!guidStruct)
    {
        guidStruct = FindObjectByNameForResolve(
            mem, off, "Guid", "Struct");
    }

    i32 sizeSearchStart = (off.UStruct_ChildProperties != -1)
        ? off.UStruct_ChildProperties + 8
        : (off.UStruct_Children != -1)
            ? off.UStruct_Children + 8
            : superOff + 0x18;

    for (i32 sizeOff = sizeSearchStart;
         sizeOff <= sizeSearchStart + 0x20;
         sizeOff += 4)
    {
        bool colorOk = false, guidOk = false;

        if (colorStruct)
        {
            i32 val = 0;
            ReadValue(mem, colorStruct + sizeOff, val);
            colorOk = (val == 0x04);
        }
        if (guidStruct)
        {
            i32 val = 0;
            ReadValue(mem, guidStruct + sizeOff, val);
            guidOk = (val == 0x10);
        }

        if (colorOk && guidOk)
        {
            off.UStruct_Size = sizeOff;
            break;
        }
        else if ((colorOk || guidOk) && off.UStruct_Size == -1)
        {
            // 只有一个匹配，继续搜索但记录候选
            off.UStruct_Size = sizeOff;
        }
    }

    if (off.UStruct_Size != -1)
    {
        std::cerr << "[xrd] UStruct::Size +0x"
                  << std::hex << off.UStruct_Size
                  << std::dec << "\n";
    }

    return off.UStruct_SuperStruct != -1;
}

} // namespace resolve
} // namespace xrd
