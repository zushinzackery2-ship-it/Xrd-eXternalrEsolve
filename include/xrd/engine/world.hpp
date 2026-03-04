#pragma once
// Xrd-eXternalrEsolve - UWorld / Actor / Pawn 访问
// 游戏世界遍历的高层 API

#include "../core/context.hpp"
#include "../resolve/scan_world.hpp"
#include "../resolve/fvector_detect.hpp"
#include "objects.hpp"
#include "objects_search.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

namespace xrd
{

// ─── GWorld 延迟扫描 ───

namespace detail
{
    inline std::mutex& GWorldResolveMutex()
    {
        static std::mutex m;
        return m;
    }

    // 延迟发现 World 链偏移（与 AutoInit Phase 6 相同逻辑）
    inline void DiscoverWorldChainOffsets()
    {
        auto& ctx = Ctx();
        uptr world = 0;
        if (!ReadPtr(*ctx.mem, ctx.off.GWorld, world) || !IsCanonicalUserPtr(world))
        {
            return;
        }

        uptr worldClass = GetObjectClass(world);
        if (!worldClass)
        {
            return;
        }

        if (ctx.off.UWorld_OwningGameInstance == -1)
            ctx.off.UWorld_OwningGameInstance = GetPropertyOffsetByName(worldClass, "OwningGameInstance");
        if (ctx.off.UWorld_PersistentLevel == -1)
            ctx.off.UWorld_PersistentLevel = GetPropertyOffsetByName(worldClass, "PersistentLevel");

        if (ctx.off.UWorld_OwningGameInstance == -1)
        {
            return;
        }

        uptr gi = 0;
        if (!ReadPtr(*ctx.mem, world + ctx.off.UWorld_OwningGameInstance, gi) || !IsCanonicalUserPtr(gi))
        {
            return;
        }

        uptr giClass = GetObjectClass(gi);
        if (giClass && ctx.off.UGameInstance_LocalPlayers == -1)
            ctx.off.UGameInstance_LocalPlayers = GetPropertyOffsetByName(giClass, "LocalPlayers");

        if (ctx.off.UGameInstance_LocalPlayers == -1)
        {
            return;
        }

        uptr lpData = 0;
        i32 lpCount = 0;
        ReadPtr(*ctx.mem, gi + ctx.off.UGameInstance_LocalPlayers, lpData);
        ReadI32(*ctx.mem, gi + ctx.off.UGameInstance_LocalPlayers + 8, lpCount);
        if (!IsCanonicalUserPtr(lpData) || lpCount <= 0)
        {
            return;
        }

        uptr lp0 = 0;
        if (!ReadPtr(*ctx.mem, lpData, lp0) || !IsCanonicalUserPtr(lp0))
        {
            return;
        }

        uptr lpClass = GetObjectClass(lp0);
        if (lpClass && ctx.off.ULocalPlayer_PlayerController == -1)
            ctx.off.ULocalPlayer_PlayerController = GetPropertyOffsetByName(lpClass, "PlayerController");

        if (ctx.off.ULocalPlayer_PlayerController == -1)
        {
            return;
        }

        uptr pc = 0;
        if (!ReadPtr(*ctx.mem, lp0 + ctx.off.ULocalPlayer_PlayerController, pc) || !IsCanonicalUserPtr(pc))
        {
            return;
        }

        uptr pcClass = GetObjectClass(pc);
        if (!pcClass)
        {
            return;
        }

        if (ctx.off.APlayerController_Pawn == -1)
        {
            i32 pawnOff = GetPropertyOffsetByName(pcClass, "Pawn");
            if (pawnOff == -1)
                pawnOff = GetPropertyOffsetByName(pcClass, "AcknowledgedPawn");
            ctx.off.APlayerController_Pawn = pawnOff;
        }
        if (ctx.off.APlayerController_PlayerCameraManager == -1)
            ctx.off.APlayerController_PlayerCameraManager = GetPropertyOffsetByName(pcClass, "PlayerCameraManager");

        std::cerr << "[xrd] World链偏移（延迟发现）:\n";
        std::cerr << "  PersistentLevel: +0x" << std::hex << ctx.off.UWorld_PersistentLevel << std::dec << "\n";
        std::cerr << "  Pawn:            +0x" << std::hex << ctx.off.APlayerController_Pawn << std::dec << "\n";
    }

    // 尝试延迟扫描 GWorld（线程安全，带 3 秒冷却）
    inline bool TryLazyResolveGWorld()
    {
        using clock = std::chrono::steady_clock;
        static auto s_lastAttempt = clock::time_point{};

        // 无锁快速检查冷却，避免频繁加锁
        auto now = clock::now();
        if (now - s_lastAttempt < std::chrono::seconds(3))
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(GWorldResolveMutex());
        auto& ctx = Ctx();

        // 双检查：另一个线程可能已经解析成功
        if (ctx.off.GWorld != 0)
        {
            return true;
        }

        // 双检查冷却（持锁后再验一次）
        now = clock::now();
        if (now - s_lastAttempt < std::chrono::seconds(3))
        {
            return false;
        }
        s_lastAttempt = now;

        std::cerr << "[xrd] 尝试延迟扫描 GWorld...\n";

        uptr gworld = 0;
        if (!resolve::ScanGWorld(ctx.sections, *ctx.mem, ctx.off, gworld))
        {
            return false;
        }

        ctx.off.GWorld = gworld;

        // 补充 FVector 精度检测
        bool isDouble = false;
        resolve::DetectDoublePrecision(*ctx.mem, ctx.off, isDouble);
        ctx.off.bUseDoublePrecision = isDouble;
        std::cerr << "[xrd] 延迟精度检测: "
                  << (isDouble ? "double (UE5)" : "float (UE4)") << "\n";

        // 补充 World 链偏移发现
        DiscoverWorldChainOffsets();
        return true;
    }
} // namespace detail

// ─── UWorld 访问 ───

inline uptr GetUWorld()
{
    if (!IsInited())
    {
        return 0;
    }

    // GWorld 为 0 时尝试延迟扫描
    if (Off().GWorld == 0)
    {
        if (!detail::TryLazyResolveGWorld())
        {
            return 0;
        }
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

    // 一次性批量读取整个 Actor 指针数组，避免 N 次 RPM
    std::vector<uptr> rawPtrs(arr.count);
    if (!Mem().Read(arr.data, rawPtrs.data(), static_cast<std::size_t>(arr.count) * sizeof(uptr)))
    {
        return result;
    }

    result.reserve(arr.count);
    for (i32 i = 0; i < arr.count; ++i)
    {
        if (IsCanonicalUserPtr(rawPtrs[i]))
        {
            result.push_back(rawPtrs[i]);
        }
    }
    return result;
}

} // namespace xrd
