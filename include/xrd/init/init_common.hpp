#pragma once
// Xrd-eXternalrEsolve - 公共扫描与偏移发现
// AutoInit / AutoInitDriver / AutoInitSharedMem 共用逻辑
// 依赖 resolve/ 和 engine/ 模块（由 xrd.hpp 保证先包含）

#include "../core/context.hpp"
#include "../core/process.hpp"
#include "../core/process_sections.hpp"
#include "../physx/physx_pe.hpp"
#include "../engine/world/world_levels.hpp"
#include "init_chaos.hpp"
#include "init_world_chain.hpp"
#include <iostream>
#include <format>

namespace xrd
{
namespace detail
{

// 查找目标进程 PID（三个 AutoInit 变体共用）
inline u32 FindTargetProcess(const wchar_t* processName)
{
    u32 pid = 0;

    if (processName && processName[0] != L'\0')
    {
        pid = FindProcessId(processName);
    }
    else
    {
        // 优先通过窗口类名查找（UE 窗口类名为 "UnrealWindow"）
        pid = FindProcessByWindowClass(L"UnrealWindow");
        if (pid == 0)
        {
            // 回退到模块枚举方式
            pid = FindUnrealProcessId();
        }
    }

    if (pid == 0)
    {
        std::cerr << "[xrd] 进程未找到\n";
    }
    else
    {
        std::cerr << "[xrd] PID: " << pid << "\n";
    }

    return pid;
}

// 打印初始化摘要
inline void PrintInitSummary()
{
    auto& ctx = Ctx();
    uptr moduleBase = ctx.mainModule.base;

    std::cerr << "========================================\n";
    std::cerr << "Important Static Offsets:\n";
    std::cerr << "========================================\n";
    std::cerr << std::format("GObjects:           0x{:08X}\n",
        ctx.off.GObjects ? ctx.off.GObjects - moduleBase : 0);
    std::cerr << std::format("GNames:             0x{:08X}\n",
        ctx.off.GNames ? ctx.off.GNames - moduleBase : 0);
    std::cerr << std::format("GWorld:             0x{:08X}\n",
        ctx.off.GWorld ? ctx.off.GWorld - moduleBase : 0);
    std::cerr << std::format("ProcessEvent:       0x{:08X}\n",
        ctx.off.ProcessEvent_Addr);
    std::cerr << std::format("ProcessEventIdx:    {}\n",
        ctx.off.ProcessEvent_VTableIndex);
    std::cerr << std::format("AppendString:       0x{:08X}\n",
        ctx.off.AppendNameToString);
    std::cerr << "========================================\n";
    std::cerr << "ChunkSize:          "
              << ctx.off.ChunkSize << "\n";
    std::cerr << "FUObjectItemSize:   "
              << ctx.off.FUObjectItemSize << "\n";
    std::cerr << "========================================\n";
    std::cerr << "[xrd] 精度: "
              << (ctx.off.bUseDoublePrecision ? "double (UE5)" : "float (UE4)") << "\n";
    std::cerr << "[xrd] 对象数: "
              << resolve::GetObjectCount(*ctx.mem, ctx.off) << "\n";
}

inline bool ValidateCriticalValues(bool includeWorldChain = true);

inline bool EnsureSectionCacheReady(Context& ctx)
{
    if (!ctx.sections.empty())
    {
        const SectionCache* textSection = FindSection(ctx.sections, ".text");
        const SectionCache* dataSection = FindSection(ctx.sections, ".data");

        if (textSection != nullptr && dataSection != nullptr)
        {
            return true;
        }

        // 首轮缓存如果缺了关键段，下一轮强制重读一次，避免把坏缓存一直复用下去。
        ctx.sections.clear();
    }

    return CacheSections(*ctx.mem, ctx.mainModule.base, ctx.mainModule.size, ctx.sections);
}

inline void LogSlowInitPhase(const char* phaseName, ULONGLONG startTick, ULONGLONG thresholdMs = 100)
{
    ULONGLONG elapsedMs = GetTickCount64() - startTick;
    if (elapsedMs >= thresholdMs)
    {
        std::cerr << "[xrd][Perf] " << phaseName
                  << " 耗时 " << elapsedMs << " ms\n";
    }
}

// 公共扫描逻辑
// 前置条件：ctx.mem / ctx.mainModule / ctx.pid 已设置
inline bool DoCommonScanAndDiscover()
{
    auto& ctx = Ctx();

    // 缓存 PE 段
    if (!EnsureSectionCacheReady(ctx))
    {
        std::cerr << "[xrd] 缓存 PE 段失败\n";
        return false;
    }

    std::cerr << "[xrd] 缓存了 " << ctx.sections.size() << " 个段: ";
    for (auto& s : ctx.sections)
    {
        std::cerr << s.name << " ";
    }
    std::cerr << "\n";

    // GObjects
    bool chunked = false;
    if (!resolve::ScanGObjects(ctx.sections, *ctx.mem, ctx.off.GObjects, chunked))
    {
        std::cerr << "[xrd] GObjects 未找到\n";
        return false;
    }
    ctx.off.bIsChunkedObjArray = chunked;

    // GNames
    bool isNamePool = false;
    if (!resolve::ScanGNames(ctx.sections, *ctx.mem, ctx.off.GNames, isNamePool))
    {
        std::cerr << "[xrd] GNames 未找到\n";
        return false;
    }
    ctx.off.bUseNamePool = isNamePool;

    // UObject 偏移发现
    if (!resolve::DiscoverUObjectOffsets(*ctx.mem, ctx.off))
    {
        std::cerr << "[xrd] UObject 偏移发现失败\n";
        return false;
    }

    // 动态探测 FNamePoolBlockBits（对标 Rei-Dumper PostInit，必须在 UObject 偏移之后）
    if (ctx.off.bUseNamePool)
    {
        resolve::DetectFNamePoolBlockBits(*ctx.mem, ctx.off);
    }

    resolve::DiscoverStructOffsets(*ctx.mem, ctx.off);
    resolve::DiscoverPropertyBaseOffsets(*ctx.mem, ctx.off);
    resolve::DiscoverAllPropertyOffsets(*ctx.mem, ctx.off);

    {
        ULONGLONG phaseTick = GetTickCount64();
        resolve::DiscoverFunctionFlagsOffset(*ctx.mem, ctx.off);
        resolve::DiscoverExecFunctionOffset(*ctx.mem, ctx.off);
        resolve::DiscoverCastFlagsOffset(*ctx.mem, ctx.off);
        resolve::DiscoverClassDefaultObjectOffset(*ctx.mem, ctx.off);
        LogSlowInitPhase("函数与类偏移发现", phaseTick);
    }

    // UEnum::Names 偏移搜索
    if (ctx.off.UEnum_Names == -1)
    {
        ULONGLONG phaseTick = GetTickCount64();
        i32 total = resolve::GetObjectCount(*ctx.mem, ctx.off);
        for (i32 i = 0; i < total; ++i)
        {
            uptr obj = resolve::ReadObjectAt(*ctx.mem, ctx.off, i);
            if (!IsCanonicalUserPtr(obj))
            {
                continue;
            }
            uptr objCls = 0;
            ReadPtr(*ctx.mem, obj + ctx.off.UObject_Class, objCls);
            if (!IsCanonicalUserPtr(objCls))
            {
                continue;
            }
            FName clsFn{};
            if (!ReadValue(*ctx.mem, objCls + ctx.off.UObject_Name, clsFn))
            {
                continue;
            }
            std::string cn = resolve::ResolveNameDirect(
                *ctx.mem, ctx.off, clsFn.ComparisonIndex, clsFn.Number);
            if (cn != "Enum" && cn != "UserDefinedEnum")
            {
                continue;
            }
            for (i32 testOff = 0x30; testOff <= 0xA0; testOff += 8)
            {
                uptr data = 0;
                i32 count = 0, max = 0;
                ReadPtr(*ctx.mem, obj + testOff, data);
                ReadI32(*ctx.mem, obj + testOff + 8, count);
                ReadI32(*ctx.mem, obj + testOff + 12, max);
                if (IsCanonicalUserPtr(data) &&
                    count > 0 && count <= 256 &&
                    max >= count && max <= 1024)
                {
                    FName fn{};
                    if (ReadValue(*ctx.mem, data, fn))
                    {
                        std::string testName = resolve::ResolveNameDirect(
                            *ctx.mem, ctx.off, fn.ComparisonIndex, fn.Number);
                        if (!testName.empty() && testName.size() < 256)
                        {
                            ctx.off.UEnum_Names = testOff;
                            std::cerr << "[xrd] UEnum::Names +0x"
                                      << std::hex << testOff << std::dec << "\n";
                            break;
                        }
                    }
                }
            }
            if (ctx.off.UEnum_Names != -1)
            {
                break;
            }
        }
        LogSlowInitPhase("UEnum::Names 偏移搜索", phaseTick);
    }

    // UField::Next
    if (ctx.off.UObject_Outer != -1)
    {
        ctx.off.UField_Next = ctx.off.UObject_Outer + 8;
    }

    // 运行时扫描
    {
        ULONGLONG phaseTick = GetTickCount64();
        resolve::ScanGWorld(ctx.sections, *ctx.mem, ctx.off, ctx.off.GWorld);
        LogSlowInitPhase("GWorld 扫描", phaseTick);
    }

    {
        ULONGLONG phaseTick = GetTickCount64();
        resolve::ScanProcessEvent(ctx.sections, *ctx.mem, ctx.off);
        LogSlowInitPhase("ProcessEvent 扫描", phaseTick);
    }

    {
        ULONGLONG phaseTick = GetTickCount64();
        resolve::ScanAppendString(ctx.sections, *ctx.mem, ctx.off);
        LogSlowInitPhase("AppendString 扫描", phaseTick);
    }

    if (ctx.off.DebugCanvasObjCacheAddr == 0)
    {
        uptr dcoAddr = 0;
        if (resolve::ScanDebugCanvasObject(ctx.sections, *ctx.mem, ctx.off, dcoAddr))
        {
            ctx.off.DebugCanvasObjCacheAddr = dcoAddr;
            std::cerr << std::format("[xrd] DebugCanvasObject 找到: 0x{:X} (RVA=0x{:X})\n",
                dcoAddr, dcoAddr - ctx.mainModule.base);
        }
        else
        {
            std::cerr << "[xrd] DebugCanvasObject 未找到（ViewProj 链路不可用）\n";
        }
    }

    // 物理后端检测：PhysX / Chaos
    if (ctx.off.physicsBackend == UEOffsets::ePhysicsUnknown)
    {
        uptr pxBase = 0, pxGlobal = 0;
        if (ResolvePhysXGlobalInstance(*ctx.mem, ctx.pid, pxBase, pxGlobal))
        {
            ctx.off.physicsBackend = UEOffsets::ePhysX;
            ctx.off.PhysXDllBase = pxBase;
            ctx.off.PhysXGlobalPtr = pxGlobal;
            std::cerr << std::format("[xrd] 物理后端: PhysX\n");
            std::cerr << std::format("[xrd] PhysX DLL base: 0x{:X}\n", pxBase);
            std::cerr << std::format("[xrd] PhysX NpPhysics*: 0x{:X}\n", pxGlobal);
        }
        else
        {
            ctx.off.physicsBackend = UEOffsets::eChaos;
            std::cerr << "[xrd] 物理后端: Chaos（PhysX DLL 未找到）\n";
        }
    }

    if (!ValidateCriticalValues(false))
    {
        return false;
    }

    // 标记初始化完成（后续 FindClassByName 等高层 API 依赖此标志）
    ctx.inited = true;
    std::cerr << "[xrd] ctx.inited = true\n";

    // World 链偏移发现（必须在 InitChaosOffsets 之前，Loaded Levels / Actor 枚举依赖这些偏移）
    ULONGLONG worldChainTick = GetTickCount64();
    std::cerr << "[xrd] DiscoverWorldChainOffsets 开始...\n"; std::cerr.flush();
    DiscoverWorldChainOffsets(ctx);
    std::cerr << "[xrd] DiscoverWorldChainOffsets 完成\n"; std::cerr.flush();
    LogSlowInitPhase("World 链偏移发现", worldChainTick);

    // FVector 精度检测（通过反射读取 RelativeLocation.ElementSize: 24=double, 12=float）
    {
        ULONGLONG phaseTick = GetTickCount64();
        bool isDouble = false;
        std::cerr << "[xrd] GetAllActors 开始...\n"; std::cerr.flush();
        auto actors = GetAllActors();
        bool detected = false;
        for (i32 i = 0; i < std::min((i32)actors.size(), (i32)20); ++i)
        {
            uptr actor = actors[i];
            if (!IsCanonicalUserPtr(actor)) continue;

            uptr actorClass = GetObjectClass(actor);
            if (!actorClass) continue;

            i32 rootOff = GetPropertyOffsetByName(actorClass, "RootComponent");
            if (rootOff < 0) continue;

            uptr rootComp = 0;
            if (!ReadPtr(*ctx.mem, actor + rootOff, rootComp) || !IsCanonicalUserPtr(rootComp))
            {
                continue;
            }

            uptr compClass = GetObjectClass(rootComp);
            if (!compClass) continue;

            // 在类继承链中找 RelativeLocation 属性对象
            uptr propObj = 0;
            uptr cur = compClass;
            while (cur)
            {
                uptr p = FindPropertyInStruct(cur, "RelativeLocation");
                if (p)
                {
                    propObj = p;
                    break;
                }
                cur = GetSuperStruct(cur);
            }
            if (!propObj) continue;

            i32 elemSize = 0;
            ReadValue(*ctx.mem, propObj + ctx.off.Property_ElementSize, elemSize);

            if (elemSize == 24)
            {
                isDouble = true;
                std::cerr << "[xrd] FVector 精度: double (UE5) [RelativeLocation.ElementSize=24]\n";
                detected = true;
                break;
            }
            else if (elemSize == 12)
            {
                isDouble = false;
                std::cerr << "[xrd] FVector 精度: float (UE4) [RelativeLocation.ElementSize=12]\n";
                detected = true;
                break;
            }
        }
        if (!detected)
        {
            std::cerr << "[xrd] 无法检测 FVector 精度，默认 float\n";
        }
        ctx.off.bUseDoublePrecision = isDouble;
        LogSlowInitPhase("FVector 精度检测", phaseTick);
    }

    // Chaos 偏移反射发现（依赖 World 链偏移）
    if (ctx.off.physicsBackend == UEOffsets::eChaos && ctx.off.GWorld)
    {
        ULONGLONG phaseTick = GetTickCount64();
        InitChaosOffsets(ctx);
        LogSlowInitPhase("Chaos 偏移发现", phaseTick);
    }

    return true;
}

// 验证关键值：全部有效返回 true，任一缺失则打印并返回 false
inline bool ValidateCriticalValues(bool includeWorldChain)
{
    auto& ctx = Ctx();
    bool allValid = true;

    auto check = [&](bool ok, const char* name) {
        if (!ok)
        {
            std::cerr << "[xrd] 缺少关键值: " << name << "\n";
            allValid = false;
        }
    };

    // 全局指针
    check(ctx.off.GNames != 0,                    "GNames");
    check(ctx.off.GObjects != 0,                   "GObjects");
    check(ctx.off.GWorld != 0,                     "GWorld");
    check(ctx.off.DebugCanvasObjCacheAddr != 0,    "DebugCanvasObject");
    check(ctx.off.ProcessEvent_Addr != 0,          "ProcessEvent");
    check(ctx.off.ProcessEvent_VTableIndex >= 0,   "ProcessEventIdx");
    check(ctx.off.AppendNameToString != 0,         "AppendString");

    if (includeWorldChain)
    {
        check(ctx.off.UWorld_PersistentLevel >= 0,             "UWorld_PersistentLevel");
        check(ctx.off.UWorld_OwningGameInstance >= 0,           "UWorld_OwningGameInstance");
        check(ctx.off.UGameInstance_LocalPlayers >= 0,          "UGameInstance_LocalPlayers");
        check(ctx.off.ULocalPlayer_PlayerController >= 0,       "ULocalPlayer_PlayerController");
        check(ctx.off.ULevel_Actors >= 0,                       "ULevel_Actors");
        check(ctx.off.APlayerController_Pawn >= 0,              "APlayerController_Pawn");
    }

    return allValid;
}

// 重试前重置偏移（保留 PID / mem / mainModule / 已成功缓存的静态段）
inline void ResetOffsetsForRetry()
{
    auto& ctx = Ctx();
    ctx.off = UEOffsets{};
    ctx.chaosOff = ChaosOffsets{};
    ctx.inited = false;
}

} // namespace detail
} // namespace xrd
