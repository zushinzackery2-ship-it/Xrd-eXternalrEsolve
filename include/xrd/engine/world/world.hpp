#pragma once
// Xrd-eXternalrEsolve - UWorld / Actor / Pawn 访问
// 游戏世界遍历的高层 API

#include "../../core/context.hpp"
#include "../../resolve/globals/scan_world.hpp"
#include "../objects/objects.hpp"
#include "../objects/objects_search.hpp"
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
        if (ctx.off.UWorld_Levels == -1)
            ctx.off.UWorld_Levels = GetPropertyOffsetByName(worldClass, "Levels");

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
        {
            std::cerr << "[xrd] ULocalPlayer类: 0x" << std::hex << lpClass << std::dec << "\n";
            std::cerr << "[xrd] 尝试反射查找PlayerController属性...\n";
            ctx.off.ULocalPlayer_PlayerController = GetPropertyOffsetByName(lpClass, "PlayerController");
            std::cerr << "[xrd] PlayerController偏移: " << ctx.off.ULocalPlayer_PlayerController << "\n";
        }

        if (ctx.off.ULocalPlayer_PlayerController == -1)
        {
            std::cerr << "[xrd] ERROR: 无法找到ULocalPlayer::PlayerController属性\n";
            return;
        }

        uptr pc = 0;
        if (!ReadPtr(*ctx.mem, lp0 + ctx.off.ULocalPlayer_PlayerController, pc) || !IsCanonicalUserPtr(pc))
        {
            std::cerr << "[xrd] PlayerController实例无效: pc=0x" << std::hex << pc << std::dec << "\n";
            return;
        }

        std::cerr << "[xrd] PlayerController实例: 0x" << std::hex << pc << std::dec << "\n";

        uptr pcClass = GetObjectClass(pc);
        if (!pcClass)
        {
            std::cerr << "[xrd] 无法获取PlayerController类\n";
            return;
        }

        std::cerr << "[xrd] PlayerController类: 0x" << std::hex << pcClass << std::dec << "\n";

        if (ctx.off.APlayerController_Pawn == -1)
        {
            std::cerr << "[xrd] 尝试反射查找Pawn属性...\n";
            i32 pawnOff = GetPropertyOffsetByName(pcClass, "Pawn");
            std::cerr << "[xrd] Pawn偏移: " << pawnOff << "\n";
            if (pawnOff == -1)
            {
                std::cerr << "[xrd] 尝试反射查找AcknowledgedPawn属性...\n";
                pawnOff = GetPropertyOffsetByName(pcClass, "AcknowledgedPawn");
                std::cerr << "[xrd] AcknowledgedPawn偏移: " << pawnOff << "\n";
            }
            ctx.off.APlayerController_Pawn = pawnOff;
        }
        if (ctx.off.APlayerController_PlayerCameraManager == -1)
        {
            std::cerr << "[xrd] 尝试反射查找PlayerCameraManager属性...\n";
            i32 camOff = GetPropertyOffsetByName(pcClass, "PlayerCameraManager");
            std::cerr << "[xrd] PlayerCameraManager偏移: " << camOff << "\n";
            ctx.off.APlayerController_PlayerCameraManager = camOff;
        }

        std::cerr << "[xrd] World链偏移（延迟发现）:\n";
        std::cerr << "  PersistentLevel: +0x" << std::hex << ctx.off.UWorld_PersistentLevel << std::dec << "\n";
        std::cerr << "  Levels:          +0x" << std::hex << ctx.off.UWorld_Levels << std::dec << "\n";
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

} // namespace xrd
