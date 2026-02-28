#pragma once
// Xrd-eXternalrEsolve - GNames 扫描器
// 搜索 FNamePool (UE4.23+) 或 TNameEntryArray

#include "../core/context.hpp"
#include <iostream>

namespace xrd
{
namespace resolve
{

// 验证 FNamePool 候选地址：检查 block0 中是否包含 "None" 字符串
inline bool ValidateGNames_NamePool(const IMemoryAccessor& mem, uptr candidate)
{
    // FNamePool 布局:
    //   +0x00: Lock (8 bytes)
    //   +0x08: CurrentBlock (u32)
    //   +0x0C: CurrentByteCursor (u32)
    //   +0x10: Blocks[] (指针数组)
    u32 currentBlock = 0, cursor = 0;
    ReadValue(mem, candidate + 0x08, currentBlock);
    ReadValue(mem, candidate + 0x0C, cursor);

    if (currentBlock > 8192 || cursor == 0 || cursor > 0x40000)
    {
        return false;
    }

    uptr block0 = 0;
    if (!ReadPtr(mem, candidate + 0x10, block0))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(block0))
    {
        return false;
    }

    // 在 block0 起始附近搜索 "None" 字符串
    for (u32 off = 0; off < 64; off += 2)
    {
        char buf[5] = {};
        if (mem.Read(block0 + off, buf, 4) && std::string(buf, 4) == "None")
        {
            return true;
        }
    }

    return false;
}

// 验证 TNameEntryArray 候选地址
inline bool ValidateGNames_Array(const IMemoryAccessor& mem, uptr candidate)
{
    uptr chunksPtr = 0;
    if (!ReadPtr(mem, candidate, chunksPtr))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(chunksPtr))
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
    if (!ReadPtr(mem, chunksPtr, chunk0))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(chunk0))
    {
        return false;
    }

    uptr entry0 = 0;
    if (!ReadPtr(mem, chunk0, entry0))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(entry0))
    {
        return false;
    }

    // 在 entry 的不同偏移处尝试读取 "None"
    for (i32 off : {0x10, 0x0C, 0x08, 0x06, 0x04})
    {
        char buf[5] = {};
        if (mem.Read(entry0 + off, buf, 4) && std::string(buf, 4) == "None")
        {
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

    u32 dataSize = dataSection->size;
    uptr dataVa = dataSection->va;

    // 优先尝试 FNamePool（现代 UE 更常见）
    for (u32 offset = 0; offset + 0x20 <= dataSize; offset += 8)
    {
        uptr candidate = dataVa + offset;
        if (ValidateGNames_NamePool(mem, candidate))
        {
            outGNames = candidate;
            outIsNamePool = true;
            std::cerr << "[xrd] GNames (FNamePool) 找到: 0x"
                      << std::hex << candidate << std::dec << "\n";
            return true;
        }
    }

    // 回退到 TNameEntryArray
    for (u32 offset = 0; offset + 0x10 <= dataSize; offset += 8)
    {
        uptr candidate = dataVa + offset;
        if (ValidateGNames_Array(mem, candidate))
        {
            outGNames = candidate;
            outIsNamePool = false;
            std::cerr << "[xrd] GNames (TNameEntryArray) 找到: 0x"
                      << std::hex << candidate << std::dec << "\n";
            return true;
        }
    }

    std::cerr << "[xrd] GNames 未找到\n";
    return false;
}

} // namespace resolve
} // namespace xrd
