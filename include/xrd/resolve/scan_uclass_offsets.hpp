#pragma once
// Xrd-eXternalrEsolve - UClass 偏移发现
// 自动扫描 UClass::CastFlags, ClassDefaultObject, ImplementedInterfaces
// 参考 Rei-Dumper: OffsetFinder

#include "../core/context.hpp"
#include "../engine/names.hpp"
#include "scan_offsets.hpp"
#include <iostream>
#include <algorithm>

namespace xrd
{
namespace resolve
{

// 发现 UClass::CastFlags 偏移
// CastFlags 是 u64，用于快速类型判断
// 特征：大多数 UClass 的 CastFlags 非零
inline bool DiscoverCastFlagsOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (off.UStruct_Size == -1)
    {
        return false;
    }

    i32 total = GetObjectCount(mem, off);
    std::vector<uptr> classObjects;

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
        // 元类的 Class 指向自身
        uptr metaCls = 0;
        if (ReadPtr(mem, cls + off.UObject_Class, metaCls) &&
            metaCls == cls)
        {
            classObjects.push_back(obj);
            if (classObjects.size() >= 30)
            {
                break;
            }
        }
    }

    if (classObjects.size() < 5)
    {
        return false;
    }

    // CastFlags 在 UStruct::Size 之后的范围搜索
    // 通常是 u64，且多数 UClass 有非零值
    i32 searchStart = off.UStruct_Size + 4;
    // 对齐到 8
    searchStart = (searchStart + 7) & ~7;

    for (i32 testOff = searchStart;
         testOff <= searchStart + 0x40; testOff += 8)
    {
        int nonZeroCount = 0;
        int zeroCount = 0;
        for (auto cls : classObjects)
        {
            u64 flags = 0;
            ReadValue(mem, cls + testOff, flags);
            if (flags != 0)
            {
                nonZeroCount++;
            }
            else
            {
                zeroCount++;
            }
        }
        // 大多数 UClass 有 CastFlags，但也有少数为 0
        if (nonZeroCount > (int)classObjects.size() * 40 / 100 &&
            zeroCount > 0)
        {
            off.UClass_CastFlags = testOff;
            std::cerr << "[xrd] UClass::CastFlags +0x"
                      << std::hex << testOff << std::dec << "\n";
            break;
        }
    }

    return off.UClass_CastFlags != -1;
}

// 发现 UClass::ClassDefaultObject 偏移
// CDO 是一个指向该类默认实例的指针
// 特征：指向一个合法 UObject，且该对象的 Class 指回当前 UClass
inline bool DiscoverClassDefaultObjectOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (off.UClass_CastFlags == -1)
    {
        return false;
    }

    i32 total = GetObjectCount(mem, off);
    std::vector<uptr> classObjects;

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
        if (ReadPtr(mem, cls + off.UObject_Class, metaCls) &&
            metaCls == cls)
        {
            classObjects.push_back(obj);
            if (classObjects.size() >= 30)
            {
                break;
            }
        }
    }

    if (classObjects.size() < 5)
    {
        return false;
    }

    // CDO 在 CastFlags 之后搜索
    i32 searchStart = off.UClass_CastFlags + 8;
    for (i32 testOff = searchStart;
         testOff <= searchStart + 0x40; testOff += 8)
    {
        int validCount = 0;
        for (auto cls : classObjects)
        {
            uptr cdoPtr = 0;
            if (!ReadPtr(mem, cls + testOff, cdoPtr))
            {
                continue;
            }
            if (!IsCanonicalUserPtr(cdoPtr))
            {
                continue;
            }
            // CDO 的 Class 应该指回当前 UClass
            uptr cdoClass = 0;
            if (ReadPtr(mem, cdoPtr + off.UObject_Class, cdoClass) &&
                cdoClass == cls)
            {
                validCount++;
            }
        }
        if (validCount > (int)classObjects.size() * 50 / 100)
        {
            off.UClass_ClassDefaultObject = testOff;
            std::cerr << "[xrd] UClass::ClassDefaultObject +0x"
                      << std::hex << testOff << std::dec << "\n";
            return true;
        }
    }

    return false;
}

} // namespace resolve
} // namespace xrd
