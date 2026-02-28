#pragma once
// Xrd-eXternalrEsolve - Property 基础偏移发现
// 发现 Property::ArrayDim, ElementSize, PropertyFlags, Offset_Internal
// 参考 Rei-Dumper: 通过已知结构体成员的已知值来定位

#include "../core/context.hpp"
#include "../engine/names.hpp"
#include "scan_offsets.hpp"
#include <iostream>
#include <algorithm>

namespace xrd
{
namespace resolve
{

// 在 FField 链中找到指定名字的属性
inline uptr FindPropertyInChain(
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    uptr structObj,
    const std::string& propName)
{
    uptr prop = 0;
    if (off.UStruct_ChildProperties != -1)
    {
        ReadPtr(mem, structObj + off.UStruct_ChildProperties, prop);
    }

    while (IsCanonicalUserPtr(prop))
    {
        FName fn{};
        if (ReadValue(mem, prop + off.FField_Name, fn))
        {
            std::string name = ResolveNameDirect(
                mem, off, fn.ComparisonIndex, fn.Number);
            if (name == propName)
            {
                return prop;
            }
        }
        ReadPtr(mem, prop + off.FField_Next, prop);
    }
    return 0;
}

// 通过名字在 GObjects 中找到一个 UStruct 对象
inline uptr FindStructByName(
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    const std::string& targetName)
{
    i32 total = GetObjectCount(mem, off);
    // 全量搜索，Guid 等基础结构体可能在任意位置
    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        FName objFn{};
        if (!ReadValue(mem, obj + off.UObject_Name, objFn))
        {
            continue;
        }
        std::string objName = ResolveNameDirect(
            mem, off, objFn.ComparisonIndex, objFn.Number);
        if (objName != targetName)
        {
            continue;
        }

        // 验证是 Class 或 ScriptStruct 类型
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
        std::string clsName = ResolveNameDirect(
            mem, off, clsFn.ComparisonIndex, clsFn.Number);
        if (clsName == "Class" || clsName == "ScriptStruct" ||
            clsName == "Struct")
        {
            return obj;
        }
    }
    return 0;
}

// 发现 Property::ElementSize 偏移
// Guid 结构体的成员 A 的 ElementSize 应为 4
inline bool DiscoverPropertyElementSizeOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    uptr guidStruct = FindStructByName(mem, off, "Guid");
    if (!guidStruct)
    {
        std::cerr << "[xrd] 未找到 Guid 结构体\n";
        return false;
    }

    uptr propA = FindPropertyInChain(mem, off, guidStruct, "A");
    uptr propD = FindPropertyInChain(mem, off, guidStruct, "D");
    if (!propA || !propD)
    {
        std::cerr << "[xrd] 未找到 Guid.A/D 属性\n";
        return false;
    }

    // 在 FField 布局之后搜索值为 4 的 i32
    // FField 基础大小约 0x30~0x38
    for (i32 testOff = 0x30; testOff <= 0x60; testOff += 4)
    {
        i32 valA = 0, valD = 0;
        ReadValue(mem, propA + testOff, valA);
        ReadValue(mem, propD + testOff, valD);
        if (valA == 4 && valD == 4)
        {
            off.Property_ElementSize = testOff;
            std::cerr << "[xrd] Property::ElementSize +0x"
                      << std::hex << testOff << std::dec << "\n";
            return true;
        }
    }
    return false;
}

// 发现 Property::ArrayDim 偏移
// Guid.A 的 ArrayDim 应为 1，在 ElementSize 附近搜索
inline bool DiscoverPropertyArrayDimOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (off.Property_ElementSize == -1)
    {
        return false;
    }

    uptr guidStruct = FindStructByName(mem, off, "Guid");
    if (!guidStruct) return false;

    uptr propA = FindPropertyInChain(mem, off, guidStruct, "A");
    uptr propC = FindPropertyInChain(mem, off, guidStruct, "C");
    if (!propA || !propC) return false;

    i32 lo = off.Property_ElementSize - 0x10;
    i32 hi = off.Property_ElementSize + 0x10;
    if (lo < 0x30) lo = 0x30;

    for (i32 testOff = lo; testOff <= hi; testOff += 4)
    {
        if (testOff == off.Property_ElementSize)
        {
            continue;
        }
        i32 valA = 0, valC = 0;
        ReadValue(mem, propA + testOff, valA);
        ReadValue(mem, propC + testOff, valC);
        if (valA == 1 && valC == 1)
        {
            off.Property_ArrayDim = testOff;
            std::cerr << "[xrd] Property::ArrayDim +0x"
                      << std::hex << testOff << std::dec << "\n";
            return true;
        }
    }
    return false;
}

// 发现 Property::Offset_Internal 偏移
// Guid.A 的 Offset=0x00, Guid.B 的 Offset=0x04,
// Guid.C 的 Offset=0x08, Guid.D 的 Offset=0x0C
inline bool DiscoverPropertyOffsetInternalOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    uptr guidStruct = FindStructByName(mem, off, "Guid");
    if (!guidStruct) return false;

    uptr propA = FindPropertyInChain(mem, off, guidStruct, "A");
    uptr propB = FindPropertyInChain(mem, off, guidStruct, "B");
    uptr propC = FindPropertyInChain(mem, off, guidStruct, "C");
    if (!propA || !propB || !propC) return false;

    for (i32 testOff = 0x30; testOff <= 0x60; testOff += 4)
    {
        if (testOff == off.Property_ElementSize ||
            testOff == off.Property_ArrayDim)
        {
            continue;
        }
        i32 valA = 0, valB = 0, valC = 0;
        ReadValue(mem, propA + testOff, valA);
        ReadValue(mem, propB + testOff, valB);
        ReadValue(mem, propC + testOff, valC);
        // A=0x00, B=0x04, C=0x08
        if (valA == 0x00 && valB == 0x04 && valC == 0x08)
        {
            off.Property_Offset = testOff;
            std::cerr << "[xrd] Property::Offset_Internal +0x"
                      << std::hex << testOff << std::dec << "\n";
            return true;
        }
    }
    return false;
}

// 发现 Property::PropertyFlags 偏移
// Guid.A 的 Flags 包含 Edit|ZeroConstructor|SaveGame|IsPlainOldData 等
// 特征：非零的 u64，且在 ElementSize 和 Offset 之间或之后
inline bool DiscoverPropertyFlagsOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (off.Property_ElementSize == -1)
    {
        return false;
    }

    uptr guidStruct = FindStructByName(mem, off, "Guid");
    if (!guidStruct) return false;

    uptr propA = FindPropertyInChain(mem, off, guidStruct, "A");
    uptr propD = FindPropertyInChain(mem, off, guidStruct, "D");
    if (!propA || !propD) return false;

    // PropertyFlags 是 u64，在 Property 布局中
    // 通常在 ElementSize 之后、Offset_Internal 之前
    i32 searchStart = 0x38;
    i32 searchEnd = 0x60;

    for (i32 testOff = searchStart; testOff <= searchEnd; testOff += 8)
    {
        if (testOff == off.Property_ElementSize ||
            testOff == off.Property_ArrayDim ||
            testOff == off.Property_Offset)
        {
            continue;
        }
        u64 flagsA = 0, flagsD = 0;
        ReadValue(mem, propA + testOff, flagsA);
        ReadValue(mem, propD + testOff, flagsD);
        // Guid 成员的 flags 应该非零且相同
        if (flagsA != 0 && flagsA == flagsD)
        {
            off.Property_PropertyFlags = testOff;
            std::cerr << "[xrd] Property::PropertyFlags +0x"
                      << std::hex << testOff << std::dec << "\n";
            return true;
        }
    }
    return false;
}

} // namespace resolve
} // namespace xrd
