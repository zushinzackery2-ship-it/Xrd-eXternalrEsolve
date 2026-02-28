#pragma once
// Xrd-eXternalrEsolve - DebugCanvasObject 定位
// 扫描 .data 段，找到存储 DebugCanvasObject TMap Data 指针的静态地址
// 用于 ViewProjection 矩阵读取链路

#include "../core/context.hpp"
#include "scan_property_offsets.hpp"

namespace xrd::resolve
{

// 从 GNames 位置向两侧扩散扫描 .data 段
// 查找 FName 为 "DebugCanvasObject" 的全局指针（TMap Data 数组指针）
inline bool ScanDebugCanvasObject(
    const std::vector<SectionCache>& sections,
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    uptr& outAddr)
{
    outAddr = 0;

    const SectionCache* dataSection = FindSection(sections, ".data");
    if (!dataSection)
    {
        return false;
    }

    uptr dataVa = dataSection->va;
    u32 dataSize = dataSection->size;

    // GNames 必须在 .data 段内才能以它为中心扩散
    if (off.GNames < dataVa || off.GNames >= dataVa + dataSize)
    {
        return false;
    }

    u32 gnamesOffset = static_cast<u32>(off.GNames - dataVa) & ~7u;
    u32 maxRadius = std::max(gnamesOffset, dataSize - gnamesOffset);

    for (u32 radius = 0; radius <= maxRadius; radius += 8)
    {
        // 向两侧扩散
        for (int dir = 0; dir < 2; ++dir)
        {
            u32 off_in_data;
            if (dir == 0)
            {
                if (radius > gnamesOffset)
                {
                    continue;
                }
                off_in_data = gnamesOffset - radius;
            }
            else
            {
                if (radius == 0)
                {
                    continue;
                }
                off_in_data = gnamesOffset + radius;
                if (off_in_data + 8 > dataSize)
                {
                    continue;
                }
            }

            uptr addr = dataVa + off_in_data;
            uptr ptr = 0;
            if (!ReadPtr(mem, addr, ptr))
            {
                continue;
            }
            if (!IsCanonicalUserPtr(ptr))
            {
                continue;
            }

            // 尝试读取对象的 FName
            FName fn{};
            if (!ReadValue(mem, ptr + off.UObject_Name, fn))
            {
                continue;
            }

            std::string name = ResolveNameDirect(
                mem, off, fn.ComparisonIndex, fn.Number);

            if (name == "DebugCanvasObject")
            {
                outAddr = addr;
                return true;
            }
        }
    }

    return false;
}

} // namespace xrd::resolve
