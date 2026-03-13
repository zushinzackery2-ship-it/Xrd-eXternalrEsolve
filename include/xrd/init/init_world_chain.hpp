#pragma once
// Xrd-eXternalrEsolve - World 链偏移反射发现
// UWorld -> OwningGameInstance -> LocalPlayers[0] -> PlayerController -> Pawn
// 依赖 engine/objects.hpp 和 engine/objects_search.hpp（由 xrd.hpp 保证先包含）

#include "../core/context.hpp"
#include <iostream>
#include <format>

namespace xrd
{

// 通过反射遍历 World 链，自动发现各节点偏移
inline void DiscoverWorldChainOffsets(Context& ctx)
{
    auto& mem = *ctx.mem;
    auto& off = ctx.off;

    if (!off.GWorld)
    {
        return;
    }

    uptr world = 0;
    if (!ReadPtr(mem, off.GWorld, world) || !IsCanonicalUserPtr(world))
    {
        return;
    }

    uptr worldClass = GetObjectClass(world);
    if (worldClass)
    {
        if (off.UWorld_OwningGameInstance == -1)
            off.UWorld_OwningGameInstance = GetPropertyOffsetByName(worldClass, "OwningGameInstance");
        if (off.UWorld_PersistentLevel == -1)
            off.UWorld_PersistentLevel = GetPropertyOffsetByName(worldClass, "PersistentLevel");
        if (off.UWorld_Levels == -1)
            off.UWorld_Levels = GetPropertyOffsetByName(worldClass, "Levels");
    }

    // GameInstance -> LocalPlayers
    if (off.UWorld_OwningGameInstance != -1)
    {
        uptr gi = 0;
        if (ReadPtr(mem, world + off.UWorld_OwningGameInstance, gi) && IsCanonicalUserPtr(gi))
        {
            uptr giClass = GetObjectClass(gi);
            if (giClass && off.UGameInstance_LocalPlayers == -1)
                off.UGameInstance_LocalPlayers = GetPropertyOffsetByName(giClass, "LocalPlayers");

            // LocalPlayers[0] -> PlayerController
            if (off.UGameInstance_LocalPlayers != -1)
            {
                uptr lpData = 0;
                i32 lpCount = 0;
                ReadPtr(mem, gi + off.UGameInstance_LocalPlayers, lpData);
                ReadI32(mem, gi + off.UGameInstance_LocalPlayers + 8, lpCount);

                if (IsCanonicalUserPtr(lpData) && lpCount > 0)
                {
                    uptr lp0 = 0;
                    if (ReadPtr(mem, lpData, lp0) && IsCanonicalUserPtr(lp0))
                    {
                        uptr lpClass = GetObjectClass(lp0);
                        if (lpClass && off.ULocalPlayer_PlayerController == -1)
                            off.ULocalPlayer_PlayerController = GetPropertyOffsetByName(lpClass, "PlayerController");

                        // PlayerController -> Pawn / PlayerCameraManager
                        if (off.ULocalPlayer_PlayerController != -1)
                        {
                            uptr pc = 0;
                            if (ReadPtr(mem, lp0 + off.ULocalPlayer_PlayerController, pc) && IsCanonicalUserPtr(pc))
                            {
                                uptr pcClass = GetObjectClass(pc);
                                if (pcClass)
                                {
                                    if (off.APlayerController_Pawn == -1)
                                    {
                                        i32 pawnOff = GetPropertyOffsetByName(pcClass, "Pawn");
                                        if (pawnOff == -1)
                                            pawnOff = GetPropertyOffsetByName(pcClass, "AcknowledgedPawn");
                                        off.APlayerController_Pawn = pawnOff;
                                    }
                                    if (off.APlayerController_PlayerCameraManager == -1)
                                        off.APlayerController_PlayerCameraManager = GetPropertyOffsetByName(pcClass, "PlayerCameraManager");
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::cerr << "[xrd] World链偏移:\n";
    std::cerr << std::format("  OwningGameInstance:  +0x{:X}\n", off.UWorld_OwningGameInstance);
    std::cerr << std::format("  PersistentLevel:     +0x{:X}\n", off.UWorld_PersistentLevel);
    std::cerr << std::format("  Levels:              +0x{:X}\n", off.UWorld_Levels);
    std::cerr << std::format("  LocalPlayers:        +0x{:X}\n", off.UGameInstance_LocalPlayers);
    std::cerr << std::format("  PlayerController:    +0x{:X}\n", off.ULocalPlayer_PlayerController);
    std::cerr << std::format("  Pawn:                +0x{:X}\n", off.APlayerController_Pawn);
    std::cerr << std::format("  PlayerCameraManager: +0x{:X}\n", off.APlayerController_PlayerCameraManager);
}

} // namespace xrd
