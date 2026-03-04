#pragma once
// Xrd-eXternalrEsolve - UObject 偏移发现
// 通过统计分析自动发现 UObject/UStruct 各字段偏移

#include "../core/context.hpp"
#include <iostream>
#include <map>
#include <algorithm>

namespace xrd
{
namespace resolve
{

// 从 GObjects 中按索引读取一个 UObject 指针
inline uptr ReadObjectAt(const IMemoryAccessor& mem, const UEOffsets& off, i32 index)
{
    if (off.bIsChunkedObjArray)
    {
        i32 chunkIdx = index / off.ChunkSize;
        i32 withinIdx = index % off.ChunkSize;

        // +0x00 处是 Chunks** 指针
        uptr chunksPtr = 0;
        if (!ReadPtr(mem, off.GObjects, chunksPtr) || !IsCanonicalUserPtr(chunksPtr))
        {
            return 0;
        }

        uptr chunk = 0;
        if (!ReadPtr(mem, chunksPtr + chunkIdx * sizeof(uptr), chunk) ||
            !IsCanonicalUserPtr(chunk))
        {
            return 0;
        }

        uptr obj = 0;
        ReadPtr(mem, chunk + withinIdx * off.FUObjectItemSize + off.FUObjectItemInitialOffset, obj);
        return obj;
    }
    else
    {
        // Fixed 模式：+0x00 处是 Objects* 指针
        uptr objectsPtr = 0;
        if (!ReadPtr(mem, off.GObjects, objectsPtr) || !IsCanonicalUserPtr(objectsPtr))
        {
            return 0;
        }

        uptr obj = 0;
        ReadPtr(mem, objectsPtr + index * off.FUObjectItemSize + off.FUObjectItemInitialOffset, obj);
        return obj;
    }
}

// 获取对象总数
inline i32 GetObjectCount(const IMemoryAccessor& mem, const UEOffsets& off)
{
    if (off.bIsChunkedObjArray)
    {
        // Chunked: NumElements 在 +0x14
        i32 count = 0;
        ReadValue(mem, off.GObjects + 0x14, count);
        return count;
    }
    else
    {
        // Fixed: NumObjects 在 sizeof(void*) + 4 = +0x0C (64bit)
        i32 count = 0;
        ReadValue(mem, off.GObjects + static_cast<i32>(sizeof(uptr)) + 4, count);
        return count;
    }
}

// 通过采样分析发现 UObject 各字段偏移
inline bool DiscoverUObjectOffsets(const IMemoryAccessor& mem, UEOffsets& off)
{
    i32 totalObjects = GetObjectCount(mem, off);
    if (totalObjects <= 0)
    {
        std::cerr << "[xrd] 对象数量为 0\n";
        return false;
    }

    std::cerr << "[xrd] 对象总数: " << totalObjects << "\n";

    // 采样前 500 个有效对象
    std::vector<uptr> samples;
    for (i32 i = 0; i < std::min(totalObjects, 500); ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (IsCanonicalUserPtr(obj))
        {
            samples.push_back(obj);
        }
    }

    if (samples.size() < 10)
    {
        std::cerr << "[xrd] 有效对象太少: " << samples.size() << "\n";
        return false;
    }

    std::cerr << "[xrd] 采样 " << samples.size() << " 个对象用于偏移发现\n";

    // ─── 发现 Class 偏移（指向另一个合法 UObject 的指针） ───
    for (i32 classOff : {0x10, 0x18, 0x08})
    {
        int validCount = 0;
        for (auto obj : samples)
        {
            uptr classPtr = 0;
            if (ReadPtr(mem, obj + classOff, classPtr) && IsCanonicalUserPtr(classPtr))
            {
                validCount++;
            }
        }
        if (validCount > (int)samples.size() * 80 / 100)
        {
            off.UObject_Class = classOff;
            std::cerr << "[xrd] UObject::Class +0x" << std::hex << classOff
                      << std::dec << " (命中: " << validCount << ")\n";
            break;
        }
    }

    if (off.UObject_Class == -1)
    {
        std::cerr << "[xrd] 未找到 UObject::Class 偏移\n";
        return false;
    }

    // ─── 发现 Index 偏移（i32，值应与对象在 GObjects 中的索引匹配） ───
    for (i32 idxOff : {0x0C, 0x08, 0x04})
    {
        int matchCount = 0;
        for (i32 i = 0; i < std::min((i32)samples.size(), 100); ++i)
        {
            i32 readIdx = 0;
            if (ReadValue(mem, samples[i] + idxOff, readIdx) && readIdx == i)
            {
                matchCount++;
            }
        }
        if (matchCount > 50)
        {
            off.UObject_Index = idxOff;
            std::cerr << "[xrd] UObject::Index +0x" << std::hex << idxOff
                      << std::dec << " (匹配: " << matchCount << ")\n";
            break;
        }
    }

    // ─── 发现 Flags 偏移 ───
    for (i32 flagOff : {0x08, 0x04, 0x0C})
    {
        if (flagOff == off.UObject_Index || flagOff == off.UObject_Class)
        {
            continue;
        }

        int validCount = 0;
        for (auto obj : samples)
        {
            i32 flags = 0;
            ReadValue(mem, obj + flagOff, flags);
            // 常见标志位在低 16 位
            if (flags != 0 && (flags & 0xFFFF0000) == 0)
            {
                validCount++;
            }
        }
        if (validCount > 0)
        {
            off.UObject_Flags = flagOff;
            std::cerr << "[xrd] UObject::Flags +0x" << std::hex << flagOff << std::dec << "\n";
            break;
        }
    }

    // ─── 发现 Name 偏移（FName: CompIdx 应为合理正整数） ───
    for (i32 nameOff : {0x18, 0x20, 0x10, 0x28})
    {
        if (nameOff == off.UObject_Class)
        {
            continue;
        }

        int validCount = 0;
        for (i32 i = 0; i < std::min((i32)samples.size(), 50); ++i)
        {
            i32 compIdx = 0;
            if (ReadValue(mem, samples[i] + nameOff, compIdx))
            {
                if (compIdx > 0 && compIdx < 2000000)
                {
                    validCount++;
                }
            }
        }
        if (validCount > 30)
        {
            off.UObject_Name = nameOff;
            std::cerr << "[xrd] UObject::Name +0x" << std::hex << nameOff
                      << std::dec << " (命中: " << validCount << ")\n";
            break;
        }
    }

    // ─── 发现 Outer 偏移（指针，顶层 Package 为 null） ───
    for (i32 outerOff : {0x20, 0x28, 0x30, 0x18})
    {
        if (outerOff == off.UObject_Class || outerOff == off.UObject_Name)
        {
            continue;
        }

        int validOrNull = 0;
        for (auto obj : samples)
        {
            uptr outer = 0;
            if (ReadPtr(mem, obj + outerOff, outer))
            {
                if (outer == 0 || IsCanonicalUserPtr(outer))
                {
                    validOrNull++;
                }
            }
        }
        if (validOrNull > (int)samples.size() * 70 / 100)
        {
            off.UObject_Outer = outerOff;
            std::cerr << "[xrd] UObject::Outer +0x" << std::hex << outerOff << std::dec << "\n";
            break;
        }
    }

    off.UObject_Vft = 0x00;
    return off.UObject_Class != -1 && off.UObject_Name != -1;
}

} // namespace resolve
} // namespace xrd
