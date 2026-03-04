#pragma once
// Xrd-eXternalrEsolve - GObjects 扫描器
// 参考 Dumper7 的 ObjectArray 实现
// 支持 FChunkedFixedUObjectArray 和 FFixedUObjectArray 两种布局
// 自动探测 FUObjectItemSize 和 FUObjectItemInitialOffset

#include "../core/context.hpp"
#include <iostream>
#include <algorithm>

namespace xrd
{
namespace resolve
{

// ─── FUObjectItem 自动探测 ───
// 参考 Dumper7 的 InitializeFUObjectItem
// 从第一个 FUObjectItem 的原始字节中探测 ItemSize 和 InitialOffset
inline bool ProbeItemLayout(
    const IMemoryAccessor& mem,
    uptr firstItemPtr,
    UEOffsets& off)
{
    // 找 InitialOffset：第一个能解引用为合法指针的 4 字节对齐偏移
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

    // 找 ItemSize：从 initialOffset+8 开始，找下一个合法对象指针
    // 第二个对象的指针位于 firstItemPtr + ItemSize + initialOffset
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

    // 回退默认值
    off.FUObjectItemSize = 0x18;
    return false;
}

// ─── Chunked 布局验证 ───
// 参考 Dumper7: Objects** at +0x00, MaxElements +0x10, NumElements +0x14,
//               MaxChunks +0x18, NumChunks +0x1C
inline bool ValidateChunked(
    const IMemoryAccessor& mem,
    uptr candidate,
    UEOffsets& off)
{
    uptr objectsPtr = 0;
    if (!ReadPtr(mem, candidate, objectsPtr) || !IsCanonicalUserPtr(objectsPtr))
    {
        return false;
    }

    i32 maxElements = 0, numElements = 0, maxChunks = 0, numChunks = 0;
    ReadI32(mem, candidate + 0x10, maxElements);
    ReadI32(mem, candidate + 0x14, numElements);
    ReadI32(mem, candidate + 0x18, maxChunks);
    ReadI32(mem, candidate + 0x1C, numChunks);

    // Dumper7 的验证条件
    if (numChunks < 1 || numChunks > 0x14)
        return false;
    if (maxChunks < 6 || maxChunks > 0x5FF)
        return false;
    if (numElements <= 0x800 || maxElements <= 0x10000)
        return false;
    if (numElements > maxElements || numChunks > maxChunks)
        return false;
    if ((maxElements % 0x10) != 0)
        return false;

    i32 elemPerChunk = maxElements / maxChunks;
    if ((elemPerChunk % 0x10) != 0)
        return false;
    if (elemPerChunk < 0x8000 || elemPerChunk > 0x80000)
        return false;

    // 验证 NumChunks 和 NumElements 的一致性
    if (((numElements / elemPerChunk) + 1) != numChunks)
        return false;
    if ((maxElements / elemPerChunk) != maxChunks)
        return false;

    // 验证前几个 chunk 指针
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

    // 探测 FUObjectItem 布局
    uptr firstChunk = 0;
    ReadPtr(mem, objectsPtr, firstChunk);
    if (IsCanonicalUserPtr(firstChunk))
    {
        ProbeItemLayout(mem, firstChunk, off);
    }

    return true;
}

// ─── Fixed 布局验证 ───
// +0x00: Objects*, +0x08: MaxObjects, +0x0C: NumObjects
inline bool ValidateFixed(
    const IMemoryAccessor& mem,
    uptr candidate,
    UEOffsets& off)
{
    uptr objectsPtr = 0;
    if (!ReadPtr(mem, candidate, objectsPtr) || !IsCanonicalUserPtr(objectsPtr))
    {
        return false;
    }

    i32 maxObj = 0, numObj = 0;
    ReadI32(mem, candidate + static_cast<i32>(sizeof(uptr)), maxObj);
    ReadI32(mem, candidate + static_cast<i32>(sizeof(uptr)) + 4, numObj);

    if (numObj < 0x1000 || numObj > maxObj || maxObj > 0x400000)
    {
        return false;
    }

    // 验证第 5 个对象的 InternalIndex == 5
    // 参考 Dumper7 的验证方式
    // FUObjectItem 默认: ptr(8) + int32(4) + int32(4) = 0x10
    uptr fifthObj = 0;
    if (!ReadPtr(mem, objectsPtr + 5 * 0x18, fifthObj) ||
        !IsCanonicalUserPtr(fifthObj))
    {
        // 试 0x10 大小
        if (!ReadPtr(mem, objectsPtr + 5 * 0x10, fifthObj) ||
            !IsCanonicalUserPtr(fifthObj))
        {
            return false;
        }
    }

    // 验证 InternalIndex
    i32 idx = -1;
    // Index 通常在 VftPtr(8) + Flags(4) 之后 = +0x0C
    ReadI32(mem, fifthObj + 0x0C, idx);
    if (idx != 5)
    {
        // 也试 +0x08
        ReadI32(mem, fifthObj + 0x08, idx);
    }
    if (idx != 5)
    {
        return false;
    }

    off.bIsChunkedObjArray = false;
    off.GObjects = candidate;

    // 探测 FUObjectItem 布局
    ProbeItemLayout(mem, objectsPtr, off);

    return true;
}

// 在 .data 段中扫描 GObjects
inline bool ScanGObjects(
    const std::vector<SectionCache>& sections,
    const IMemoryAccessor& mem,
    uptr& outGObjects,
    bool& outChunked)
{
    auto& off = Ctx().off;

    // 先在 .data 段搜索
    for (auto& secName : {".data"})
    {
        const SectionCache* sec = FindSection(sections, secName);
        if (!sec)
        {
            continue;
        }

        for (u32 offset = 0; offset + 0x20 <= sec->size; offset += 4)
        {
            uptr candidate = sec->va + offset;

            // 优先尝试 chunked（更常见）
            if (ValidateChunked(mem, candidate, off))
            {
                outGObjects = off.GObjects;
                outChunked = true;
                std::cerr << "[xrd] GObjects 找到: 0x" << std::hex
                          << outGObjects << " (chunked)"
                          << std::dec << "\n";
                return true;
            }
        }

        // chunked 没找到，再试 fixed
        for (u32 offset = 0; offset + 0x20 <= sec->size; offset += 4)
        {
            uptr candidate = sec->va + offset;
            if (ValidateFixed(mem, candidate, off))
            {
                outGObjects = off.GObjects;
                outChunked = false;
                std::cerr << "[xrd] GObjects 找到: 0x" << std::hex
                          << outGObjects << " (fixed)"
                          << std::dec << "\n";
                return true;
            }
        }
    }

    // 回退：扫描所有段
    for (auto& sec : sections)
    {
        if (sec.name == ".data")
            continue;

        for (u32 offset = 0; offset + 0x20 <= sec.size; offset += 4)
        {
            uptr candidate = sec.va + offset;
            if (ValidateChunked(mem, candidate, off))
            {
                outGObjects = off.GObjects;
                outChunked = true;
                std::cerr << "[xrd] GObjects 找到: 0x" << std::hex
                          << outGObjects << " (chunked, "
                          << sec.name << ")" << std::dec << "\n";
                return true;
            }
            if (ValidateFixed(mem, candidate, off))
            {
                outGObjects = off.GObjects;
                outChunked = false;
                std::cerr << "[xrd] GObjects 找到: 0x" << std::hex
                          << outGObjects << " (fixed, "
                          << sec.name << ")" << std::dec << "\n";
                return true;
            }
        }
    }

    std::cerr << "[xrd] GObjects 未找到\n";
    return false;
}

} // namespace resolve
} // namespace xrd
