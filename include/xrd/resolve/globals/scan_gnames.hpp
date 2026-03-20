#pragma once
// Xrd-eXternalrEsolve - GNames 扫描器
// 优先对标 Rei-Dumper 的定点定位，再回退到轻量 .data 验证

#include "../../core/context.hpp"
#include "../../engine/names.hpp"
#include "../runtime/scan_runtime_common.hpp"
#include "../uobject/scan_offsets.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <vector>

namespace xrd
{
namespace resolve
{

inline uptr ResolveRipRelativeTarget(
    const SectionCache& section,
    u32 instructionOffset,
    u32 displacementOffset,
    u32 instructionSize)
{
    if (instructionOffset + displacementOffset + sizeof(i32) > section.size)
    {
        return 0;
    }

    i32 displacement = *reinterpret_cast<const i32*>(
        &section.data[instructionOffset + displacementOffset]);
    uptr instructionVa = section.va + instructionOffset;
    return instructionVa + instructionSize + displacement;
}

inline u32 VaToSectionOffset(const SectionCache& section, uptr address)
{
    return static_cast<u32>(address - section.va);
}

inline bool ContainsStringInBuffer(
    const std::vector<char>& buffer,
    const char* needle)
{
    const std::size_t needleLength = std::strlen(needle);
    if (needleLength == 0 || buffer.size() < needleLength)
    {
        return false;
    }

    auto it = std::search(
        buffer.begin(),
        buffer.end(),
        needle,
        needle + needleLength);
    return it != buffer.end();
}

inline uptr FindWideStringInSections(
    const std::vector<SectionCache>& sections,
    const char* str)
{
    std::vector<u8> widePattern;
    for (const char* p = str; *p != '\0'; ++p)
    {
        widePattern.push_back(static_cast<u8>(*p));
        widePattern.push_back(0);
    }

    if (widePattern.empty())
    {
        return 0;
    }

    for (const auto& sec : sections)
    {
        if (sec.data.size() < widePattern.size())
        {
            continue;
        }

        for (u32 i = 0; i + widePattern.size() <= sec.data.size(); ++i)
        {
            if (std::memcmp(
                    &sec.data[i],
                    widePattern.data(),
                    widePattern.size()) == 0)
            {
                return sec.va + i;
            }
        }
    }

    return 0;
}

inline bool FindLeaXrefInRange(
    const SectionCache& textSection,
    uptr targetVa,
    uptr startVa,
    u32 searchLength)
{
    if (startVa < textSection.va || startVa >= textSection.va + textSection.size)
    {
        return false;
    }

    u32 startOffset = VaToSectionOffset(textSection, startVa);
    u32 endOffset = startOffset + searchLength;
    if (endOffset > textSection.size)
    {
        endOffset = textSection.size;
    }

    for (u32 i = startOffset; i + 7 <= endOffset; ++i)
    {
        u8 rex = textSection.data[i];
        if (rex != 0x48 && rex != 0x4C)
        {
            continue;
        }
        if (textSection.data[i + 1] != 0x8D)
        {
            continue;
        }

        u8 modrm = textSection.data[i + 2];
        if ((modrm & 0xC7) != 0x05)
        {
            continue;
        }

        uptr resolved = ResolveRipRelativeTarget(textSection, i, 3, 7);
        if (resolved == targetVa)
        {
            return true;
        }
    }

    return false;
}

inline bool IsLikelyInitSrwLockCall(
    const SectionCache& textSection,
    const IMemoryAccessor& mem,
    u32 callOffset)
{
    if (callOffset + 6 > textSection.size)
    {
        return false;
    }

    if (textSection.data[callOffset] != 0xFF || textSection.data[callOffset + 1] != 0x15)
    {
        return false;
    }

    uptr iatSlot = ResolveRipRelativeTarget(textSection, callOffset, 2, 6);
    if (!iatSlot)
    {
        return false;
    }

    uptr importedFunction = 0;
    if (!ReadPtr(mem, iatSlot, importedFunction) || !IsCanonicalUserPtr(importedFunction))
    {
        return false;
    }

    FARPROC initSrwLock = nullptr;
    FARPROC rtlInitSrwLock = nullptr;

    HMODULE kernel32Module = GetModuleHandleW(L"kernel32.dll");
    if (kernel32Module != nullptr)
    {
        initSrwLock = GetProcAddress(kernel32Module, "InitializeSRWLock");
    }

    HMODULE ntdllModule = GetModuleHandleW(L"ntdll.dll");
    if (ntdllModule != nullptr)
    {
        rtlInitSrwLock = GetProcAddress(ntdllModule, "RtlInitializeSRWLock");
    }

    if (initSrwLock != nullptr
        && importedFunction == reinterpret_cast<uptr>(initSrwLock))
    {
        return true;
    }

    if (rtlInitSrwLock != nullptr
        && importedFunction == reinterpret_cast<uptr>(rtlInitSrwLock))
    {
        return true;
    }

    return false;
}

inline bool HasLikelyInitSrwLockNearStart(
    const SectionCache& textSection,
    const IMemoryAccessor& mem,
    uptr functionVa,
    u32 searchLength)
{
    if (functionVa < textSection.va || functionVa >= textSection.va + textSection.size)
    {
        return false;
    }

    u32 startOffset = VaToSectionOffset(textSection, functionVa);
    u32 endOffset = startOffset + searchLength;
    if (endOffset > textSection.size)
    {
        endOffset = textSection.size;
    }

    bool hasAnyIndirectImportCall = false;
    for (u32 i = startOffset; i + 6 <= endOffset; ++i)
    {
        if (textSection.data[i] == 0xFF && textSection.data[i + 1] == 0x15)
        {
            hasAnyIndirectImportCall = true;
            if (IsLikelyInitSrwLockCall(textSection, mem, i))
            {
                return true;
            }
        }
    }

    return hasAnyIndirectImportCall;
}

inline bool ValidateNamePoolCandidateFast(
    const IMemoryAccessor& mem,
    uptr candidate)
{
    u32 currentBlock = 0;
    u32 cursor = 0;
    ReadValue(mem, candidate + 0x08, currentBlock);
    ReadValue(mem, candidate + 0x0C, cursor);

    if (currentBlock > 0x2000 || cursor == 0 || cursor > 0x40000)
    {
        return false;
    }

    uptr block0 = 0;
    if (!ReadPtr(mem, candidate + 0x10, block0) || !IsCanonicalUserPtr(block0))
    {
        return false;
    }

    std::vector<char> probeBuffer(0x600, 0);
    if (!mem.Read(block0, probeBuffer.data(), probeBuffer.size()))
    {
        return false;
    }

    if (!ContainsStringInBuffer(probeBuffer, "None"))
    {
        return false;
    }

    if (!ContainsStringInBuffer(probeBuffer, "ByteProperty"))
    {
        return false;
    }

    return true;
}

inline bool ValidateNameArrayCandidateFast(
    const IMemoryAccessor& mem,
    uptr candidate)
{
    uptr chunksPtr = 0;
    if (!ReadPtr(mem, candidate, chunksPtr) || !IsCanonicalUserPtr(chunksPtr))
    {
        return false;
    }

    i32 numElements = 0;
    ReadValue(mem, candidate + 0x08, numElements);
    if (numElements < 100 || numElements > 5000000)
    {
        return false;
    }

    uptr chunk0 = 0;
    if (!ReadPtr(mem, chunksPtr, chunk0) || !IsCanonicalUserPtr(chunk0))
    {
        return false;
    }

    uptr entry0 = 0;
    if (!ReadPtr(mem, chunk0, entry0) || !IsCanonicalUserPtr(entry0))
    {
        return false;
    }

    for (i32 stringOffset : { 0x10, 0x0C, 0x08, 0x06, 0x04 })
    {
        char buffer[5] = {};
        if (mem.Read(entry0 + stringOffset, buffer, 4)
            && std::string(buffer, 4) == "None")
        {
            return true;
        }
    }

    return false;
}

inline bool TryFindNamePoolByConstructorPattern(
    const std::vector<SectionCache>& sections,
    const IMemoryAccessor& mem,
    uptr& outGNames)
{
    const SectionCache* textSection = FindSection(sections, ".text");
    if (textSection == nullptr || textSection->data.empty())
    {
        return false;
    }

    uptr bytePropertyVa = FindStringInSections(sections, "ByteProperty");
    if (!bytePropertyVa)
    {
        bytePropertyVa = FindWideStringInSections(sections, "ByteProperty");
    }

    if (!bytePropertyVa)
    {
        return false;
    }

    u32 searchOffset = 0;
    while (searchOffset < textSection->size)
    {
        i32 matchOffset = MatchSignature(
            textSection->data.data(),
            textSection->size,
            "48 8D 0D ? ? ? ? E8",
            searchOffset);

        if (matchOffset < 0)
        {
            break;
        }

        u32 signatureOffset = static_cast<u32>(matchOffset);
        searchOffset = signatureOffset + 1;

        uptr namePoolCandidate = ResolveRipRelativeTarget(
            *textSection,
            signatureOffset,
            3,
            7);
        if (!namePoolCandidate)
        {
            continue;
        }

        uptr constructorVa = ResolveE8Call(
            textSection->data.data(),
            signatureOffset + 7,
            textSection->va);
        if (constructorVa < textSection->va
            || constructorVa >= textSection->va + textSection->size)
        {
            continue;
        }

        if (!HasLikelyInitSrwLockNearStart(
                *textSection,
                mem,
                constructorVa,
                0x50))
        {
            continue;
        }

        if (!FindLeaXrefInRange(
                *textSection,
                bytePropertyVa,
                constructorVa,
                0x2A0))
        {
            continue;
        }

        if (!ValidateNamePoolCandidateFast(mem, namePoolCandidate))
        {
            continue;
        }

        outGNames = namePoolCandidate;
        return true;
    }

    return false;
}

inline bool TryFindNamePoolByDataScan(
    const SectionCache& dataSection,
    const IMemoryAccessor& mem,
    uptr& outGNames)
{
    for (u32 offset = 0; offset + 0x20 <= dataSection.size; offset += 8)
    {
        uptr candidate = dataSection.va + offset;
        if (ValidateNamePoolCandidateFast(mem, candidate))
        {
            outGNames = candidate;
            return true;
        }
    }

    return false;
}

inline bool TryFindNameArrayByDataScan(
    const SectionCache& dataSection,
    const IMemoryAccessor& mem,
    uptr& outGNames)
{
    for (u32 offset = 0; offset + 0x10 <= dataSection.size; offset += 8)
    {
        uptr candidate = dataSection.va + offset;
        if (ValidateNameArrayCandidateFast(mem, candidate))
        {
            outGNames = candidate;
            return true;
        }
    }

    return false;
}

// 在 .data 段中扫描 GNames
inline bool ScanGNames(
    const std::vector<SectionCache>& sections,
    const IMemoryAccessor& mem,
    uptr& outGNames,
    bool& outIsNamePool)
{
    const SectionCache* dataSection = FindSection(sections, ".data");
    if (!dataSection)
    {
        std::cerr << "[xrd] .data 段未找到（GNames 扫描）\n";
        return false;
    }

    if (TryFindNamePoolByConstructorPattern(sections, mem, outGNames))
    {
        outIsNamePool = true;
        std::cerr << "[xrd] GNames (FNamePool) 找到: 0x"
                  << std::hex << outGNames
                  << " (ctor)\n" << std::dec;
        return true;
    }

    if (TryFindNamePoolByDataScan(*dataSection, mem, outGNames))
    {
        outIsNamePool = true;
        std::cerr << "[xrd] GNames (FNamePool) 找到: 0x"
                  << std::hex << outGNames
                  << " (.data fallback)\n" << std::dec;
        return true;
    }

    if (TryFindNameArrayByDataScan(*dataSection, mem, outGNames))
    {
        outIsNamePool = false;
        std::cerr << "[xrd] GNames (TNameEntryArray) 找到: 0x"
                  << std::hex << outGNames
                  << " (.data fallback)\n" << std::dec;
        return true;
    }

    std::cerr << "[xrd] GNames 未找到\n";
    return false;
}

// 动态探测 FNamePoolBlockBits（对标 Rei-Dumper NameArray::PostInit）
// 从 0xE(14) 开始，逆序遍历对象，找到 CompIdx >> BlockBits == NumChunks 的值
inline void DetectFNamePoolBlockBits(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (!off.bUseNamePool || off.GNames == 0)
    {
        return;
    }

    i32 numChunks = 0;
    for (i32 maxChunkOffset : { 0x00, 0x08 })
    {
        i32 value = 0;
        ReadValue(mem, off.GNames + maxChunkOffset, value);
        if (value > 0 && value < 0x10000)
        {
            numChunks = value;
            break;
        }
    }

    if (numChunks <= 0)
    {
        std::cerr << "[xrd] FNamePool NumChunks 读取失败，BlockBits 保持默认\n";
        return;
    }

    i32 totalObjects = GetObjectCount(mem, off);
    if (totalObjects <= 0)
    {
        return;
    }

    i32 blockBits = 0x0E;
    constexpr i32 kMaxBlockBits = 0x14;

    i32 index = totalObjects - 1;
    while (index >= 0 && blockBits <= kMaxBlockBits)
    {
        uptr object = ReadObjectAt(mem, off, index);
        if (!IsCanonicalUserPtr(object))
        {
            index--;
            continue;
        }

        FName name{};
        if (!ReadValue(mem, object + off.UObject_Name, name) || name.ComparisonIndex < 0)
        {
            index--;
            continue;
        }

        i32 chunkIndex = name.ComparisonIndex >> blockBits;
        if (chunkIndex == numChunks)
        {
            break;
        }

        if (chunkIndex > numChunks)
        {
            blockBits++;
            index = totalObjects - 1;
            continue;
        }

        index--;
    }

    if (blockBits > kMaxBlockBits)
    {
        std::cerr << "[xrd] FNamePoolBlockBits 探测失败，保持默认\n";
        return;
    }

    off.FNamePoolBlockBits = blockBits;
    std::cerr << "[xrd] FNamePoolBlockBits: 0x"
              << std::hex << blockBits << std::dec << "\n";
}

} // namespace resolve
} // namespace xrd
