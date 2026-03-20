#pragma once
// Xrd-eXternalrEsolve - GObjects 扫描器
// 参考 Dumper7 的 ObjectArray 实现
// 支持 FChunkedFixedUObjectArray 和 FFixedUObjectArray 两种布局
// 自动探测 FUObjectItemSize 和 FUObjectItemInitialOffset

#include "scan_gobjects_validate.hpp"
#include <iostream>

namespace xrd
{
namespace resolve
{

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
            if (ValidateChunked(mem, candidate, off, sec))
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
            if (ValidateFixed(mem, candidate, off, sec))
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
            if (ValidateChunked(mem, candidate, off, &sec))
            {
                outGObjects = off.GObjects;
                outChunked = true;
                std::cerr << "[xrd] GObjects 找到: 0x" << std::hex
                          << outGObjects << " (chunked, "
                          << sec.name << ")" << std::dec << "\n";
                return true;
            }
            if (ValidateFixed(mem, candidate, off, &sec))
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
