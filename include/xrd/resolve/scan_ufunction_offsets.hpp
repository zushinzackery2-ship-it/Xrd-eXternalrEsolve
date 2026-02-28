#pragma once
// Xrd-eXternalrEsolve - UFunction 偏移发现
// 自动扫描 UFunction::FunctionFlags 和 UFunction::ExecFunction
// 参考 Rei-Dumper: OffsetFinder::FindFunctionFlagsOffset / FindFunctionNativeFuncOffset

#include "../core/context.hpp"
#include "../engine/names.hpp"
#include "scan_offsets.hpp"
#include <iostream>
#include <algorithm>

namespace xrd
{
namespace resolve
{

// 发现 UFunction::FunctionFlags 偏移
// 策略：找到 UFunction 对象，FunctionFlags 是一个 u32，
// 常见值包含 FUNC_Native(0x400)、FUNC_Final(0x01) 等
inline bool DiscoverFunctionFlagsOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (off.UStruct_Size == -1)
    {
        return false;
    }

    i32 total = GetObjectCount(mem, off);
    std::vector<uptr> funcObjects;

    // 收集 UFunction 对象
    for (i32 i = 0; i < std::min(total, 5000); ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

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
        if (cn == "Function")
        {
            funcObjects.push_back(obj);
            if (funcObjects.size() >= 30)
            {
                break;
            }
        }
    }

    if (funcObjects.size() < 5)
    {
        std::cerr << "[xrd] UFunction 对象太少\n";
        return false;
    }

    // FunctionFlags 在 UStruct 之后的范围搜索
    // UStruct 在 Size 之后还有 MinAlignment、Script 等字段
    // FunctionFlags 特征：u32，多数函数包含 FUNC_Final(0x01)
    // 且至少有一些函数包含 FUNC_Native(0x400)
    // 搜索范围扩大到 UStruct::Size + 0x80
    i32 searchStart = off.UStruct_Size + 4;
    i32 bestOff = -1;
    int bestScore = 0;

    for (i32 testOff = searchStart;
         testOff <= searchStart + 0x80; testOff += 4)
    {
        int validCount = 0;
        int nativeCount = 0;
        int publicCount = 0;
        for (auto func : funcObjects)
        {
            u32 flags = 0;
            ReadValue(mem, func + testOff, flags);
            // FunctionFlags 通常非零，低位有常见标志
            if (flags != 0 && (flags & 0xFFFF) != 0)
            {
                validCount++;
            }
            // FUNC_Native = 0x400
            if (flags & 0x400)
            {
                nativeCount++;
            }
            // FUNC_Public = 0x20000
            if (flags & 0x20000)
            {
                publicCount++;
            }
        }
        // 需要大多数非零，且至少有一些 Native 或 Public 函数
        int score = validCount * 3 + nativeCount * 5
            + publicCount * 2;
        if (validCount > (int)funcObjects.size() * 70 / 100
            && (nativeCount > 0 || publicCount > 0)
            && score > bestScore)
        {
            bestScore = score;
            bestOff = testOff;
        }
    }

    if (bestOff != -1)
    {
        off.UFunction_FunctionFlags = bestOff;
        std::cerr << "[xrd] UFunction::FunctionFlags +0x"
                  << std::hex << bestOff << std::dec << "\n";
    }

    return off.UFunction_FunctionFlags != -1;
}

// 发现 UFunction::ExecFunction 偏移
// 策略：在 FunctionFlags 之后找一个合法的函数指针
inline bool DiscoverExecFunctionOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (off.UFunction_FunctionFlags == -1)
    {
        return false;
    }

    i32 total = GetObjectCount(mem, off);
    std::vector<uptr> nativeFuncs;

    // 收集带 FUNC_Native(0x400) 标志的函数
    for (i32 i = 0; i < std::min(total, 5000); ++i)
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

        FName clsFn{};
        if (!ReadValue(mem, cls + off.UObject_Name, clsFn))
        {
            continue;
        }
        std::string cn = ResolveNameDirect(
            mem, off, clsFn.ComparisonIndex, clsFn.Number);
        if (cn != "Function")
        {
            continue;
        }

        u32 flags = 0;
        ReadValue(mem, obj + off.UFunction_FunctionFlags, flags);
        // FUNC_Native = 0x400
        if (flags & 0x400)
        {
            nativeFuncs.push_back(obj);
            if (nativeFuncs.size() >= 20)
            {
                break;
            }
        }
    }

    if (nativeFuncs.size() < 3)
    {
        return false;
    }

    // ExecFunction 是一个函数指针，在 FunctionFlags 之后
    i32 searchStart = off.UFunction_FunctionFlags + 4;
    // 对齐到 8 字节
    searchStart = (searchStart + 7) & ~7;

    for (i32 testOff = searchStart;
         testOff <= searchStart + 0x20; testOff += 8)
    {
        int validCount = 0;
        for (auto func : nativeFuncs)
        {
            uptr execPtr = 0;
            if (ReadPtr(mem, func + testOff, execPtr) &&
                IsCanonicalUserPtr(execPtr))
            {
                validCount++;
            }
        }
        if (validCount > (int)nativeFuncs.size() * 70 / 100)
        {
            off.UFunction_ExecFunction = testOff;
            std::cerr << "[xrd] UFunction::ExecFunction +0x"
                      << std::hex << testOff << std::dec << "\n";
            return true;
        }
    }

    return false;
}

} // namespace resolve
} // namespace xrd
