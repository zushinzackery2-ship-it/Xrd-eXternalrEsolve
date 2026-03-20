#pragma once

#include "../../core/context.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>

namespace xrd
{
namespace resolve
{

template<typename T>
inline bool ReadCandidateFieldCached(
    const SectionCache* section,
    uptr candidate,
    u32 fieldOffset,
    T& out)
{
    if (section == nullptr || candidate < section->va)
    {
        return false;
    }

    u32 sectionOffset = static_cast<u32>(candidate - section->va);
    if (sectionOffset + fieldOffset + sizeof(T) > section->size)
    {
        return false;
    }

    std::memcpy(&out, &section->data[sectionOffset + fieldOffset], sizeof(T));
    return true;
}

inline bool ProbeItemLayout(
    const IMemoryAccessor& mem,
    uptr firstItemPtr,
    UEOffsets& off)
{
    i32 initialOffset = 0;
    for (i32 i = 0; i < 0x20; i += 4)
    {
        uptr val = 0;
        if (ReadPtr(mem, firstItemPtr + i, val) && IsCanonicalUserPtr(val))
        {
            initialOffset = i;
            break;
        }
    }

    for (i32 itemSize = static_cast<i32>(sizeof(uptr)) + 4;
         itemSize <= 0x38; itemSize += 4)
    {
        uptr secondObj = 0;
        uptr thirdObj = 0;
        if (ReadPtr(mem, firstItemPtr + initialOffset + itemSize, secondObj) &&
            ReadPtr(mem, firstItemPtr + initialOffset + itemSize * 2, thirdObj) &&
            IsCanonicalUserPtr(secondObj) && IsCanonicalUserPtr(thirdObj))
        {
            off.FUObjectItemSize = itemSize;
            off.FUObjectItemInitialOffset = initialOffset;
            std::cerr << "[xrd] FUObjectItem: size=0x"
                      << std::hex << itemSize
                      << " initialOffset=0x" << initialOffset
                      << std::dec << "\n";
            return true;
        }
    }

    off.FUObjectItemSize = 0x18;
    return false;
}

inline bool ValidateChunked(
    const IMemoryAccessor& mem,
    uptr candidate,
    UEOffsets& off,
    const SectionCache* cachedSection = nullptr)
{
    uptr objectsPtr = 0;
    if (!ReadCandidateFieldCached(cachedSection, candidate, 0x00, objectsPtr)
        && !ReadPtr(mem, candidate, objectsPtr))
    {
        return false;
    }

    if (!IsCanonicalUserPtr(objectsPtr))
    {
        return false;
    }

    i32 maxElements = 0;
    i32 numElements = 0;
    i32 maxChunks = 0;
    i32 numChunks = 0;

    if (!ReadCandidateFieldCached(cachedSection, candidate, 0x10, maxElements))
    {
        ReadI32(mem, candidate + 0x10, maxElements);
    }
    if (!ReadCandidateFieldCached(cachedSection, candidate, 0x14, numElements))
    {
        ReadI32(mem, candidate + 0x14, numElements);
    }
    if (!ReadCandidateFieldCached(cachedSection, candidate, 0x18, maxChunks))
    {
        ReadI32(mem, candidate + 0x18, maxChunks);
    }
    if (!ReadCandidateFieldCached(cachedSection, candidate, 0x1C, numChunks))
    {
        ReadI32(mem, candidate + 0x1C, numChunks);
    }

    if (numChunks < 1 || numChunks > 0x14)
    {
        return false;
    }
    if (maxChunks < 6 || maxChunks > 0x5FF)
    {
        return false;
    }
    if (numElements <= 0x800 || maxElements <= 0x10000)
    {
        return false;
    }
    if (numElements > maxElements || numChunks > maxChunks)
    {
        return false;
    }
    if ((maxElements % 0x10) != 0)
    {
        return false;
    }

    i32 elemPerChunk = maxElements / maxChunks;
    if ((elemPerChunk % 0x10) != 0)
    {
        return false;
    }
    if (elemPerChunk < 0x8000 || elemPerChunk > 0x80000)
    {
        return false;
    }

    if (((numElements / elemPerChunk) + 1) != numChunks)
    {
        return false;
    }
    if ((maxElements / elemPerChunk) != maxChunks)
    {
        return false;
    }

    for (i32 i = 0; i < std::min(numChunks, 3); ++i)
    {
        uptr chunk = 0;
        if (!ReadPtr(mem, objectsPtr + i * sizeof(uptr), chunk) ||
            !IsCanonicalUserPtr(chunk))
        {
            return false;
        }
    }

    off.bIsChunkedObjArray = true;
    off.ChunkSize = elemPerChunk;
    off.GObjects = candidate;

    uptr firstChunk = 0;
    ReadPtr(mem, objectsPtr, firstChunk);
    if (IsCanonicalUserPtr(firstChunk))
    {
        ProbeItemLayout(mem, firstChunk, off);
    }

    return true;
}

inline bool ValidateFixed(
    const IMemoryAccessor& mem,
    uptr candidate,
    UEOffsets& off,
    const SectionCache* cachedSection = nullptr)
{
    uptr objectsPtr = 0;
    if (!ReadCandidateFieldCached(cachedSection, candidate, 0x00, objectsPtr)
        && !ReadPtr(mem, candidate, objectsPtr))
    {
        return false;
    }

    if (!IsCanonicalUserPtr(objectsPtr))
    {
        return false;
    }

    i32 maxObj = 0;
    i32 numObj = 0;
    if (!ReadCandidateFieldCached(
            cachedSection,
            candidate,
            static_cast<u32>(sizeof(uptr)),
            maxObj))
    {
        ReadI32(mem, candidate + static_cast<i32>(sizeof(uptr)), maxObj);
    }
    if (!ReadCandidateFieldCached(
            cachedSection,
            candidate,
            static_cast<u32>(sizeof(uptr)) + 4,
            numObj))
    {
        ReadI32(mem, candidate + static_cast<i32>(sizeof(uptr)) + 4, numObj);
    }

    if (numObj < 0x1000 || numObj > maxObj || maxObj > 0x400000)
    {
        return false;
    }

    uptr fifthObj = 0;
    if (!ReadPtr(mem, objectsPtr + 5 * 0x18, fifthObj) ||
        !IsCanonicalUserPtr(fifthObj))
    {
        if (!ReadPtr(mem, objectsPtr + 5 * 0x10, fifthObj) ||
            !IsCanonicalUserPtr(fifthObj))
        {
            return false;
        }
    }

    i32 idx = -1;
    ReadI32(mem, fifthObj + 0x0C, idx);
    if (idx != 5)
    {
        ReadI32(mem, fifthObj + 0x08, idx);
    }
    if (idx != 5)
    {
        return false;
    }

    off.bIsChunkedObjArray = false;
    off.GObjects = candidate;
    ProbeItemLayout(mem, objectsPtr, off);
    return true;
}

} // namespace resolve
} // namespace xrd
