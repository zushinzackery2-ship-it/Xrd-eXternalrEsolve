#pragma once
// Xrd-eXternalrEsolve - 场景切换检测
// `UWorld` / `Level` 的防抖与状态机在多个项目里都会重复出现，统一抽到 xrd。

#include "../core/context.hpp"

namespace xrd
{
struct SceneWatchState
{
    uptr lastWorldPtr = 0;
    uptr lastLevelPtr = 0;
    uptr candidateWorldPtr = 0;
    uptr candidateLevelPtr = 0;
    ULONGLONG candidateWorldTick = 0;
    ULONGLONG candidateLevelTick = 0;
    ULONGLONG debounceMs = 200;
};

struct SceneWatchResult
{
    bool changed = false;
    uptr previousWorldPtr = 0;
    uptr previousLevelPtr = 0;
    uptr currentWorldPtr = 0;
    uptr currentLevelPtr = 0;
};

inline bool IsPlausibleScenePtr(uptr ptr)
{
    if (!ptr || ptr < 0x10000)
    {
        return false;
    }

    uptr modBase = Ctx().mainModule.base;
    uptr modSize = Ctx().mainModule.size;
    if (modBase && modSize && ptr >= modBase && ptr < modBase + modSize)
    {
        return false;
    }

    return true;
}

inline bool QueryScenePointers(const IMemoryAccessor& mem, uptr& outWorld, uptr& outLevel)
{
    outWorld = 0;
    outLevel = 0;

    if (Ctx().off.GWorld == 0)
    {
        return false;
    }

    if (!ReadPtr(mem, Ctx().off.GWorld, outWorld) || !IsPlausibleScenePtr(outWorld))
    {
        outWorld = 0;
        return false;
    }

    if (Off().UWorld_PersistentLevel != -1)
    {
        ReadPtr(mem, outWorld + Off().UWorld_PersistentLevel, outLevel);
        if (!IsPlausibleScenePtr(outLevel))
        {
            outLevel = 0;
        }
    }

    return true;
}

inline SceneWatchResult UpdateSceneWatch(
    SceneWatchState& state,
    uptr currentWorld,
    uptr currentLevel,
    ULONGLONG now = GetTickCount64())
{
    SceneWatchResult result{};
    result.currentWorldPtr = currentWorld;
    result.currentLevelPtr = currentLevel;

    if (!state.lastWorldPtr && currentWorld)
    {
        state.lastWorldPtr = currentWorld;
    }

    if (!state.lastLevelPtr && currentLevel)
    {
        state.lastLevelPtr = currentLevel;
    }

    bool worldChanged = false;
    bool levelChanged = false;

    if (currentWorld && state.lastWorldPtr && currentWorld != state.lastWorldPtr)
    {
        if (currentWorld != state.candidateWorldPtr)
        {
            state.candidateWorldPtr = currentWorld;
            state.candidateWorldTick = now;
        }
        else if (now - state.candidateWorldTick >= state.debounceMs)
        {
            worldChanged = true;
            state.candidateWorldPtr = 0;
            state.candidateWorldTick = 0;
        }
    }
    else
    {
        state.candidateWorldPtr = 0;
        state.candidateWorldTick = 0;
    }

    if (currentLevel && state.lastLevelPtr && currentLevel != state.lastLevelPtr)
    {
        if (currentLevel != state.candidateLevelPtr)
        {
            state.candidateLevelPtr = currentLevel;
            state.candidateLevelTick = now;
        }
        else if (now - state.candidateLevelTick >= state.debounceMs)
        {
            levelChanged = true;
            state.candidateLevelPtr = 0;
            state.candidateLevelTick = 0;
        }
    }
    else
    {
        state.candidateLevelPtr = 0;
        state.candidateLevelTick = 0;
    }

    if (worldChanged || levelChanged)
    {
        result.changed = true;
        result.previousWorldPtr = state.lastWorldPtr;
        result.previousLevelPtr = state.lastLevelPtr;

        if (currentWorld)
        {
            state.lastWorldPtr = currentWorld;
        }

        if (currentLevel)
        {
            state.lastLevelPtr = currentLevel;
        }
    }

    return result;
}

inline SceneWatchResult DetectSceneChange(SceneWatchState& state, const IMemoryAccessor& mem)
{
    uptr currentWorld = 0;
    uptr currentLevel = 0;
    QueryScenePointers(mem, currentWorld, currentLevel);
    return UpdateSceneWatch(state, currentWorld, currentLevel);
}

inline void ResetSceneWatch(SceneWatchState& state)
{
    state = SceneWatchState{};
}
} // namespace xrd
