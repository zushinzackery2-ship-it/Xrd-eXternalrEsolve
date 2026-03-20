#pragma once
// Xrd-eXternalrEsolve - GWorld 扫描器
// 对标 Rei-Dumper: 在 GObjects 中找到 World 实例，然后在 .data 段反向搜索指向它的指针

#include "../../core/context.hpp"
#include "../property/scan_property_offsets.hpp"
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xrd
{
namespace resolve
{

inline void FindPointerMatchesInCachedSection(
    const SectionCache& section,
    uptr targetValue,
    std::vector<uptr>& matches)
{
    for (u32 offset = 0; offset + sizeof(uptr) <= section.size; offset += sizeof(uptr))
    {
        uptr value = 0;
        std::memcpy(&value, section.data.data() + offset, sizeof(value));
        if (value == targetValue)
        {
            matches.push_back(section.va + offset);
        }
    }
}

inline bool IsNonCdoWorldInstance(
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    uptr obj,
    std::unordered_map<uptr, bool>& worldClassCache)
{
    if (!IsCanonicalUserPtr(obj))
    {
        return false;
    }

    uptr cls = 0;
    if (!ReadPtr(mem, obj + off.UObject_Class, cls) || !IsCanonicalUserPtr(cls))
    {
        return false;
    }

    auto cacheIt = worldClassCache.find(cls);
    bool isWorldClass = false;

    if (cacheIt != worldClassCache.end())
    {
        isWorldClass = cacheIt->second;
    }
    else
    {
        FName clsFn{};
        if (!ReadValue(mem, cls + off.UObject_Name, clsFn))
        {
            worldClassCache.emplace(cls, false);
            return false;
        }

        std::string clsName = ResolveNameDirect(
            mem,
            off,
            clsFn.ComparisonIndex,
            clsFn.Number);
        isWorldClass = (clsName == "World");
        worldClassCache.emplace(cls, isWorldClass);
    }

    if (!isWorldClass)
    {
        return false;
    }

    u32 flags = 0;
    ReadValue(mem, obj + off.UObject_Flags, flags);
    return (flags & 0x10) == 0;
}

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

    // 从 UObjectArray 中收集所有 "World" 类实例（非 CDO），
    // 然后再去 .data 段做指针恒等校对。
    // 之前这里只取第一个 World 实例，切场或重连时很容易挑错对象。
    i32 total = GetObjectCount(mem, off);
    std::unordered_map<uptr, bool> worldClassCache;
    std::unordered_set<uptr> worldObjects;
    worldObjects.reserve(8);

    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (IsNonCdoWorldInstance(mem, off, obj, worldClassCache))
        {
            worldObjects.insert(obj);
        }
    }

    if (worldObjects.empty())
    {
        std::cerr << "[xrd] GWorld 未找到（无 World 实例）\n";
        return false;
    }

    // 在 .data 段中搜索所有指向 World 实例的指针。
    const SectionCache* dataSection = FindSection(sections, ".data");
    if (!dataSection)
    {
        return false;
    }

    std::vector<uptr> results;
    results.reserve(4);
    for (u32 offset = 0; offset + sizeof(uptr) <= dataSection->size; offset += sizeof(uptr))
    {
        uptr value = 0;
        std::memcpy(&value, dataSection->data.data() + offset, sizeof(value));
        if (worldObjects.find(value) != worldObjects.end())
        {
            results.push_back(dataSection->va + offset);
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
        // 候选搜索走本地缓存，但最终归属仍用极少量远程重读确认。
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
