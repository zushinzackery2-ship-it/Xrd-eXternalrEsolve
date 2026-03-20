#pragma once
// Xrd-eXternalrEsolve - Loaded Levels / Actor 全量枚举
// 把 World 下所有已加载 Level 的 Actors 汇总出来，避免只看 PersistentLevel 导致漏对象。

#include "world_access.hpp"
#include <unordered_set>

namespace xrd
{
namespace detail
{
    inline bool IsLevelObject(uptr levelPtr)
    {
        if (!IsCanonicalUserPtr(levelPtr))
        {
            return false;
        }

        uptr levelClass = GetObjectClass(levelPtr);
        return levelClass != 0 && GetObjectName(levelClass) == "Level";
    }

    inline bool DiscoverUWorldLevelsOffset(uptr world)
    {
        if (!IsCanonicalUserPtr(world))
        {
            return false;
        }

        auto& off = Ctx().off;
        if (off.UWorld_Levels != -1)
        {
            return true;
        }

        uptr worldClass = GetObjectClass(world);
        if (!worldClass)
        {
            return false;
        }

        off.UWorld_Levels = GetPropertyOffsetByName(worldClass, "Levels");
        return off.UWorld_Levels != -1;
    }

    inline void AppendUniqueLevel(
        uptr levelPtr,
        std::unordered_set<uptr>& seenLevels,
        std::vector<uptr>& outLevels)
    {
        if (!IsLevelObject(levelPtr))
        {
            return;
        }

        if (seenLevels.insert(levelPtr).second)
        {
            outLevels.push_back(levelPtr);
        }
    }
} // namespace detail

inline bool GetLoadedLevelsForWorld(uptr world, std::vector<uptr>& outLevels)
{
    outLevels.clear();
    if (!IsCanonicalUserPtr(world))
    {
        return false;
    }

    std::unordered_set<uptr> seenLevels;
    seenLevels.reserve(16);

    uptr persistentLevel = 0;
    if (GetPersistentLevel(world, persistentLevel))
    {
        detail::AppendUniqueLevel(persistentLevel, seenLevels, outLevels);
    }

    auto& off = Ctx().off;
    if (detail::DiscoverUWorldLevelsOffset(world) && off.UWorld_Levels != -1)
    {
        uptr levelsData = 0;
        i32 levelsCount = 0;
        if (GReadPtr(world + off.UWorld_Levels, levelsData)
            && GReadI32(world + off.UWorld_Levels + 8, levelsCount)
            && IsCanonicalUserPtr(levelsData)
            && levelsCount > 0
            && levelsCount <= 4096)
        {
            std::vector<uptr> levels(static_cast<std::size_t>(levelsCount));
            if (Mem().Read(
                levelsData,
                levels.data(),
                levels.size() * sizeof(uptr)))
            {
                for (uptr levelPtr : levels)
                {
                    detail::AppendUniqueLevel(levelPtr, seenLevels, outLevels);
                }
            }
        }
    }

    return !outLevels.empty();
}

inline bool GetLoadedLevels(std::vector<uptr>& outLevels)
{
    return GetLoadedLevelsForWorld(GetUWorld(), outLevels);
}

inline std::vector<uptr> GetAllActors()
{
    std::vector<uptr> result;

    std::vector<uptr> levels;
    if (!GetLoadedLevels(levels))
    {
        return result;
    }

    std::unordered_set<uptr> seenActors;
    seenActors.reserve(8192);

    for (uptr levelPtr : levels)
    {
        ActorArray actorArray{};
        if (!GetLevelActors(levelPtr, actorArray))
        {
            continue;
        }

        std::vector<uptr> rawActorPtrs(static_cast<std::size_t>(actorArray.count));
        if (!Mem().Read(
            actorArray.data,
            rawActorPtrs.data(),
            rawActorPtrs.size() * sizeof(uptr)))
        {
            continue;
        }

        for (uptr actorPtr : rawActorPtrs)
        {
            if (!IsCanonicalUserPtr(actorPtr))
            {
                continue;
            }

            if (seenActors.insert(actorPtr).second)
            {
                result.push_back(actorPtr);
            }
        }
    }

    return result;
}

} // namespace xrd
