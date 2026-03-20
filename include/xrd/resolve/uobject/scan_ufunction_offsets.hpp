#pragma once
// Xrd-eXternalrEsolve - UFunction 偏移发现
// 自动扫描 UFunction::FunctionFlags 和 UFunction::ExecFunction
// 参考 Rei-Dumper: OffsetFinder::FindFunctionFlagsOffset / FindFunctionNativeFuncOffset

#include "../../core/context.hpp"
#include "../../core/process_sections.hpp"
#include "../../engine/names.hpp"
#include "../../helpers/dump/dump_function_flags.hpp"
#include "scan_offsets.hpp"
#include <iostream>
#include <algorithm>

namespace xrd
{
namespace resolve
{

struct NamedFunctionResolveTargets
{
    uptr wasInputKeyJustPressed = 0;
    uptr toggleSpeaking = 0;
    uptr switchLevelOrFov = 0;

    bool IsComplete() const
    {
        return wasInputKeyJustPressed != 0
            && toggleSpeaking != 0
            && switchLevelOrFov != 0;
    }
};

inline uptr FindFunctionObjectByShortNameForResolve(
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    const std::string& functionName)
{
    i32 total = GetObjectCount(mem, off);
    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        uptr cls = 0;
        if (!ReadPtr(mem, obj + off.UObject_Class, cls) || !IsCanonicalUserPtr(cls))
        {
            continue;
        }

        FName clsFn{};
        if (!ReadValue(mem, cls + off.UObject_Name, clsFn))
        {
            continue;
        }

        std::string className = ResolveNameDirect(mem, off, clsFn.ComparisonIndex, clsFn.Number);
        if (className != "Function")
        {
            continue;
        }

        FName fn{};
        if (!ReadValue(mem, obj + off.UObject_Name, fn))
        {
            continue;
        }

        std::string name = ResolveNameDirect(mem, off, fn.ComparisonIndex, fn.Number);
        if (name == functionName)
        {
            return obj;
        }
    }

    return 0;
}

inline NamedFunctionResolveTargets FindNamedFunctionObjectsForResolve(
    const IMemoryAccessor& mem,
    const UEOffsets& off)
{
    NamedFunctionResolveTargets targets;
    i32 total = GetObjectCount(mem, off);

    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        uptr cls = 0;
        if (!ReadPtr(mem, obj + off.UObject_Class, cls) || !IsCanonicalUserPtr(cls))
        {
            continue;
        }

        FName clsFn{};
        if (!ReadValue(mem, cls + off.UObject_Name, clsFn))
        {
            continue;
        }

        std::string className = ResolveNameDirect(mem, off, clsFn.ComparisonIndex, clsFn.Number);
        if (className != "Function")
        {
            continue;
        }

        FName fn{};
        if (!ReadValue(mem, obj + off.UObject_Name, fn))
        {
            continue;
        }

        std::string name = ResolveNameDirect(mem, off, fn.ComparisonIndex, fn.Number);
        if (name == "WasInputKeyJustPressed")
        {
            targets.wasInputKeyJustPressed = obj;
        }
        else if (name == "ToggleSpeaking")
        {
            targets.toggleSpeaking = obj;
        }
        else if (name == "SwitchLevel" || name == "FOV")
        {
            targets.switchLevelOrFov = obj;
        }

        if (targets.IsComplete())
        {
            break;
        }
    }

    return targets;
}

inline i32 FindExactU32Offset(
    const IMemoryAccessor& mem,
    const std::vector<std::pair<uptr, u32>>& infos,
    i32 searchStart,
    i32 searchEnd)
{
    for (i32 testOff = searchStart; testOff <= searchEnd; testOff += 4)
    {
        bool matched = true;
        for (const auto& info : infos)
        {
            u32 value = 0;
            if (!ReadValue(mem, info.first + testOff, value) || value != info.second)
            {
                matched = false;
                break;
            }
        }

        if (matched)
        {
            return testOff;
        }
    }

    return -1;
}

inline bool DiscoverFunctionFlagsOffsetExact(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    using detail::EFuncFlags::BlueprintCallable;
    using detail::EFuncFlags::BlueprintPure;
    using detail::EFuncFlags::Const;
    using detail::EFuncFlags::Exec;
    using detail::EFuncFlags::Final;
    using detail::EFuncFlags::Native;
    using detail::EFuncFlags::Public;
    using detail::EFuncFlags::RequiredAPI;

    NamedFunctionResolveTargets targets = FindNamedFunctionObjectsForResolve(mem, off);
    uptr wasInputKeyJustPressed = targets.wasInputKeyJustPressed;
    uptr toggleSpeaking = targets.toggleSpeaking;
    uptr switchLevelOrFov = targets.switchLevelOrFov;

    if (!wasInputKeyJustPressed || !toggleSpeaking || !switchLevelOrFov)
    {
        return false;
    }

    std::vector<std::pair<uptr, u32>> infos = {
        { wasInputKeyJustPressed, Final | Native | Public | BlueprintCallable | BlueprintPure | Const },
        { toggleSpeaking, Exec | Native | Public },
        { switchLevelOrFov, Exec | Native | Public },
    };

    i32 searchStart = std::max(off.UStruct_Size + 4, 0x30);
    i32 searchEnd = 0x140;

    i32 found = FindExactU32Offset(mem, infos, searchStart, searchEnd);
    if (found != -1)
    {
        off.UFunction_FunctionFlags = found;
        std::cerr << "[xrd] UFunction::FunctionFlags +0x"
                  << std::hex << found << std::dec
                  << " (exact)\n";
        return true;
    }

    for (auto& info : infos)
    {
        info.second |= RequiredAPI;
    }

    found = FindExactU32Offset(mem, infos, searchStart, searchEnd);
    if (found != -1)
    {
        off.UFunction_FunctionFlags = found;
        std::cerr << "[xrd] UFunction::FunctionFlags +0x"
                  << std::hex << found << std::dec
                  << " (exact|requiredapi)\n";
        return true;
    }

    return false;
}

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

    if (DiscoverFunctionFlagsOffsetExact(mem, off))
    {
        return true;
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
                  << std::hex << bestOff << std::dec
                  << " (heuristic)\n";
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

    NamedFunctionResolveTargets targets = FindNamedFunctionObjectsForResolve(mem, off);
    uptr wasInputKeyJustPressed = targets.wasInputKeyJustPressed;
    uptr toggleSpeaking = targets.toggleSpeaking;
    uptr switchLevelOrFov = targets.switchLevelOrFov;

    const SectionCache* textSec = FindSection(Ctx().sections, ".text");
    auto IsLikelyExecPtr = [textSec](uptr ptr) -> bool
    {
        if (!IsCanonicalUserPtr(ptr))
        {
            return false;
        }

        if (textSec == nullptr)
        {
            return true;
        }

        return ptr >= textSec->va && ptr < textSec->va + textSec->size;
    };

    if (wasInputKeyJustPressed && toggleSpeaking && switchLevelOrFov)
    {
        for (i32 testOff = 0x30; testOff <= 0x140; testOff += static_cast<i32>(sizeof(uptr)))
        {
            uptr ptr0 = 0;
            uptr ptr1 = 0;
            uptr ptr2 = 0;

            if (!ReadPtr(mem, wasInputKeyJustPressed + testOff, ptr0)
                || !ReadPtr(mem, toggleSpeaking + testOff, ptr1)
                || !ReadPtr(mem, switchLevelOrFov + testOff, ptr2))
            {
                continue;
            }

            if (!IsLikelyExecPtr(ptr0)
                || !IsLikelyExecPtr(ptr1)
                || !IsLikelyExecPtr(ptr2))
            {
                continue;
            }

            off.UFunction_ExecFunction = testOff;
            std::cerr << "[xrd] UFunction::ExecFunction +0x"
                      << std::hex << testOff << std::dec
                      << " (exact/text)\n";
            return true;
        }
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
                IsLikelyExecPtr(execPtr))
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
