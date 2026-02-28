#pragma once
// Xrd-eXternalrEsolve - ProcessEvent 扫描器
// 通过 VTable 模式匹配定位 UObject::ProcessEvent
// 参考 Rei-Dumper: Off::InSDK::ProcessEvent::InitPE_Windows
// 外部读取模式：读取 VTable 函数指针，在 .text 段中搜索特征码

#include "../core/context.hpp"
#include "scan_offsets.hpp"
#include <iostream>
#include <algorithm>

namespace xrd
{
namespace resolve
{

// 在缓存的 .text 段数据中搜索字节模式
// pattern 中 -1 表示通配符
inline bool FindPatternInCachedSection(
    const SectionCache& sec,
    const std::vector<i32>& pattern,
    uptr searchStart,
    u32 searchLen)
{
    if (pattern.empty() || sec.data.empty())
    {
        return false;
    }

    // 将 searchStart 转换为段内偏移
    if (searchStart < sec.va)
    {
        return false;
    }
    u32 startOff = static_cast<u32>(searchStart - sec.va);
    u32 endOff = startOff + searchLen;
    if (endOff > sec.size)
    {
        endOff = sec.size;
    }

    u32 patLen = static_cast<u32>(pattern.size());
    if (patLen == 0 || endOff < patLen)
    {
        return false;
    }

    for (u32 i = startOff; i + patLen <= endOff; ++i)
    {
        bool match = true;
        for (u32 j = 0; j < patLen; ++j)
        {
            if (pattern[j] == -1)
            {
                continue;
            }
            if (sec.data[i + j] != static_cast<u8>(pattern[j]))
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            return true;
        }
    }
    return false;
}

// 通过 VTable 扫描定位 ProcessEvent
// 策略：读取第一个 UObject 的 VTable，遍历虚函数指针
// 对每个函数检查是否包含 FunctionFlags 偏移的特征码
inline bool ScanProcessEvent(
    const std::vector<SectionCache>& sections,
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (off.UFunction_FunctionFlags == -1)
    {
        std::cerr << "[xrd] FunctionFlags 未知，跳过 ProcessEvent 扫描\n";
        return false;
    }

    // 读取第一个对象的 VTable
    uptr firstObj = ReadObjectAt(mem, off, 0);
    if (!IsCanonicalUserPtr(firstObj))
    {
        return false;
    }

    uptr vftPtr = 0;
    if (!ReadPtr(mem, firstObj, vftPtr) || !IsCanonicalUserPtr(vftPtr))
    {
        return false;
    }

    // 找到 .text 段
    const SectionCache* textSec = FindSection(sections, ".text");
    if (!textSec)
    {
        return false;
    }

    // FunctionFlags 偏移的低字节
    u8 ffLo = static_cast<u8>(off.UFunction_FunctionFlags & 0xFF);
    u8 ffHi = static_cast<u8>((off.UFunction_FunctionFlags >> 8) & 0xFF);

    // ProcessEvent 特征：
    // test [reg+FunctionFlags], 0x00000400 (FUNC_Net)
    // 和 test [reg+FunctionFlags], 0x00400000 (FUNC_HasOutParms)
    // 对应字节: F7 xx <ffLo> <ffHi> 00 00 00 04 00 00
    std::vector<i32> pattern1 = {
        0xF7, -1, ffLo, ffHi, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00
    };
    std::vector<i32> pattern2 = {
        0xF7, -1, ffLo, ffHi, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00
    };

    // 遍历 VTable 中的函数指针（最多检查 200 个）
    for (i32 idx = 0; idx < 200; ++idx)
    {
        uptr funcAddr = 0;
        if (!ReadPtr(mem, vftPtr + idx * sizeof(uptr), funcAddr))
        {
            break;
        }
        if (!IsCanonicalUserPtr(funcAddr))
        {
            break;
        }

        // 检查函数是否在 .text 段范围内
        if (funcAddr < textSec->va ||
            funcAddr >= textSec->va + textSec->size)
        {
            continue;
        }

        // 在函数体内搜索两个特征码
        bool found1 = FindPatternInCachedSection(
            *textSec, pattern1, funcAddr, 0x400);
        bool found2 = FindPatternInCachedSection(
            *textSec, pattern2, funcAddr, 0xF00);

        if (found1 && found2)
        {
            off.ProcessEvent_VTableIndex = idx;
            // 计算 RVA
            uptr moduleBase = Ctx().mainModule.base;
            off.ProcessEvent_Addr = funcAddr - moduleBase;

            std::cerr << "[xrd] ProcessEvent 找到: VTable["
                      << idx << "] RVA=0x"
                      << std::hex << off.ProcessEvent_Addr
                      << std::dec << "\n";
            return true;
        }
    }

    std::cerr << "[xrd] ProcessEvent 未找到\n";
    return false;
}

} // namespace resolve
} // namespace xrd
