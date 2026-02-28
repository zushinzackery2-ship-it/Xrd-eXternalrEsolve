#pragma once
// Xrd-eXternalrEsolve - AppendString 扫描器
// 对标 Rei-Dumper: 通过字符串 "ForwardShadingQuality_" 的引用定位 AppendString
// 在字符串引用附近搜索 CALL 指令，解析相对调用目标

#include "../core/context.hpp"
#include <iostream>
#include <cstring>

namespace xrd
{
namespace resolve
{

// 在已缓存的段数据中搜索 ASCII 字符串，返回其 VA
inline uptr FindStringInSections(
    const std::vector<SectionCache>& sections,
    const char* str)
{
    std::size_t len = std::strlen(str);

    for (const auto& sec : sections)
    {
        if (sec.data.empty() || sec.size < len)
        {
            continue;
        }
        for (u32 i = 0; i + len <= sec.size; ++i)
        {
            if (std::memcmp(&sec.data[i], str, len) == 0)
            {
                return sec.va + i;
            }
        }
    }
    return 0;
}

// 在 .text 段中搜索引用指定 VA 的 LEA 指令（RIP 相对寻址）
// 返回 LEA 指令的偏移（相对于 .text 段起始）
inline i32 FindLeaXrefInText(
    const SectionCache& textSec,
    uptr targetVa)
{
    for (u32 i = 0; i + 7 <= textSec.size; ++i)
    {
        // REX.W LEA: 48 8D xx [disp32] 或 4C 8D xx [disp32]
        u8 rex = textSec.data[i];
        if (rex != 0x48 && rex != 0x4C)
        {
            continue;
        }
        if (textSec.data[i + 1] != 0x8D)
        {
            continue;
        }

        u8 modrm = textSec.data[i + 2];
        // mod=00, rm=101 -> RIP 相对
        if ((modrm & 0xC7) != 0x05)
        {
            continue;
        }

        i32 disp = *reinterpret_cast<const i32*>(&textSec.data[i + 3]);
        uptr instrVa = textSec.va + i;
        uptr resolved = instrVa + 7 + disp;

        if (resolved == targetVa)
        {
            return static_cast<i32>(i);
        }
    }
    return -1;
}

// 简单的签名匹配器：空格分隔的十六进制字节，? 为通配符
// 返回匹配到的偏移（相对于 data），-1 表示未找到
inline i32 MatchSignature(
    const u8* data, u32 dataSize,
    const char* pattern,
    u32 startOffset = 0)
{
    // 解析签名为字节和掩码
    u8 bytes[64]{};
    bool mask[64]{};
    u32 patLen = 0;

    const char* p = pattern;
    while (*p && patLen < 64)
    {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (*p == '?')
        {
            bytes[patLen] = 0;
            mask[patLen] = false;
            ++patLen;
            ++p;
        }
        else
        {
            char hex[3] = { p[0], p[1], 0 };
            bytes[patLen] = static_cast<u8>(std::strtoul(hex, nullptr, 16));
            mask[patLen] = true;
            ++patLen;
            p += 2;
        }
    }

    if (patLen == 0) return -1;

    for (u32 i = startOffset; i + patLen <= dataSize; ++i)
    {
        bool ok = true;
        for (u32 j = 0; j < patLen; ++j)
        {
            if (mask[j] && data[i + j] != bytes[j])
            {
                ok = false;
                break;
            }
        }
        if (ok) return static_cast<i32>(i);
    }
    return -1;
}

// 解析 E8 CALL 的目标地址
inline uptr ResolveE8Call(const u8* data, u32 e8Offset, uptr sectionVa)
{
    i32 disp = *reinterpret_cast<const i32*>(&data[e8Offset + 1]);
    uptr callVa = sectionVa + e8Offset;
    return callVa + 5 + disp;
}

// 对标 Rei-Dumper FName::Init_Windows
// 1. 在所有段中搜索字符串 "ForwardShadingQuality_"
// 2. 在 .text 中找到引用该字符串的 LEA 指令（即 StringRef）
// 3. 从 StringRef 开始用 Rei-Dumper 的 6 种签名匹配 E8 CALL
// 4. 解析 CALL 的相对目标地址即为 AppendString
inline bool ScanAppendString(
    const std::vector<SectionCache>& sections,
    [[maybe_unused]] const IMemoryAccessor& mem,
    UEOffsets& off)
{
    uptr moduleBase = Ctx().mainModule.base;

    const SectionCache* textSec = FindSection(sections, ".text");
    if (!textSec || textSec->data.empty())
    {
        return false;
    }

    // Step 1: 搜索 "ForwardShadingQuality_" 字符串（搜索所有段）
    uptr strVa = FindStringInSections(sections, "ForwardShadingQuality_");
    if (!strVa)
    {
        std::cerr << "[xrd] 未找到 ForwardShadingQuality_ 字符串\n";
        return false;
    }

    // Step 2: 在 .text 中找引用该字符串的 LEA（对标 FindByStringInAllSections）
    i32 leaOff = FindLeaXrefInText(*textSec, strVa);
    if (leaOff < 0)
    {
        std::cerr << "[xrd] 未找到 ForwardShadingQuality_ 的 xref\n";
        return false;
    }

    // Step 3: 从 LEA 位置开始搜索 Rei-Dumper 的 6 种签名
    // 对标 Rei-Dumper PossibleSigs (x64)
    // 所有签名末尾都是 E8（CALL），前面是参数设置指令
    const char* sigs[] = {
        "48 8D ? ? 48 8D ? ? E8",
        "48 8D ? ? ? 48 8D ? ? E8",
        "48 8D ? ? 49 8B ? E8",
        "48 8D ? ? ? 49 8B ? E8",
        "48 8D ? ? 48 8B ? E8",
        "48 8D ? ? ? 48 8B ? E8",
    };
    // 每个签名中 E8 的位置（从签名起始算的偏移）
    const u32 e8Positions[] = { 8, 9, 7, 8, 7, 8 };

    u32 searchStart = static_cast<u32>(leaOff);
    u32 searchEnd = searchStart + 0x50;
    if (searchEnd + 10 > textSec->size)
    {
        searchEnd = textSec->size - 10;
    }

    for (int s = 0; s < 6; ++s)
    {
        i32 matchOff = MatchSignature(
            textSec->data.data(), searchEnd + 10,
            sigs[s], searchStart);

        if (matchOff < 0) continue;

        // 检查匹配位置在搜索范围内
        if (static_cast<u32>(matchOff) > searchEnd) continue;

        u32 e8Off = static_cast<u32>(matchOff) + e8Positions[s];
        uptr targetVa = ResolveE8Call(textSec->data.data(), e8Off, textSec->va);

        // 验证目标在 .text 段内
        if (targetVa < textSec->va || targetVa >= textSec->va + textSec->size)
        {
            continue;
        }

        uptr rva = targetVa - moduleBase;
        off.AppendNameToString = rva;
        std::cerr << "[xrd] AppendString RVA=0x"
                  << std::hex << rva << std::dec << "\n";
        return true;
    }

    // 回退: 使用备用字符串 " Bone: " (Rei-Dumper TryFindApendStringBackupStringRef)
    const char* backupSigs[] = {
        "48 8B ? 48 8B ? ? E8",
        "48 8B ? ? 48 89 ? ? E8",
        "48 8B ? 48 89 ? ? ? E8",
    };
    const u32 backupE8[] = { 7, 8, 8 };

    uptr boneStrVa = FindStringInSections(sections, " Bone: ");
    if (boneStrVa)
    {
        i32 boneLeaOff = FindLeaXrefInText(*textSec, boneStrVa);
        if (boneLeaOff >= 0)
        {
            // 对标 Rei-Dumper: 向上搜索 0xB0 字节
            u32 boneSearchStart = (static_cast<u32>(boneLeaOff) > 0xB0)
                ? static_cast<u32>(boneLeaOff) - 0xB0 : 0;
            u32 boneSearchEnd = static_cast<u32>(boneLeaOff);

            for (int s = 0; s < 3; ++s)
            {
                i32 matchOff = MatchSignature(
                    textSec->data.data(), boneSearchEnd + 10,
                    backupSigs[s], boneSearchStart);

                if (matchOff < 0 || static_cast<u32>(matchOff) > boneSearchEnd)
                {
                    continue;
                }

                u32 e8Off = static_cast<u32>(matchOff) + backupE8[s];
                uptr targetVa = ResolveE8Call(textSec->data.data(), e8Off, textSec->va);

                if (targetVa >= textSec->va && targetVa < textSec->va + textSec->size)
                {
                    uptr rva = targetVa - moduleBase;
                    off.AppendNameToString = rva;
                    std::cerr << "[xrd] AppendString (backup) RVA=0x"
                              << std::hex << rva << std::dec << "\n";
                    return true;
                }
            }
        }
    }

    std::cerr << "[xrd] AppendString 未找到\n";
    return false;
}

} // namespace resolve
} // namespace xrd
