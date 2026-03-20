#pragma once
// Xrd-eXternalrEsolve - Actor 枚举缓存
// 只在 Level / Actor TArray 结构特征变化时重建全量 Actor 列表，
// 避免热循环里每次都把所有数组整批搬一遍。

#include "../engine/world/world_levels.hpp"
#include <unordered_set>
#include <vector>

namespace xrd
{
struct LevelActorSourceSignature
{
    uptr levelPtr = 0;
    uptr dataPtr = 0;
    i32 count = 0;
    u64 fingerprint = 0;

    bool operator==(const LevelActorSourceSignature& other) const
    {
        return levelPtr == other.levelPtr
            && dataPtr == other.dataPtr
            && count == other.count
            && fingerprint == other.fingerprint;
    }
};

class ActorEnumerationCache
{
public:
    bool Refresh(uptr worldPtr, std::vector<uptr>& outActors)
    {
        outActors.clear();
        lastRefreshRebuilt = false;

        if (!IsCanonicalUserPtr(worldPtr))
        {
            Reset();
            return false;
        }

        std::vector<LevelActorSourceSignature> currentSources;
        if (!CollectLevelSources(worldPtr, currentSources))
        {
            Reset();
            return false;
        }

        if (cachedActorPtrs.empty() || currentSources != cachedSources)
        {
            cachedSources = std::move(currentSources);
            RebuildActorCache();
            lastRefreshRebuilt = true;
        }

        outActors = cachedActorPtrs;
        return !outActors.empty();
    }

    bool LastRefreshRebuilt() const
    {
        return lastRefreshRebuilt;
    }

    void Reset()
    {
        cachedSources.clear();
        cachedActorPtrs.clear();
        lastRefreshRebuilt = false;
    }

private:
    static u64 BuildActorArrayFingerprint(const ActorArray& actorArray)
    {
        if (!IsCanonicalUserPtr(actorArray.data) || actorArray.count <= 0)
        {
            return 0;
        }

        constexpr u64 kFnvOffset = 1469598103934665603ull;
        constexpr u64 kFnvPrime = 1099511628211ull;
        constexpr i32 kSampleCount = 16;

        u64 fingerprint = kFnvOffset;
        for (i32 sampleIndex = 0; sampleIndex < kSampleCount; ++sampleIndex)
        {
            i32 actorIndex = 0;
            if (actorArray.count > 1)
            {
                actorIndex = (sampleIndex * (actorArray.count - 1)) / (kSampleCount - 1);
            }

            uptr actorPtr = 0;
            ReadPtr(
                Mem(),
                actorArray.data + static_cast<uptr>(actorIndex) * sizeof(uptr),
                actorPtr
            );

            fingerprint ^= static_cast<u64>(actorPtr);
            fingerprint *= kFnvPrime;
        }

        return fingerprint;
    }

    static bool CollectLevelSources(
        uptr worldPtr,
        std::vector<LevelActorSourceSignature>& outSources)
    {
        outSources.clear();

        std::vector<uptr> levels;
        if (!GetLoadedLevelsForWorld(worldPtr, levels))
        {
            return false;
        }

        outSources.reserve(levels.size());
        for (uptr levelPtr : levels)
        {
            ActorArray actorArray{};
            if (!GetLevelActors(levelPtr, actorArray))
            {
                continue;
            }

            LevelActorSourceSignature signature;
            signature.levelPtr = levelPtr;
            signature.dataPtr = actorArray.data;
            signature.count = actorArray.count;
            signature.fingerprint = BuildActorArrayFingerprint(actorArray);
            outSources.push_back(signature);
        }

        return !outSources.empty();
    }

    void RebuildActorCache()
    {
        cachedActorPtrs.clear();
        if (cachedSources.empty())
        {
            return;
        }

        std::size_t expectedActorCount = 0;
        for (const LevelActorSourceSignature& source : cachedSources)
        {
            if (source.count > 0)
            {
                expectedActorCount += static_cast<std::size_t>(source.count);
            }
        }

        cachedActorPtrs.reserve(expectedActorCount);

        std::unordered_set<uptr> seenActors;
        seenActors.reserve(expectedActorCount * 2 + 1);

        for (const LevelActorSourceSignature& source : cachedSources)
        {
            std::vector<uptr> rawActorPtrs(static_cast<std::size_t>(source.count));
            if (!Mem().Read(
                source.dataPtr,
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
                    cachedActorPtrs.push_back(actorPtr);
                }
            }
        }
    }

    std::vector<LevelActorSourceSignature> cachedSources;
    std::vector<uptr> cachedActorPtrs;
    bool lastRefreshRebuilt = false;
};
} // namespace xrd
