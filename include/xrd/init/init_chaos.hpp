#pragma once
// Xrd-eXternalrEsolve - Chaos 偏移反射发现 + 扫描
// 依赖 engine/objects.hpp 和 engine/objects_search.hpp（由 xrd.hpp 保证先包含）

#include "../core/context.hpp"
#include <iostream>
#include <format>
#include <cmath>
#include <vector>
#include "../engine/world/world.hpp"

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

    // ── 1. 通过 actor-walking 发现 SDK 偏移 ──
    // 和 GetAPawn 同方式：从已知 Actor 出发遍历组件，不用 FindClassByName
    // 避免全量对象遍历对 FNamePool 名称解析的依赖
    if (co.PrimComp_BodyInstance < 0 || co.BodySetup_AggGeom < 0 || co.BodyInstance_StructSize < 0)
    {
        bool needBI = (co.PrimComp_BodyInstance < 0);
        bool needAG = (co.BodySetup_AggGeom < 0);
        bool needSZ = (co.BodyInstance_StructSize < 0);

        ActorArray actorArray{};
        if (GetPersistentLevelActors(actorArray) && IsCanonicalUserPtr(actorArray.data) && actorArray.count > 0)
        {
            i32 checkCount = (actorArray.count < 300) ? actorArray.count : 300;
            std::vector<uptr> actorPtrs(checkCount);
            if (mem.Read(actorArray.data, actorPtrs.data(), checkCount * sizeof(uptr)))
            {
                for (uptr actorPtr : actorPtrs)
                {
                    if (!IsCanonicalUserPtr(actorPtr))
                    {
                        continue;
                    }
                    uptr actorClass = GetObjectClass(actorPtr);
                    if (!actorClass)
                    {
                        continue;
                    }
                    i32 rootCompOff = GetPropertyOffsetByName(actorClass, "RootComponent");
                    if (rootCompOff < 0)
                    {
                        continue;
                    }
                    uptr rootComp = 0;
                    if (!ReadPtr(mem, actorPtr + rootCompOff, rootComp) || !IsCanonicalUserPtr(rootComp))
                    {
                        continue;
                    }
                    uptr compClass = GetObjectClass(rootComp);
                    if (!compClass)
                    {
                        continue;
                    }

                    if (needBI)
                    {
                        i32 biOff = GetPropertyOffsetByName(compClass, "BodyInstance");
                        if (biOff >= 0)
                        {
                            co.PrimComp_BodyInstance = biOff;
                            needBI = false;
                            std::cerr << std::format(
                                "[xrd][Chaos] PrimComp::BodyInstance = +0x{:X} (actor-walk)\n", biOff);
                        }
                    }

                    if (needAG && co.PrimComp_BodyInstance >= 0)
                    {
                        i32 bodySetupWeakIndex = 0;
                        if (ReadValue(mem, rootComp + co.PrimComp_BodyInstance + 0x08, bodySetupWeakIndex) &&
                            bodySetupWeakIndex > 0)
                        {
                            uptr bodySetup = GetObjectByIndex(bodySetupWeakIndex);
                            if (IsCanonicalUserPtr(bodySetup))
                            {
                                uptr bsClass = GetObjectClass(bodySetup);
                                if (bsClass)
                                {
                                    i32 aggOff = GetPropertyOffsetByName(bsClass, "AggGeom");
                                    if (aggOff >= 0)
                                    {
                                        co.BodySetup_AggGeom = aggOff;
                                        needAG = false;
                                        std::cerr << std::format(
                                            "[xrd][Chaos] BodySetup::AggGeom = +0x{:X} (actor-walk)\n", aggOff);
                                    }
                                }
                            }
                        }
                    }

                    if (needSZ && co.PrimComp_BodyInstance >= 0 && off.StructProperty_Struct >= 0 && off.UStruct_Size >= 0)
                    {
                        uptr biProp = 0;
                        uptr cur = compClass;
                        while (cur && !biProp)
                        {
                            biProp = FindPropertyInStruct(cur, "BodyInstance");
                            if (!biProp)
                            {
                                cur = GetSuperStruct(cur);
                            }
                        }
                        if (biProp)
                        {
                            uptr structPtr = 0;
                            ReadPtr(mem, biProp + off.StructProperty_Struct, structPtr);
                            if (IsCanonicalUserPtr(structPtr))
                            {
                                i32 structSize = 0;
                                ReadValue(mem, structPtr + off.UStruct_Size, structSize);
                                if (structSize > 0 && structSize < 0x1000)
                                {
                                    co.BodyInstance_StructSize = structSize;
                                    needSZ = false;
                                    std::cerr << std::format(
                                        "[xrd][Chaos] FBodyInstance size = 0x{:X} (actor-walk)\n", structSize);
                                }
                            }
                        }
                    }

                    if (!needBI && !needAG && !needSZ)
                    {
                        break;
                    }
                }
            }
        }

        if (needBI)
        {
            std::cerr << "[xrd][Chaos] PrimComp::BodyInstance 未找到\n";
        }
        if (needAG)
        {
            std::cerr << "[xrd][Chaos] BodySetup::AggGeom 未找到\n";
        }
        if (needSZ)
        {
            std::cerr << "[xrd][Chaos] FBodyInstance 结构大小未知\n";
        }
    }

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

            // 验证 1: candidate -> OwnerWorld 应该回指 UWorld
            uptr backRef = 0;
            if (!ReadPtr(mem, candidate + co.PhysScene_OwnerWorld, backRef) || backRef != world)
            {
                continue;
            }

            // 验证 2: candidate -> Solver -> PhysScene 应该回指 candidate
            uptr solver = 0;
            if (!ReadPtr(mem, candidate + co.PhysScene_Solver, solver) || !IsCanonicalUserPtr(solver))
            {
                continue;
            }
            uptr solverBackRef = 0;
            if (!ReadPtr(mem, solver + co.Solver_PhysScene, solverBackRef) || solverBackRef != candidate)
            {
                continue;
            }

            co.UWorld_PhysScene = testOff;
            off.ChaosPhysScene = candidate;
            std::cerr << std::format("[xrd][Chaos] FPhysScene_Chaos 找到: UWorld+0x{:X} = 0x{:X}\n",
                                     testOff, candidate);
            break;
        }

        if (co.UWorld_PhysScene < 0)
        {
            std::cerr << "[xrd][Chaos] FPhysScene_Chaos 未找到（碰撞场景链不可用）\n";
        }
    }

    // ── 3. 扫描 FBodyInstance 尾部私有物理句柄 ──
    if (co.BodyInstance_PhysicsProxy < 0 && co.BodyInstance_StructSize > 0)
    {
        std::cerr << "[xrd][Chaos] 扫描 FBodyInstance 私有物理句柄 ...\n";

        if (off.UWorld_PersistentLevel >= 0 && co.PrimComp_BodyInstance >= 0)
        {
            uptr level = 0;
            ReadPtr(mem, world + off.UWorld_PersistentLevel, level);

            if (IsCanonicalUserPtr(level) && off.ULevel_Actors >= 0)
            {
                uptr actorsData = 0;
                i32 actorsCount = 0;
                ReadPtr(mem, level + off.ULevel_Actors, actorsData);
                ReadValue(mem, level + off.ULevel_Actors + 8, actorsCount);

                if (IsCanonicalUserPtr(actorsData) && actorsCount > 0)
                {
                    i32 checkCount = (actorsCount < 200) ? actorsCount : 200;
                    std::vector<uptr> actorPtrs(checkCount);
                    mem.Read(actorsData, actorPtrs.data(), checkCount * sizeof(uptr));

                    bool found = false;
                    for (uptr actorPtr : actorPtrs)
                    {
                        if (found) break;
                        if (!IsCanonicalUserPtr(actorPtr)) continue;

                        uptr actorClass = GetObjectClass(actorPtr);
                        if (!actorClass) continue;

                        i32 rootCompOff = GetPropertyOffsetByName(actorClass, "RootComponent");
                        if (rootCompOff < 0) continue;

                        uptr rootComp = 0;
                        if (!ReadPtr(mem, actorPtr + rootCompOff, rootComp) || !IsCanonicalUserPtr(rootComp))
                        {
                            continue;
                        }

                        uptr compClass = GetObjectClass(rootComp);
                        if (!compClass) continue;

                        i32 biTestOff = GetPropertyOffsetByName(compClass, "BodyInstance");
                        if (biTestOff < 0) continue;

                        uptr bodyInstanceAddr = rootComp + biTestOff;

                        i32 scanStart = co.BodyInstance_StructSize - 0x40;
                        if (scanStart < 0x20) scanStart = 0x20;
                        i32 scanEnd = co.BodyInstance_StructSize;

                        for (i32 probeOff = scanStart; probeOff < scanEnd; probeOff += 8)
                        {
                            uptr proxyCandidate = 0;
                            if (!ReadPtr(mem, bodyInstanceAddr + probeOff, proxyCandidate) ||
                                !IsCanonicalUserPtr(proxyCandidate))
                            {
                                continue;
                            }

                            uptr particle = 0;
                            if (!ReadPtr(mem, proxyCandidate + co.Proxy_Particle, particle) ||
                                !IsCanonicalUserPtr(particle))
                            {
                                continue;
                            }

                            double px = 0, py = 0, pz = 0;
                            ReadValue(mem, particle + co.Particle_PosX, px);
                            ReadValue(mem, particle + co.Particle_PosY, py);
                            ReadValue(mem, particle + co.Particle_PosZ, pz);

                            if (std::abs(px) < 1e8 && std::abs(py) < 1e8 && std::abs(pz) < 1e8 &&
                                (std::abs(px) > 1e-6 || std::abs(py) > 1e-6 || std::abs(pz) > 1e-6))
                            {
                                co.BodyInstance_PhysicsProxy = probeOff;
                                std::cerr << std::format(
                                    "[xrd][Chaos] FBodyInstance::PhysicsProxy = +0x{:X} (Pos: {:.1f}, {:.1f}, {:.1f})\n",
                                    probeOff, px, py, pz);
                                found = true;
                                break;
                            }
                        }
                    }

                    if (!found)
                    {
                        std::cerr << "[xrd][Chaos] FBodyInstance 私有句柄未找到\n";
                    }
                }
            }
        }
    }

    std::cerr << "[xrd][Chaos] === 偏移发现完成 ===\n";
}

} // namespace xrd
