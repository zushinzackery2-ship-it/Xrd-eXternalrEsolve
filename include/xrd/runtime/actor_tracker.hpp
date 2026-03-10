#pragma once
// Xrd-eXternalrEsolve - Actor 跟踪器
// 把场景检测、Actor 指针快照、类名缓存、玩家 Pawn 回退统一收口，避免上层重复实现。

#include "scene_watch.hpp"
#include "../engine/world/world_actors.hpp"
#include "../helpers/w2s.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace xrd
{
struct PlayerActorState
{
    uptr pawnPtr = 0;
    uptr meshPtr = 0;
    FVector worldPos{};
    bool hasWorldPos = false;
};

struct ActorTrackerSnapshot
{
    bool actorsValid = false;
    SceneWatchResult scene{};
    std::vector<uptr> actorPtrs;
    PlayerActorState player;
};

class ActorTracker
{
public:
    bool Poll(ActorTrackerSnapshot& out)
    {
        out = {};
        out.scene = DetectSceneChange(sceneWatchState, Mem());

        if (out.scene.changed)
        {
            ResetActorCaches();
            lastPlayerState = PlayerActorState{};
        }

        ActorArray actorArray;
        if (GetPersistentLevelActors(actorArray)
            && actorArray.count > 0
            && actorArray.count <= 100000)
        {
            std::vector<uptr> rawActorPtrs(static_cast<std::size_t>(actorArray.count));
            if (Mem().Read(
                actorArray.data,
                rawActorPtrs.data(),
                rawActorPtrs.size() * sizeof(uptr)))
            {
                out.actorPtrs.reserve(rawActorPtrs.size());
                for (uptr actorPtr : rawActorPtrs)
                {
                    if (IsCanonicalUserPtr(actorPtr))
                    {
                        out.actorPtrs.push_back(actorPtr);
                    }
                }

                PurgeActorClassCache(out.actorPtrs);
                out.actorsValid = true;
            }
        }

        UpdatePlayerState(out.player, out.scene.currentWorldPtr != 0);
        return out.actorsValid;
    }

    void Reset()
    {
        ResetSceneWatch(sceneWatchState);
        ResetActorCaches();
        lastPlayerState = PlayerActorState{};
    }

    std::string ResolveActorClassName(uptr actorPtr)
    {
        std::vector<uptr> actorPtrs = { actorPtr };
        std::vector<std::string> classNames;
        ResolveActorClassNames(actorPtrs, classNames);
        return classNames.empty() ? std::string{} : classNames[0];
    }

    void ResolveActorClassNames(
        const std::vector<uptr>& actorPtrs,
        std::vector<std::string>& outClassNames)
    {
        outClassNames.assign(actorPtrs.size(), std::string{});
        if (actorPtrs.empty())
        {
            return;
        }

        std::vector<uptr> classAddresses;
        std::vector<std::size_t> unresolvedIndices;
        classAddresses.reserve(actorPtrs.size());
        unresolvedIndices.reserve(actorPtrs.size());

        for (std::size_t i = 0; i < actorPtrs.size(); ++i)
        {
            uptr actorPtr = actorPtrs[i];
            if (!actorPtr)
            {
                continue;
            }

            auto actorIt = actorClassNameCache.find(actorPtr);
            if (actorIt != actorClassNameCache.end())
            {
                outClassNames[i] = actorIt->second;
                continue;
            }

            classAddresses.push_back(actorPtr + Off().UObject_Class);
            unresolvedIndices.push_back(i);
        }

        if (classAddresses.empty())
        {
            return;
        }

        std::vector<uptr> classPointers(classAddresses.size(), 0);
        ReadBatchUniform<uptr>(
            Mem(),
            classAddresses.data(),
            classPointers.data(),
            static_cast<u32>(classAddresses.size())
        );

        for (std::size_t i = 0; i < unresolvedIndices.size(); ++i)
        {
            uptr actorPtr = actorPtrs[unresolvedIndices[i]];
            uptr classPtr = classPointers[i];
            if (!IsCanonicalUserPtr(classPtr))
            {
                continue;
            }

            std::string className;
            auto classIt = classNameCache.find(classPtr);
            if (classIt != classNameCache.end())
            {
                className = classIt->second;
            }
            else
            {
                className = GetObjectName(classPtr);
                if (!className.empty())
                {
                    classNameCache.emplace(classPtr, className);
                }
            }

            if (!className.empty())
            {
                actorClassNameCache.emplace(actorPtr, className);
                outClassNames[unresolvedIndices[i]] = std::move(className);
            }
        }
    }

private:
    void ResetActorCaches()
    {
        classNameCache.clear();
        actorClassNameCache.clear();
    }

    void PurgeActorClassCache(const std::vector<uptr>& actorPtrs)
    {
        if (actorClassNameCache.empty())
        {
            return;
        }

        std::unordered_map<uptr, bool> activeActors;
        activeActors.reserve(actorPtrs.size() * 2 + 1);
        for (uptr actorPtr : actorPtrs)
        {
            activeActors[actorPtr] = true;
        }

        for (auto it = actorClassNameCache.begin(); it != actorClassNameCache.end();)
        {
            if (activeActors.find(it->first) == activeActors.end())
            {
                it = actorClassNameCache.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void UpdatePlayerState(PlayerActorState& out, bool allowFallback)
    {
        uptr pawnPtr = GetAPawn();
        uptr meshPtr = pawnPtr ? ReadActorFieldPtr(pawnPtr, "Mesh") : 0;

        FVector worldPos{};
        bool hasWorldPos = false;
        if (pawnPtr)
        {
            // APawnBase 只认 Pawn 的 RootComponent。
            // 这里如果回退到 Mesh，会把“基于 Pawn 基座”的语义污染掉。
            hasWorldPos = GetActorRootWorldPos(pawnPtr, worldPos);
        }

        if (pawnPtr)
        {
            out.pawnPtr = pawnPtr;
            if (meshPtr)
            {
                out.meshPtr = meshPtr;
            }
            else if (lastPlayerState.pawnPtr == pawnPtr)
            {
                // Mesh 指针偶发漏读时继续沿用同 Pawn 的旧值，避免玩家骨骼线程瞬断。
                out.meshPtr = lastPlayerState.meshPtr;
            }

            if (hasWorldPos)
            {
                out.worldPos = worldPos;
                out.hasWorldPos = true;
            }
            else if (lastPlayerState.pawnPtr == pawnPtr && lastPlayerState.hasWorldPos)
            {
                // Pawn 链还在，但这次瞬时没读到 C2W。
                // 这里继续沿用上一帧有效位置，避免 APawnBase 与 CameraBase 来回切换。
                out.worldPos = lastPlayerState.worldPos;
                out.hasWorldPos = true;
            }
            else
            {
                out.worldPos = FVector{};
                out.hasWorldPos = false;
            }

            lastPlayerState.pawnPtr = pawnPtr;
            lastPlayerState.meshPtr = out.meshPtr;
            if (!out.hasWorldPos)
            {
                lastPlayerState.worldPos = FVector{};
            }
            lastPlayerState.hasWorldPos = out.hasWorldPos;
            if (out.hasWorldPos)
            {
                lastPlayerState.worldPos = out.worldPos;
            }
            return;
        }

        if (allowFallback)
        {
            out = lastPlayerState;
            return;
        }

        out = {};
    }

    SceneWatchState sceneWatchState;
    std::unordered_map<uptr, std::string> classNameCache;
    std::unordered_map<uptr, std::string> actorClassNameCache;
    PlayerActorState lastPlayerState;
};
} // namespace xrd
