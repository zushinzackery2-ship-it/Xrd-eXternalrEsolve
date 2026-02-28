#pragma once
// Xrd-eXternalrEsolve - UWorld / Actor / Pawn 访问
// 游戏世界遍历的高层 API

#include "../core/context.hpp"
#include "objects.hpp"
#include <string>
#include <vector>

namespace xrd
{

// ─── UWorld 访问 ───

inline uptr GetUWorld()
{
    if (!IsInited() || Off().GWorld == 0)
    {
        return 0;
    }
    uptr world = 0;
    GReadPtr(Off().GWorld, world);
    return world;
}

// ─── Level / Actor 访问 ───

struct ActorArray
{
    uptr data  = 0;
    i32  count = 0;
};

inline bool GetPersistentLevelActors(ActorArray& out)
{
    out = {};
    uptr world = GetUWorld();
    if (!world)
    {
        return false;
    }

    auto& off = Ctx().off;

    // 尝试已知偏移
    uptr level = 0;
    if (off.UWorld_PersistentLevel != -1)
    {
        GReadPtr(world + off.UWorld_PersistentLevel, level);
    }

    // 如果未知则扫描
    if (!IsCanonicalUserPtr(level))
    {
        for (i32 levelOff = 0x28; levelOff <= 0x100; levelOff += 8)
        {
            uptr candidate = 0;
            if (!GReadPtr(world + levelOff, candidate))
            {
                continue;
            }
            if (!IsCanonicalUserPtr(candidate))
            {
                continue;
            }

            // 通过类名验证是否为 ULevel
            uptr levelClass = 0;
            if (!GReadPtr(candidate + off.UObject_Class, levelClass))
            {
                continue;
            }
            if (!IsCanonicalUserPtr(levelClass))
            {
                continue;
            }

            std::string className = GetObjectName(levelClass);
            if (className == "Level")
            {
                level = candidate;
                Ctx().off.UWorld_PersistentLevel = levelOff;
                break;
            }
        }
    }

    if (!IsCanonicalUserPtr(level))
    {
        return false;
    }

    // 在 ULevel 中查找 Actors TArray
    if (off.ULevel_Actors != -1)
    {
        GReadPtr(level + off.ULevel_Actors, out.data);
        GReadI32(level + off.ULevel_Actors + 8, out.count);
        return IsCanonicalUserPtr(out.data) && out.count > 0;
    }

    // 扫描 Actors 数组
    for (i32 actorsOff = 0x80; actorsOff <= 0x150; actorsOff += 8)
    {
        uptr data = 0;
        i32 count = 0;
        if (!GReadPtr(level + actorsOff, data))
        {
            continue;
        }
        if (!IsCanonicalUserPtr(data))
        {
            continue;
        }

        GReadI32(level + actorsOff + 8, count);
        if (count <= 0 || count >= 200000)
        {
            continue;
        }

        uptr firstActor = 0;
        if (GReadPtr(data, firstActor) && IsCanonicalUserPtr(firstActor))
        {
            out.data = data;
            out.count = count;
            Ctx().off.ULevel_Actors = actorsOff;
            return true;
        }
    }

    return false;
}

// ─── 高层 Actor API ───

inline std::vector<uptr> GetAllActors()
{
    std::vector<uptr> result;
    ActorArray arr;
    if (!GetPersistentLevelActors(arr))
    {
        return result;
    }

    result.reserve(arr.count);
    for (i32 i = 0; i < arr.count; ++i)
    {
        uptr actor = 0;
        if (GReadPtr(arr.data + i * sizeof(uptr), actor) && IsCanonicalUserPtr(actor))
        {
            result.push_back(actor);
        }
    }
    return result;
}

} // namespace xrd
