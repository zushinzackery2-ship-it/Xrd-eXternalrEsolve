#pragma once
// Xrd-eXternalrEsolve - Actor 高层 API 与字段读取
// 从 world.hpp 拆分：Actor 过滤、PlayerController/Pawn、字段便利函数

#include "world.hpp"
#include "objects_search.hpp"

namespace xrd
{

// ─── 高层 Actor API ───

inline std::vector<uptr> GetActorsOfClass(const std::string& className)
{
    std::vector<uptr> result;
    for (uptr actor : GetAllActors())
    {
        if (GetObjectClassName(actor) == className)
        {
            result.push_back(actor);
        }
    }
    return result;
}

inline std::vector<uptr> GetActorsOfClassContains(const std::string& substr)
{
    std::vector<uptr> result;
    for (uptr actor : GetAllActors())
    {
        std::string cn = GetObjectClassName(actor);
        if (cn.find(substr) != std::string::npos)
        {
            result.push_back(actor);
        }
    }
    return result;
}

inline bool IsActorOfClass(uptr actor, const std::string& className)
{
    return IsObjectOfClass(actor, className);
}

inline std::string GetActorFullName(uptr actor)
{
    return GetObjectFullName(actor);
}

// ─── PlayerController / Pawn ───

inline uptr GetPlayerController()
{
    auto& off = Ctx().off;

    // 优先路径：UWorld -> OwningGameInstance -> LocalPlayers[0] -> PlayerController
    if (off.UWorld_OwningGameInstance != -1 &&
        off.UGameInstance_LocalPlayers != -1 &&
        off.ULocalPlayer_PlayerController != -1)
    {
        uptr world = GetUWorld();
        if (world)
        {
            uptr gi = 0;
            if (GReadPtr(world + off.UWorld_OwningGameInstance, gi) && IsCanonicalUserPtr(gi))
            {
                uptr lpData = 0;
                i32 lpCount = 0;
                GReadPtr(gi + off.UGameInstance_LocalPlayers, lpData);
                GReadI32(gi + off.UGameInstance_LocalPlayers + 8, lpCount);

                if (IsCanonicalUserPtr(lpData) && lpCount > 0)
                {
                    uptr lp0 = 0;
                    if (GReadPtr(lpData, lp0) && IsCanonicalUserPtr(lp0))
                    {
                        uptr pc = 0;
                        if (GReadPtr(lp0 + off.ULocalPlayer_PlayerController, pc) && IsCanonicalUserPtr(pc))
                        {
                            return pc;
                        }
                    }
                }
            }
        }
    }

    // 回退：遍历 Actor 列表
    for (uptr actor : GetAllActors())
    {
        if (IsObjectOfClass(actor, "PlayerController"))
        {
            return actor;
        }
    }
    return 0;
}

inline uptr GetAPawn()
{
    uptr pc = GetPlayerController();
    if (!pc)
    {
        return 0;
    }

    auto& off = Ctx().off;

    // AutoInit 已缓存 Pawn 偏移，直接使用
    // 若 AutoInit 未能缓存（-1），则通过反射延迟发现
    if (off.APlayerController_Pawn == -1)
    {
        uptr pcClass = GetObjectClass(pc);
        if (pcClass)
        {
            i32 pawnOff = GetPropertyOffsetByName(pcClass, "Pawn");
            if (pawnOff == -1)
                pawnOff = GetPropertyOffsetByName(pcClass, "AcknowledgedPawn");
            Ctx().off.APlayerController_Pawn = pawnOff;
        }
    }

    if (off.APlayerController_Pawn != -1)
    {
        uptr pawn = 0;
        GReadPtr(pc + off.APlayerController_Pawn, pawn);
        return pawn;
    }

    return 0;
}

// ─── 字段读取便利函数 ───

inline uptr ReadActorFieldPtr(uptr actor, const std::string& fieldName)
{
    if (!actor)
    {
        return 0;
    }
    uptr cls = GetObjectClass(actor);
    if (!cls)
    {
        return 0;
    }
    i32 offset = GetPropertyOffsetByName(cls, fieldName);
    if (offset == -1)
    {
        return 0;
    }
    uptr val = 0;
    GReadPtr(actor + offset, val);
    return val;
}

inline i32 ReadActorFieldInt32(uptr actor, const std::string& fieldName)
{
    if (!actor)
    {
        return 0;
    }
    uptr cls = GetObjectClass(actor);
    if (!cls)
    {
        return 0;
    }
    i32 offset = GetPropertyOffsetByName(cls, fieldName);
    if (offset == -1)
    {
        return 0;
    }
    i32 val = 0;
    GReadI32(actor + offset, val);
    return val;
}

inline float ReadActorFieldFloat(uptr actor, const std::string& fieldName)
{
    if (!actor)
    {
        return 0.0f;
    }
    uptr cls = GetObjectClass(actor);
    if (!cls)
    {
        return 0.0f;
    }
    i32 offset = GetPropertyOffsetByName(cls, fieldName);
    if (offset == -1)
    {
        return 0.0f;
    }
    float val = 0.0f;
    GReadValue(actor + offset, val);
    return val;
}

} // namespace xrd
