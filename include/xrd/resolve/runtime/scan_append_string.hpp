#pragma once
// Xrd-eXternalrEsolve - AppendString 扫描器
// 对齐 Rei-Dumper 的主路径、内联回退、备用字符串回退和最终兜底

#include "scan_runtime_common.hpp"
#include <array>
#include <iostream>

namespace xrd
{
namespace resolve
{

inline bool ScanAppendString(
    const std::vector<SectionCache>& sections,
    [[maybe_unused]] const IMemoryAccessor& mem,
    UEOffsets& off)
{
    constexpr std::array<const char*, 6> kPrimarySigs = {
        "48 8D ? ? 48 8D ? ? E8",
        "48 8D ? ? ? 48 8D ? ? E8",
        "48 8D ? ? 49 8B ? E8",
        "48 8D ? ? ? 49 8B ? E8",
        "48 8D ? ? 48 8B ? E8",
        "48 8D ? ? ? 48 8B ? E8",
    };
    constexpr const char* kInlineSig =
        "8B ? ? E8 ? ? ? ? 48 8D ? ? ? 48 8B C8 E8 ? ? ? ?";
    constexpr std::array<const char*, 3> kBackupSigs = {
        "48 8B ? 48 8B ? ? E8",
        "48 8B ? ? 48 89 ? ? E8",
        "48 8B ? 48 89 ? ? ? E8",
    };
    constexpr std::array<const char*, 3> kConvNameToStringSigs = {
        "89 44 ? ? 48 01 ? ? E8",
        "48 89 ? ? 48 8D ? ? ? E8",
        "48 89 ? ? ? 48 89 ? ? E8",
    };

    const uptr moduleBase = Ctx().mainModule.base;
    const uptr forwardStringRef = FindStringRefInAllSections<char>(
        sections,
        "ForwardShadingQuality_");

    uptr primaryAppendString = 0;
    bool bPrimaryMayBeInlineOverlap = false;

    if (forwardStringRef)
    {
        std::cerr << "[xrd] ForwardShadingQuality_ xref RVA=0x"
                  << std::hex << (forwardStringRef - moduleBase)
                  << std::dec << "\n";

        for (const char* sig : kPrimarySigs)
        {
            const uptr candidate = FindPatternInRange(
                sections,
                sig,
                forwardStringRef,
                0x50,
                true,
                -1);
            if (!candidate || !IsLikelyCodeAddress(sections, candidate))
            {
                continue;
            }

            primaryAppendString = candidate;
            bPrimaryMayBeInlineOverlap =
                std::strcmp(sig, "48 8D ? ? ? 48 8B ? E8") == 0;
            break;
        }

        if (!primaryAppendString)
        {
            std::cerr << "[xrd] ForwardShadingQuality_ 主签名未命中\n";
        }

        if (!primaryAppendString || bPrimaryMayBeInlineOverlap)
        {
            const uptr inlineMatchVa = FindPatternInRange(
                sections,
                kInlineSig,
                forwardStringRef,
                0x180);
            if (inlineMatchVa)
            {
                const SectionCache* sec = FindSectionByVa(sections, inlineMatchVa);
                if (sec != nullptr)
                {
                    const u32 matchOffset = static_cast<u32>(inlineMatchVa - sec->va);
                    const uptr appendStringVa = ResolveE8Call(
                        sec->data.data(),
                        matchOffset + 0x10,
                        sec->va);
                    if (StoreAppendStringResult(
                            sections,
                            appendStringVa,
                            off,
                            "(inlined) "))
                    {
                        return true;
                    }
                }
            }

            if (!primaryAppendString)
            {
                std::cerr << "[xrd] ForwardShadingQuality_ 内联回退未命中\n";
            }
        }

        if (StoreAppendStringResult(sections, primaryAppendString, off, ""))
        {
            return true;
        }
    }
    else
    {
        std::cerr << "[xrd] 未找到 ForwardShadingQuality_ 字符串引用\n";
    }

    uptr boneStringRef = FindStringRefInAllSections<wchar_t>(sections, L" Bone: ");
    if (!boneStringRef)
    {
        boneStringRef = FindStringRefInAllSections<char>(sections, " Bone: ");
    }

    if (boneStringRef)
    {
        const SectionCache* boneSection = FindSectionByVa(sections, boneStringRef);
        if (boneSection != nullptr)
        {
            uptr searchStart = boneStringRef > 0xB0 ? boneStringRef - 0xB0 : boneSection->va;
            if (searchStart < boneSection->va)
            {
                searchStart = boneSection->va;
            }

            for (const char* sig : kBackupSigs)
            {
                const uptr candidate = FindPatternInRange(
                    sections,
                    sig,
                    searchStart,
                    0x100,
                    true,
                    -1);
                if (StoreAppendStringResult(sections, candidate, off, "(backup) "))
                {
                    return true;
                }
            }
        }

        std::cerr << "[xrd] Bone 字符串回退未命中\n";
    }
    else
    {
        std::cerr << "[xrd] 未找到 Bone 字符串引用\n";
    }

    const uptr convNameToStringExec = FindExecFunctionByString<char>(
        sections,
        "Conv_NameToString");
    if (convNameToStringExec)
    {
        std::cerr << "[xrd] Conv_NameToString Exec RVA=0x"
                  << std::hex << (convNameToStringExec - moduleBase)
                  << std::dec << "\n";

        for (const char* sig : kConvNameToStringSigs)
        {
            const uptr candidate = FindPatternInRange(
                sections,
                sig,
                convNameToStringExec,
                0x90,
                true,
                -1);
            if (StoreAppendStringResult(
                    sections,
                    candidate,
                    off,
                    "(Conv_NameToString) "))
            {
                return true;
            }
        }

        std::cerr << "[xrd] Conv_NameToString 回退未命中\n";
    }
    else
    {
        std::cerr << "[xrd] 未找到 Conv_NameToString Exec\n";
    }

    std::cerr << "[xrd] AppendString 未找到\n";
    return false;
}

} // namespace resolve
} // namespace xrd
