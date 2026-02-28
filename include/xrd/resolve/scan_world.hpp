#pragma once
// Xrd-eXternalrEsolve - GWorld 扫描器
// 对标 Rei-Dumper: 在 GObjects 中找到 World 实例，然后在 .data 段反向搜索指向它的指针

#include "../core/context.hpp"
#include "scan_property_offsets.hpp"
#include <iostream>
#include <vector>

namespace xrd
{
namespace resolve
{

// 对标 Rei-Dumper Off::InSDK::World::InitGWorld
// 1. 在 GObjects 中找到 World 类和它的实例
// 2. 扫描 .data 段搜索指向该实例的指针（即 UWorld** GWorld）
// 3. 多个结果时过滤 GActiveLogWorld
inline bool ScanGWorld(
    const std::vector<SectionCache>& sections,
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    uptr& outGWorld)
{
    outGWorld = 0;

    // 找到 "World" 类的实例（非 CDO）
    i32 total = GetObjectCount(mem, off);
    uptr worldObj = 0;

    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        // 读取对象的类名
        uptr cls = 0;
        if (!ReadPtr(mem, obj + off.UObject_Class, cls) || !IsCanonicalUserPtr(cls))
        {
            continue;
        }

        FName clsFn{};
        if (!ReadValue(mem, cls + off.UObject_Name, clsFn))
        {
            continue;
        }

        std::string clsName = ResolveNameDirect(mem, off, clsFn.ComparisonIndex, clsFn.Number);
        if (clsName != "World")
        {
            continue;
        }

        // 排除 CDO (RF_ClassDefaultObject = 0x10)
        u32 flags = 0;
        ReadValue(mem, obj + off.UObject_Flags, flags);
        if (flags & 0x10)
        {
            continue;
        }

        worldObj = obj;
        break;
    }

    if (!worldObj)
    {
        std::cerr << "[xrd] GWorld 未找到（无 World 实例）\n";
        return false;
    }

    // 在 .data 段中搜索所有指向 worldObj 的指针
    const SectionCache* dataSection = FindSection(sections, ".data");
    if (!dataSection)
    {
        return false;
    }

    std::vector<uptr> results;
    for (u32 offset = 0; offset + 8 <= dataSection->size; offset += 8)
    {
        uptr addr = dataSection->va + offset;
        uptr val = 0;
        if (!ReadPtr(mem, addr, val))
        {
            continue;
        }
        if (val == worldObj)
        {
            results.push_back(addr);
        }
    }

    if (results.empty())
    {
        std::cerr << "[xrd] GWorld 未找到（.data 中无指向 World 实例的指针）\n";
        return false;
    }

    if (results.size() == 1)
    {
        outGWorld = results[0];
    }
    else if (results.size() == 2)
    {
        // 对标 Rei-Dumper: 过滤 GActiveLogWorld
        // 短时间内重读第一个候选，如果值不变则是 GWorld
        uptr val1 = 0;
        ReadPtr(mem, results[0], val1);
        Sleep(50);
        uptr val2 = 0;
        ReadPtr(mem, results[0], val2);

        if (val1 == val2)
        {
            outGWorld = results[0];
        }
        else
        {
            outGWorld = results[1];
        }
    }
    else
    {
        // 多于 2 个结果，取第一个
        std::cerr << "[xrd] 检测到 " << results.size() << " 个 GWorld 候选\n";
        outGWorld = results[0];
    }

    std::cerr << "[xrd] GWorld 找到: 0x" << std::hex << outGWorld << std::dec << "\n";
    return true;
}

} // namespace resolve
} // namespace xrd
