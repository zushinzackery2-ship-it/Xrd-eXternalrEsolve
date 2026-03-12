#pragma once
// Xrd-eXternalrEsolve - 签名/字符串扫描辅助

#include "../../core/context.hpp"
#include "../../core/process_sections.hpp"
#include <cstdlib>
#include <cstring>

namespace xrd
{
namespace resolve
{

inline uptr FindStringInSections(
    const std::vector<SectionCache>& sections,
    const char* str)
{
    const std::size_t len = std::strlen(str);
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

inline i32 MatchSignature(
    const u8* data,
    u32 dataSize,
    const char* pattern,
    u32 startOffset = 0)
{
    u8 bytes[64]{};
    bool mask[64]{};
    u32 patLen = 0;

    for (const char* p = pattern; *p && patLen < 64;)
    {
        while (*p == ' ')
        {
            ++p;
        }
        if (!*p)
        {
            break;
        }

        if (*p == '?')
        {
            bytes[patLen] = 0;
            mask[patLen] = false;
            ++patLen;
            ++p;
            if (*p == '?')
            {
                ++p;
            }
            continue;
        }

        char hex[3] = { p[0], p[1], 0 };
        bytes[patLen] = static_cast<u8>(std::strtoul(hex, nullptr, 16));
        mask[patLen] = true;
        ++patLen;
        p += 2;
    }

    if (patLen == 0)
    {
        return -1;
    }

    for (u32 i = startOffset; i + patLen <= dataSize; ++i)
    {
        bool matched = true;
        for (u32 j = 0; j < patLen; ++j)
        {
            if (mask[j] && data[i + j] != bytes[j])
            {
                matched = false;
                break;
            }
        }
        if (matched)
        {
            return static_cast<i32>(i);
        }
    }
    return -1;
}

inline uptr ResolveE8Call(
    const u8* data,
    u32 e8Offset,
    uptr sectionVa)
{
    const i32 disp = *reinterpret_cast<const i32*>(&data[e8Offset + 1]);
    return sectionVa + e8Offset + 5 + disp;
}

inline std::size_t GetSignatureLength(const char* pattern)
{
    std::size_t length = 0;
    for (const char* p = pattern; *p;)
    {
        while (*p == ' ')
        {
            ++p;
        }
        if (!*p)
        {
            break;
        }

        ++length;
        if (*p == '?')
        {
            ++p;
            if (*p == '?')
            {
                ++p;
            }
        }
        else
        {
            p += 2;
        }
    }
    return length;
}

inline const SectionCache* FindSectionByVa(
    const std::vector<SectionCache>& sections,
    uptr address)
{
    for (const auto& sec : sections)
    {
        if (address >= sec.va && address < sec.va + sec.size)
        {
            return &sec;
        }
    }
    return nullptr;
}

template<typename CharType>
inline bool MatchStringAtVaInSections(
    const std::vector<SectionCache>& sections,
    uptr address,
    const CharType* str)
{
    if (str == nullptr || str[0] == CharType(0))
    {
        return false;
    }

    const SectionCache* sec = FindSectionByVa(sections, address);
    if (sec == nullptr)
    {
        return false;
    }

    std::size_t len = 0;
    while (str[len] != CharType(0))
    {
        ++len;
    }

    const std::size_t byteCount = (len + 1) * sizeof(CharType);
    const u32 sectionOffset = static_cast<u32>(address - sec->va);
    if (sectionOffset + byteCount > sec->size)
    {
        return false;
    }

    return std::memcmp(sec->data.data() + sectionOffset, str, byteCount) == 0;
}

inline bool ReadPointerFromCachedSections(
    const std::vector<SectionCache>& sections,
    uptr address,
    uptr& outValue)
{
    const SectionCache* sec = FindSectionByVa(sections, address);
    if (sec == nullptr)
    {
        return false;
    }

    const u32 sectionOffset = static_cast<u32>(address - sec->va);
    if (sectionOffset + sizeof(uptr) > sec->size)
    {
        return false;
    }

    std::memcpy(&outValue, sec->data.data() + sectionOffset, sizeof(uptr));
    return true;
}

inline bool IsAddressInCachedSections(
    const std::vector<SectionCache>& sections,
    uptr address)
{
    return FindSectionByVa(sections, address) != nullptr;
}

inline bool IsLikelyCodeAddress(
    const std::vector<SectionCache>& sections,
    uptr address)
{
    const SectionCache* textSec = FindSection(sections, ".text");
    if (textSec != nullptr)
    {
        return address >= textSec->va && address < textSec->va + textSec->size;
    }
    return IsAddressInCachedSections(sections, address);
}

template<typename CharType, bool bCheckIfLeaIsStrPtr = false>
inline uptr FindStringRefInAllSections(
    const std::vector<SectionCache>& sections,
    const CharType* refStr)
{
    for (const auto& sec : sections)
    {
        if (sec.data.empty())
        {
            continue;
        }

        for (u32 i = 0; i + 7 <= sec.size; ++i)
        {
            const u8 rex = sec.data[i];
            if ((rex != 0x48 && rex != 0x4C) || sec.data[i + 1] != 0x8D)
            {
                continue;
            }

            const u8 modrm = sec.data[i + 2];
            if ((modrm & 0xC7) != 0x05)
            {
                continue;
            }

            const i32 disp = *reinterpret_cast<const i32*>(&sec.data[i + 3]);
            const uptr resolved = sec.va + i + 7 + disp;
            if (MatchStringAtVaInSections(sections, resolved, refStr))
            {
                return sec.va + i;
            }

            if constexpr (bCheckIfLeaIsStrPtr)
            {
                uptr indirectPtr = 0;
                if (ReadPointerFromCachedSections(sections, resolved, indirectPtr)
                    && MatchStringAtVaInSections(sections, indirectPtr, refStr))
                {
                    return sec.va + i;
                }
            }
        }
    }
    return 0;
}

inline uptr FindPatternInRange(
    const std::vector<SectionCache>& sections,
    const char* pattern,
    uptr startVa,
    u32 range,
    bool resolveRelative = false,
    i32 relativeOffset = 0)
{
    const SectionCache* sec = FindSectionByVa(sections, startVa);
    if (sec == nullptr || sec->data.empty())
    {
        return 0;
    }

    const u32 startOffset = static_cast<u32>(startVa - sec->va);
    u32 endOffset = startOffset + range;
    if (endOffset > sec->size)
    {
        endOffset = sec->size;
    }

    const i32 matchOffset = MatchSignature(
        sec->data.data(), endOffset, pattern, startOffset);
    if (matchOffset < 0)
    {
        return 0;
    }

    if (!resolveRelative)
    {
        return sec->va + static_cast<u32>(matchOffset);
    }

    if (relativeOffset < 0)
    {
        relativeOffset = static_cast<i32>(GetSignatureLength(pattern));
    }

    const u32 dispOffset = static_cast<u32>(matchOffset + relativeOffset);
    if (dispOffset + sizeof(i32) > sec->size)
    {
        return 0;
    }

    const i32 disp = *reinterpret_cast<const i32*>(&sec->data[dispOffset]);
    return sec->va + dispOffset + 4 + disp;
}

} // namespace resolve
} // namespace xrd
