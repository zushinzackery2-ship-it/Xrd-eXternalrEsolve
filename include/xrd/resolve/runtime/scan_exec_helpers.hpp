#pragma once
// Xrd-eXternalrEsolve - Exec 函数扫描辅助

#include "scan_signature_helpers.hpp"
#include <iostream>

namespace xrd
{
namespace resolve
{

inline bool IsLikelyUFunctionSetupExec(
    const std::vector<SectionCache>& sections,
    uptr functionVa)
{
    return FindPatternInRange(
        sections,
        "48 8B 05 ? ? ? ? 48 85 C0 75 ? 48 8D 15",
        functionVa,
        0x28) != 0;
}

inline bool IsValidExecFunctionCandidate(
    const std::vector<SectionCache>& sections,
    uptr functionVa)
{
    if (!IsLikelyCodeAddress(sections, functionVa))
    {
        return false;
    }

    const SectionCache* sec = FindSectionByVa(sections, functionVa);
    if (sec == nullptr)
    {
        return false;
    }

    const u32 offset = static_cast<u32>(functionVa - sec->va);
    if (offset + 4 > sec->size)
    {
        return false;
    }

    const u8* code = sec->data.data() + offset;
    if (code[0] == 0x48 && code[1] == 0x83 && code[2] == 0xEC && code[3] == 0x28)
    {
        return false;
    }

    return !IsLikelyUFunctionSetupExec(sections, functionVa);
}

template<typename CharType>
inline uptr FindExecFunctionByString(
    const std::vector<SectionCache>& sections,
    const CharType* refStr)
{
    for (const auto& sec : sections)
    {
        if (sec.data.size() < sizeof(uptr) * 2)
        {
            continue;
        }

        for (u32 offset = 0; offset + sizeof(uptr) * 2 <= sec.size; offset += sizeof(uptr))
        {
            uptr possibleStringAddress = 0;
            uptr possibleExecAddress = 0;
            std::memcpy(&possibleStringAddress, sec.data.data() + offset, sizeof(uptr));
            std::memcpy(
                &possibleExecAddress,
                sec.data.data() + offset + sizeof(uptr),
                sizeof(uptr));

            if (possibleStringAddress == possibleExecAddress
                || !IsCanonicalUserPtr(possibleStringAddress)
                || !IsCanonicalUserPtr(possibleExecAddress))
            {
                continue;
            }

            if (!MatchStringAtVaInSections(sections, possibleStringAddress, refStr))
            {
                continue;
            }

            if (IsValidExecFunctionCandidate(sections, possibleExecAddress))
            {
                return possibleExecAddress;
            }
        }
    }

    return 0;
}

inline bool StoreAppendStringResult(
    const std::vector<SectionCache>& sections,
    uptr targetVa,
    UEOffsets& off,
    const char* sourceTag)
{
    if (!targetVa || !IsLikelyCodeAddress(sections, targetVa))
    {
        return false;
    }

    off.AppendNameToString = targetVa - Ctx().mainModule.base;
    std::cerr << "[xrd] AppendString " << sourceTag
              << "RVA=0x" << std::hex << off.AppendNameToString
              << std::dec << "\n";
    return true;
}

} // namespace resolve
} // namespace xrd
