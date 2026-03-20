#pragma once
// Xrd-eXternalrEsolve - Chaos 初始化辅助扫描
// 从 init_chaos.hpp 拆分：Actor-walk 兜底与 PhysicsProxy 扫描。

#include "../core/context.hpp"
#include "../engine/world/world_levels.hpp"
#include <cmath>
#include <format>
#include <iostream>
#include <vector>

namespace xrd
{
namespace detail
{

inline void DiscoverChaosSdkOffsetsByActorWalk(
    Context& ctx,
    const std::vector<uptr>& actorPtrs)
{
    auto& mem = *ctx.mem;
    auto& off = ctx.off;
    auto& co  = ctx.chaosOff;

    if (!(co.PrimComp_BodyInstance < 0 ||
          co.BodySetup_AggGeom < 0 ||
          co.BodyInstance_StructSize < 0))
    {
        return;
    }

    bool needBI = (co.PrimComp_BodyInstance < 0);
    bool needAG = (co.BodySetup_AggGeom < 0);
    bool needSZ = (co.BodyInstance_StructSize < 0);

    i32 checkCount = static_cast<i32>(actorPtrs.size());
    if (checkCount > 300)
    {
        checkCount = 300;
    }

    for (i32 i = 0; i < checkCount; ++i)
    {
        uptr actorPtr = actorPtrs[static_cast<std::size_t>(i)];
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
                    "[xrd][Chaos] PrimComp::BodyInstance = +0x{:X} (actor-walk)\n",
                    biOff);
            }
        }

        if (needAG && co.PrimComp_BodyInstance >= 0 && co.BodyInstance_BodySetup >= 0)
        {
            i32 bodySetupWeakIndex = 0;
            if (ReadValue(
                mem,
                rootComp + co.PrimComp_BodyInstance + co.BodyInstance_BodySetup,
                bodySetupWeakIndex) &&
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
                                "[xrd][Chaos] BodySetup::AggGeom = +0x{:X} (actor-walk)\n",
                                aggOff);
                        }
                    }
                }
            }
        }

        if (needSZ && co.PrimComp_BodyInstance >= 0 &&
            off.StructProperty_Struct >= 0 && off.UStruct_Size >= 0)
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
                            "[xrd][Chaos] FBodyInstance size = 0x{:X} (actor-walk)\n",
                            structSize);
                    }
                }
            }
        }

        if (!needBI && !needAG && !needSZ)
        {
            break;
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

inline void ScanChaosPhysicsProxyByActors(
    Context& ctx,
    const std::vector<uptr>& actorPtrs)
{
    auto& mem = *ctx.mem;
    auto& co = ctx.chaosOff;

    if (!(co.BodyInstance_PhysicsProxy < 0 &&
          co.BodyInstance_StructSize > 0 &&
          co.PrimComp_BodyInstance >= 0))
    {
        return;
    }

    std::cerr << "[xrd][Chaos] 扫描 FBodyInstance 私有物理句柄 ...\n";

    i32 checkCount = static_cast<i32>(actorPtrs.size());
    if (checkCount > 200)
    {
        checkCount = 200;
    }

    bool found = false;
    for (i32 i = 0; i < checkCount; ++i)
    {
        if (found)
        {
            break;
        }

        uptr actorPtr = actorPtrs[static_cast<std::size_t>(i)];
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

        i32 biTestOff = GetPropertyOffsetByName(compClass, "BodyInstance");
        if (biTestOff < 0)
        {
            continue;
        }

        uptr bodyInstanceAddr = rootComp + biTestOff;

        i32 scanStart = co.BodyInstance_StructSize - 0x40;
        if (scanStart < 0x20)
        {
            scanStart = 0x20;
        }
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

            double px = 0;
            double py = 0;
            double pz = 0;
            ReadValue(mem, particle + co.Particle_PosX, px);
            ReadValue(mem, particle + co.Particle_PosY, py);
            ReadValue(mem, particle + co.Particle_PosZ, pz);

            if (std::abs(px) < 1e8 && std::abs(py) < 1e8 && std::abs(pz) < 1e8 &&
                (std::abs(px) > 1e-6 || std::abs(py) > 1e-6 || std::abs(pz) > 1e-6))
            {
                co.BodyInstance_PhysicsProxy = probeOff;
                std::cerr << std::format(
                    "[xrd][Chaos] FBodyInstance::PhysicsProxy = +0x{:X} (Pos: {:.1f}, {:.1f}, {:.1f})\n",
                    probeOff,
                    px,
                    py,
                    pz);
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

} // namespace detail
} // namespace xrd
