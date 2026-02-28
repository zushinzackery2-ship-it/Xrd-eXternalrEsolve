#pragma once
// Xrd-eXternalrEsolve - AutoInit
// 一键初始化：查找进程、附加、扫描所有偏移

#include "context.hpp"
#include "../resolve/scan_gobjects.hpp"
#include "../resolve/scan_gnames.hpp"
#include "../resolve/scan_offsets.hpp"
#include "../resolve/scan_struct_offsets.hpp"
#include "../resolve/scan_property_offsets.hpp"
#include "../resolve/scan_property_base_offsets.hpp"
#include "../resolve/scan_property_base_offsets2.hpp"
#include "../resolve/scan_property_offsets_extra.hpp"
#include "../resolve/scan_ufunction_offsets.hpp"
#include "../resolve/scan_uclass_offsets.hpp"
#include "../resolve/scan_process_event.hpp"
#include "../resolve/scan_append_string.hpp"
#include "../resolve/scan_world.hpp"
#include "../resolve/fvector_detect.hpp"
#include "../resolve/scan_debug_canvas.hpp"
#include "../engine/objects.hpp"
#include "../engine/objects_search.hpp"
#include <iostream>
#include <algorithm>

namespace xrd
{

// ─── 手动偏移设置（通过 RVA） ───

inline void SetGObjects(uptr rva)
{
    Ctx().off.GObjects = Ctx().mainModule.base + rva;
}

inline void SetGNames(uptr rva)
{
    Ctx().off.GNames = Ctx().mainModule.base + rva;
}

inline void SetGWorld(uptr rva)
{
    Ctx().off.GWorld = Ctx().mainModule.base + rva;
}

// ─── 手动偏移设置（通过 VA） ───

inline void SetGObjectsVA(uptr va) { Ctx().off.GObjects = va; }
inline void SetGNamesVA(uptr va)   { Ctx().off.GNames = va; }
inline void SetGWorldVA(uptr va)   { Ctx().off.GWorld = va; }

// ─── 查询 ───

inline bool IsUsingDoublePrecision()
{
    return Ctx().off.bUseDoublePrecision;
}

// ─── AutoInit 主流程 ───

inline bool AutoInit(const wchar_t* processName = nullptr)
{
    ResetContext();
    auto& ctx = Ctx();

    std::cerr << "[xrd] === Xrd-eXternalrEsolve AutoInit ===\n";

    // ── Phase 1: 进程附加 ──
    if (processName && processName[0] != L'\0')
    {
        ctx.pid = FindProcessId(processName);
    }
    else
    {
        // 优先通过窗口类名查找（UE 窗口类名为 "UnrealWindow"）
        ctx.pid = FindProcessByWindowClass(L"UnrealWindow");
        if (ctx.pid == 0)
        {
            // 回退到模块枚举方式
            ctx.pid = FindUnrealProcessId();
        }
        if (ctx.pid == 0)
        {
            std::cerr << "[xrd] 未找到 UE 进程（窗口类名和模块枚举均失败）\n";
            return false;
        }
    }

    if (ctx.pid == 0)
    {
        std::cerr << "[xrd] 进程未找到\n";
        return false;
    }

    std::cerr << "[xrd] PID: " << ctx.pid << "\n";

    ctx.process = OpenProcessForRead(ctx.pid);
    if (!ctx.process)
    {
        std::cerr << "[xrd] 打开进程失败（尝试以管理员运行）\n";
        return false;
    }

    ctx.mem = std::make_unique<WinApiMemoryAccessor>(ctx.process);

    if (!GetMainModule(ctx.pid, ctx.mainModule))
    {
        std::cerr << "[xrd] 获取主模块失败\n";
        return false;
    }

    std::cerr << "[xrd] 模块: ";
    for (wchar_t wc : ctx.mainModule.name)
    {
        std::cerr << static_cast<char>(wc);
    }
    std::cerr << " 基址: 0x" << std::hex << ctx.mainModule.base
              << " 大小: 0x" << ctx.mainModule.size << std::dec << "\n";

    // ── 缓存 PE 段 ──
    if (!CacheSections(*ctx.mem, ctx.mainModule.base, ctx.mainModule.size, ctx.sections))
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

    // ── Phase 2: 定位 GObjects ──
    bool chunked = false;
    if (!resolve::ScanGObjects(ctx.sections, *ctx.mem, ctx.off.GObjects, chunked))
    {
        std::cerr << "[xrd] Phase 2 失败: GObjects 未找到\n";
        return false;
    }
    ctx.off.bIsChunkedObjArray = chunked;

    // ── Phase 2b: 定位 GNames ──
    bool isNamePool = false;
    if (!resolve::ScanGNames(ctx.sections, *ctx.mem, ctx.off.GNames, isNamePool))
    {
        std::cerr << "[xrd] Phase 2 失败: GNames 未找到\n";
        return false;
    }
    ctx.off.bUseNamePool = isNamePool;

    // ── Phase 3: 发现 UObject 偏移 ──
    if (!resolve::DiscoverUObjectOffsets(*ctx.mem, ctx.off))
    {
        std::cerr << "[xrd] Phase 3 失败: UObject 偏移发现失败\n";
        return false;
    }

    // ── Phase 3b: 发现 UStruct 偏移 ──
    resolve::DiscoverStructOffsets(*ctx.mem, ctx.off);

    // ── Phase 3b.5: 发现 Property 基础偏移（ArrayDim/ElementSize/Offset/Flags） ──
    resolve::DiscoverPropertyBaseOffsets(*ctx.mem, ctx.off);

    // ── Phase 3c: 发现类型化属性偏移（ObjectProperty::Class 等） ──
    resolve::DiscoverAllPropertyOffsets(*ctx.mem, ctx.off);

    // ── Phase 3d: 发现 UFunction 偏移（FunctionFlags/ExecFunction） ──
    resolve::DiscoverFunctionFlagsOffset(*ctx.mem, ctx.off);
    resolve::DiscoverExecFunctionOffset(*ctx.mem, ctx.off);

    // ── Phase 3e: 发现 UClass 偏移（CastFlags/ClassDefaultObject） ──
    resolve::DiscoverCastFlagsOffset(*ctx.mem, ctx.off);
    resolve::DiscoverClassDefaultObjectOffset(*ctx.mem, ctx.off);

    // ── Phase 3f: 估算 UEnum::Names 偏移 ──
    // 参考 Rei-Dumper: 通过已知枚举的 TArray::Count 来定位
    // 搜索范围 0x30~0xA0，覆盖 UE4/UE5 各版本
    if (ctx.off.UEnum_Names == -1)
    {
        i32 total = resolve::GetObjectCount(*ctx.mem, ctx.off);
        for (i32 i = 0; i < total; ++i)
        {
            uptr obj = resolve::ReadObjectAt(*ctx.mem, ctx.off, i);
            if (!IsCanonicalUserPtr(obj))
            {
                continue;
            }
            // 读取对象的类名（不依赖 IsInited）
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
            // 在 0x30~0xA0 范围搜索 TArray 模式
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
                            *ctx.mem, ctx.off,
                            fn.ComparisonIndex, fn.Number);
                        if (!testName.empty() && testName.size() < 256)
                        {
                            ctx.off.UEnum_Names = testOff;
                            std::cerr << "[xrd] UEnum::Names +0x"
                                      << std::hex << testOff
                                      << std::dec << "\n";
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
    }

    // ── Phase 4: UField::Next 通常紧跟 UObject 布局之后 ──
    if (ctx.off.UObject_Outer != -1)
    {
        ctx.off.UField_Next = ctx.off.UObject_Outer + 8;
    }

    // ── Phase 5: 运行时扫描 ──
    resolve::ScanGWorld(ctx.sections, *ctx.mem, ctx.off, ctx.off.GWorld);

    // ── Phase 5b: ProcessEvent 扫描 ──
    resolve::ScanProcessEvent(ctx.sections, *ctx.mem, ctx.off);

    // ── Phase 5c: AppendString 扫描 ──
    resolve::ScanAppendString(ctx.sections, *ctx.mem, ctx.off);

    bool isDouble = false;
    resolve::DetectDoublePrecision(*ctx.mem, ctx.off, isDouble);
    ctx.off.bUseDoublePrecision = isDouble;

    // ── Phase 5d: 定位 DebugCanvasObject（用于 ViewProjection 矩阵链路） ──
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

    ctx.inited = true;

    // ── Phase 6: 通过反射发现 World 链偏移 ──
    // UWorld -> OwningGameInstance -> LocalPlayers[0] -> PlayerController -> Pawn
    if (ctx.off.GWorld)
    {
        uptr world = 0;
        if (ReadPtr(*ctx.mem, ctx.off.GWorld, world) && IsCanonicalUserPtr(world))
        {
            uptr worldClass = GetObjectClass(world);
            if (worldClass)
            {
                if (ctx.off.UWorld_OwningGameInstance == -1)
                    ctx.off.UWorld_OwningGameInstance = GetPropertyOffsetByName(worldClass, "OwningGameInstance");
                if (ctx.off.UWorld_PersistentLevel == -1)
                    ctx.off.UWorld_PersistentLevel = GetPropertyOffsetByName(worldClass, "PersistentLevel");
            }

            // GameInstance -> LocalPlayers
            if (ctx.off.UWorld_OwningGameInstance != -1)
            {
                uptr gi = 0;
                if (ReadPtr(*ctx.mem, world + ctx.off.UWorld_OwningGameInstance, gi) && IsCanonicalUserPtr(gi))
                {
                    uptr giClass = GetObjectClass(gi);
                    if (giClass && ctx.off.UGameInstance_LocalPlayers == -1)
                        ctx.off.UGameInstance_LocalPlayers = GetPropertyOffsetByName(giClass, "LocalPlayers");

                    // LocalPlayers[0] -> PlayerController
                    if (ctx.off.UGameInstance_LocalPlayers != -1)
                    {
                        uptr lpData = 0;
                        i32 lpCount = 0;
                        ReadPtr(*ctx.mem, gi + ctx.off.UGameInstance_LocalPlayers, lpData);
                        ReadI32(*ctx.mem, gi + ctx.off.UGameInstance_LocalPlayers + 8, lpCount);

                        if (IsCanonicalUserPtr(lpData) && lpCount > 0)
                        {
                            uptr lp0 = 0;
                            if (ReadPtr(*ctx.mem, lpData, lp0) && IsCanonicalUserPtr(lp0))
                            {
                                uptr lpClass = GetObjectClass(lp0);
                                if (lpClass && ctx.off.ULocalPlayer_PlayerController == -1)
                                    ctx.off.ULocalPlayer_PlayerController = GetPropertyOffsetByName(lpClass, "PlayerController");

                                // PlayerController -> Pawn / PlayerCameraManager
                                if (ctx.off.ULocalPlayer_PlayerController != -1)
                                {
                                    uptr pc = 0;
                                    if (ReadPtr(*ctx.mem, lp0 + ctx.off.ULocalPlayer_PlayerController, pc) && IsCanonicalUserPtr(pc))
                                    {
                                        uptr pcClass = GetObjectClass(pc);
                                        if (pcClass)
                                        {
                                            if (ctx.off.APlayerController_Pawn == -1)
                                            {
                                                i32 pawnOff = GetPropertyOffsetByName(pcClass, "Pawn");
                                                if (pawnOff == -1)
                                                    pawnOff = GetPropertyOffsetByName(pcClass, "AcknowledgedPawn");
                                                ctx.off.APlayerController_Pawn = pawnOff;
                                            }
                                            if (ctx.off.APlayerController_PlayerCameraManager == -1)
                                                ctx.off.APlayerController_PlayerCameraManager = GetPropertyOffsetByName(pcClass, "PlayerCameraManager");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        std::cerr << "[xrd] World链偏移:\n";
        std::cerr << std::format("  OwningGameInstance:  +0x{:X}\n", ctx.off.UWorld_OwningGameInstance);
        std::cerr << std::format("  PersistentLevel:     +0x{:X}\n", ctx.off.UWorld_PersistentLevel);
        std::cerr << std::format("  LocalPlayers:        +0x{:X}\n", ctx.off.UGameInstance_LocalPlayers);
        std::cerr << std::format("  PlayerController:    +0x{:X}\n", ctx.off.ULocalPlayer_PlayerController);
        std::cerr << std::format("  Pawn:                +0x{:X}\n", ctx.off.APlayerController_Pawn);
        std::cerr << std::format("  PlayerCameraManager: +0x{:X}\n", ctx.off.APlayerController_PlayerCameraManager);
    }

    // 对标 Rei-Dumper main.cpp 的输出格式
    uptr moduleBase = ctx.mainModule.base;
    std::cerr << "[xrd] === AutoInit 完成 ===\n";
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
              << (isDouble ? "double (UE5)" : "float (UE4)") << "\n";
    std::cerr << "[xrd] 对象数: "
              << resolve::GetObjectCount(*ctx.mem, ctx.off) << "\n";

    return true;
}

// ─── 驱动模式初始化：使用 DriverMemoryAccessor ───
// 跳过 OpenProcess，通过 ReiVM 驱动读取内存
inline bool AutoInitDriver(const wchar_t* processName = nullptr)
{
    ResetContext();
    auto& ctx = Ctx();

    std::cerr << "[xrd] === Xrd-eXternalrEsolve AutoInit (Driver) ===\n";

    // Phase 1: 查找 PID
    if (processName && processName[0] != L'\0')
    {
        ctx.pid = FindProcessId(processName);
    }
    else
    {
        ctx.pid = FindProcessByWindowClass(L"UnrealWindow");
        if (ctx.pid == 0)
        {
            ctx.pid = FindUnrealProcessId();
        }
    }

    if (ctx.pid == 0)
    {
        std::cerr << "[xrd] 进程未找到\n";
        return false;
    }
    std::cerr << "[xrd] PID: " << ctx.pid << "\n";

    // Phase 1b: 打开驱动并获取主模块基址
    auto driverMem = std::make_unique<DriverMemoryAccessor>();
    if (!driverMem->Open(ctx.pid))
    {
        std::cerr << "[xrd] 驱动打开失败（确认 ReiVM 驱动已加载）\n";
        return false;
    }

    u64 moduleBase = driverMem->GetMainModule();
    if (!moduleBase)
    {
        std::cerr << "[xrd] 通过驱动获取主模块失败\n";
        return false;
    }

    ctx.mem = std::move(driverMem);

    // 用 EnumProcessModulesEx 获取模块名和大小
    if (!GetMainModule(ctx.pid, ctx.mainModule))
    {
        // 如果枚举失败，至少填 base
        ctx.mainModule.base = moduleBase;
        ctx.mainModule.size = 0x8000000; // 默认 128MB
        std::cerr << "[xrd] 模块枚举失败，使用驱动基址 0x"
                  << std::hex << moduleBase << std::dec << "\n";
    }
    else
    {
        // 优先用驱动返回的 base（更可靠）
        ctx.mainModule.base = moduleBase;
    }

    std::cerr << "[xrd] 模块基址: 0x" << std::hex << ctx.mainModule.base
              << " 大小: 0x" << ctx.mainModule.size << std::dec << "\n";

    // 后续流程与 AutoInit 完全相同：缓存段 → 扫描偏移
    if (!CacheSections(*ctx.mem, ctx.mainModule.base, ctx.mainModule.size, ctx.sections))
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

    // 从这里开始和 AutoInit 一样的扫描流程
    bool chunked = false;
    if (!resolve::ScanGObjects(ctx.sections, *ctx.mem, ctx.off.GObjects, chunked))
    {
        std::cerr << "[xrd] GObjects 未找到\n";
        return false;
    }
    ctx.off.bIsChunkedObjArray = chunked;

    bool isNamePool = false;
    if (!resolve::ScanGNames(ctx.sections, *ctx.mem, ctx.off.GNames, isNamePool))
    {
        std::cerr << "[xrd] GNames 未找到\n";
        return false;
    }
    ctx.off.bUseNamePool = isNamePool;

    if (!resolve::DiscoverUObjectOffsets(*ctx.mem, ctx.off))
    {
        std::cerr << "[xrd] UObject 偏移发现失败\n";
        return false;
    }

    resolve::DiscoverStructOffsets(*ctx.mem, ctx.off);
    resolve::DiscoverPropertyBaseOffsets(*ctx.mem, ctx.off);
    resolve::DiscoverAllPropertyOffsets(*ctx.mem, ctx.off);
    resolve::DiscoverFunctionFlagsOffset(*ctx.mem, ctx.off);
    resolve::DiscoverExecFunctionOffset(*ctx.mem, ctx.off);
    resolve::DiscoverCastFlagsOffset(*ctx.mem, ctx.off);
    resolve::DiscoverClassDefaultObjectOffset(*ctx.mem, ctx.off);

    if (ctx.off.UEnum_Names == -1)
    {
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
    }

    if (ctx.off.UObject_Outer != -1)
    {
        ctx.off.UField_Next = ctx.off.UObject_Outer + 8;
    }

    resolve::ScanGWorld(ctx.sections, *ctx.mem, ctx.off, ctx.off.GWorld);
    resolve::ScanProcessEvent(ctx.sections, *ctx.mem, ctx.off);
    resolve::ScanAppendString(ctx.sections, *ctx.mem, ctx.off);

    bool isDouble = false;
    resolve::DetectDoublePrecision(*ctx.mem, ctx.off, isDouble);
    ctx.off.bUseDoublePrecision = isDouble;

    if (ctx.off.DebugCanvasObjCacheAddr == 0)
    {
        uptr dcoAddr = 0;
        if (resolve::ScanDebugCanvasObject(ctx.sections, *ctx.mem, ctx.off, dcoAddr))
        {
            ctx.off.DebugCanvasObjCacheAddr = dcoAddr;
        }
    }

    ctx.inited = true;

    // World 链偏移发现
    if (ctx.off.GWorld)
    {
        uptr world = 0;
        if (ReadPtr(*ctx.mem, ctx.off.GWorld, world) && IsCanonicalUserPtr(world))
        {
            uptr worldClass = GetObjectClass(world);
            if (worldClass)
            {
                if (ctx.off.UWorld_OwningGameInstance == -1)
                    ctx.off.UWorld_OwningGameInstance = GetPropertyOffsetByName(worldClass, "OwningGameInstance");
                if (ctx.off.UWorld_PersistentLevel == -1)
                    ctx.off.UWorld_PersistentLevel = GetPropertyOffsetByName(worldClass, "PersistentLevel");
            }
            if (ctx.off.UWorld_OwningGameInstance != -1)
            {
                uptr gi = 0;
                if (ReadPtr(*ctx.mem, world + ctx.off.UWorld_OwningGameInstance, gi) && IsCanonicalUserPtr(gi))
                {
                    uptr giClass = GetObjectClass(gi);
                    if (giClass && ctx.off.UGameInstance_LocalPlayers == -1)
                        ctx.off.UGameInstance_LocalPlayers = GetPropertyOffsetByName(giClass, "LocalPlayers");
                    if (ctx.off.UGameInstance_LocalPlayers != -1)
                    {
                        uptr lpData = 0;
                        i32 lpCount = 0;
                        ReadPtr(*ctx.mem, gi + ctx.off.UGameInstance_LocalPlayers, lpData);
                        ReadI32(*ctx.mem, gi + ctx.off.UGameInstance_LocalPlayers + 8, lpCount);
                        if (IsCanonicalUserPtr(lpData) && lpCount > 0)
                        {
                            uptr lp0 = 0;
                            if (ReadPtr(*ctx.mem, lpData, lp0) && IsCanonicalUserPtr(lp0))
                            {
                                uptr lpClass = GetObjectClass(lp0);
                                if (lpClass && ctx.off.ULocalPlayer_PlayerController == -1)
                                    ctx.off.ULocalPlayer_PlayerController = GetPropertyOffsetByName(lpClass, "PlayerController");
                                if (ctx.off.ULocalPlayer_PlayerController != -1)
                                {
                                    uptr pc = 0;
                                    if (ReadPtr(*ctx.mem, lp0 + ctx.off.ULocalPlayer_PlayerController, pc) && IsCanonicalUserPtr(pc))
                                    {
                                        uptr pcClass = GetObjectClass(pc);
                                        if (pcClass)
                                        {
                                            if (ctx.off.APlayerController_Pawn == -1)
                                            {
                                                i32 pawnOff = GetPropertyOffsetByName(pcClass, "Pawn");
                                                if (pawnOff == -1)
                                                    pawnOff = GetPropertyOffsetByName(pcClass, "AcknowledgedPawn");
                                                ctx.off.APlayerController_Pawn = pawnOff;
                                            }
                                            if (ctx.off.APlayerController_PlayerCameraManager == -1)
                                                ctx.off.APlayerController_PlayerCameraManager = GetPropertyOffsetByName(pcClass, "PlayerCameraManager");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::cerr << "[xrd] === AutoInit (Driver) 完成 ===\n";
    return true;
}

} // namespace xrd
