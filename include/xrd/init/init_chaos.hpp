#pragma once
// Xrd-eXternalrEsolve - Chaos 偏移反射发现 + 扫描
// 依赖 engine/objects.hpp 和 engine/objects_search.hpp（由 xrd.hpp 保证先包含）

#include "../core/context.hpp"
#include "../chaos/chaos_reflection.hpp"
#include "init_chaos_scan.hpp"
#include <iostream>
#include <format>
#include <vector>

namespace xrd
{

inline void InitChaosOffsets(Context& ctx)
{
    auto& mem = *ctx.mem;
    auto& off = ctx.off;
    auto& co  = ctx.chaosOff;

    std::cerr << "[xrd][Chaos] === 开始偏移发现 ===\n";

    uptr world = 0;
    if (!ReadPtr(mem, off.GWorld, world) || !IsCanonicalUserPtr(world))
    {
        std::cerr << "[xrd][Chaos] 无法读取 GWorld\n";
        return;
    }

    detail::DiscoverChaosSdkOffsets(ctx);

    std::vector<uptr> actorPtrs;
    bool actorPtrsLoaded = false;
    auto ensureActorPtrs = [&]() -> const std::vector<uptr>&
    {
        if (!actorPtrsLoaded)
        {
            actorPtrs = GetAllActors();
            actorPtrsLoaded = true;
        }
        return actorPtrs;
    };

    detail::DiscoverChaosSdkOffsetsByActorWalk(ctx, ensureActorPtrs());

    // ── 2. 扫描 UWorld -> FPhysScene_Chaos ──
    // 非反射成员，需要遍历 UWorld 内部指针做双重回指验证
    if (co.UWorld_PhysScene < 0)
    {
        std::cerr << "[xrd][Chaos] 扫描 UWorld -> FPhysScene_Chaos ...\n";
        for (i32 testOff = 0x100; testOff < 0x800; testOff += 8)
        {
            uptr candidate = 0;
            if (!ReadPtr(mem, world + testOff, candidate) || !IsCanonicalUserPtr(candidate))
            {
                continue;
            }

            uptr backRef = 0;
            if (!ReadPtr(mem, candidate + co.PhysScene_OwnerWorld, backRef) || backRef != world)
            {
                continue;
            }

            uptr solver = 0;
            if (!ReadPtr(mem, candidate + co.PhysScene_Solver, solver) || !IsCanonicalUserPtr(solver))
            {
                continue;
            }

            uptr solverBackRef = 0;
            if (!ReadPtr(mem, solver + co.Solver_PhysScene, solverBackRef) ||
                solverBackRef != candidate)
            {
                continue;
            }

            co.UWorld_PhysScene = testOff;
            off.ChaosPhysScene = candidate;
            std::cerr << std::format(
                "[xrd][Chaos] FPhysScene_Chaos 找到: UWorld+0x{:X} = 0x{:X}\n",
                testOff,
                candidate);
            break;
        }

        if (co.UWorld_PhysScene < 0)
        {
            std::cerr << "[xrd][Chaos] FPhysScene_Chaos 未找到（碰撞场景链不可用）\n";
        }
    }

    detail::ScanChaosPhysicsProxyByActors(ctx, ensureActorPtrs());

    std::cerr << "[xrd][Chaos] === 偏移发现完成 ===\n";
}

} // namespace xrd
