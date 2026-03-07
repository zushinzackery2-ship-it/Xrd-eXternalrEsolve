#pragma once
// Xrd-eXternalrEsolve - AutoInit 主入口
// 三种模式：WinAPI / Driver / SharedMem

#include "../core/context.hpp"
#include "../memory/memory_shmem.hpp"
#include "../resolve/globals/scan_gobjects.hpp"
#include "../resolve/globals/scan_gnames.hpp"
#include "../resolve/globals/scan_world.hpp"
#include "../resolve/globals/scan_debug_canvas.hpp"
#include "../resolve/uobject/scan_offsets.hpp"
#include "../resolve/uobject/scan_struct_offsets.hpp"
#include "../resolve/uobject/scan_ufunction_offsets.hpp"
#include "../resolve/uobject/scan_uclass_offsets.hpp"
#include "../resolve/property/scan_property_offsets.hpp"
#include "../resolve/property/scan_property_base_offsets.hpp"
#include "../resolve/property/scan_property_base_offsets2.hpp"
#include "../resolve/property/scan_property_offsets_extra.hpp"
#include "../resolve/runtime/scan_process_event.hpp"
#include "../resolve/runtime/scan_append_string.hpp"
#include "../engine/objects/objects.hpp"
#include "../engine/objects/objects_search.hpp"
#include "init_helpers.hpp"
#include "init_common.hpp"
#include <iostream>

namespace xrd
{

// ─── WinAPI 模式初始化 ───

inline bool AutoInit(const wchar_t* processName = nullptr)
{
    ResetContext();
    auto& ctx = Ctx();

    std::cerr << "[xrd] === Xrd-eXternalrEsolve AutoInit ===\n";

    // Phase 1: 查找进程
    ctx.pid = detail::FindTargetProcess(processName);
    if (ctx.pid == 0)
    {
        return false;
    }

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

    // Phase 2+: 公共扫描与偏移发现
    if (!detail::DoCommonScanAndDiscover())
    {
        return false;
    }

    detail::PrintInitSummary();
    std::cerr << "[xrd] === AutoInit 完成 ===\n";
    return true;
}

// ─── 驱动模式初始化：使用 DriverMemoryAccessor ───

inline bool AutoInitDriver(const wchar_t* processName = nullptr)
{
    ResetContext();
    auto& ctx = Ctx();

    std::cerr << "[xrd] === Xrd-eXternalrEsolve AutoInit (Driver) ===\n";

    ctx.pid = detail::FindTargetProcess(processName);
    if (ctx.pid == 0)
    {
        return false;
    }

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

    if (!GetMainModule(ctx.pid, ctx.mainModule))
    {
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

    if (!detail::DoCommonScanAndDiscover())
    {
        return false;
    }

    detail::PrintInitSummary();
    std::cerr << "[xrd] === AutoInit (Driver) 完成 ===\n";
    return true;
}

// ─── 共享内存模式初始化：使用 SharedMemoryAccessor ───

inline bool AutoInitSharedMem(const wchar_t* processName = nullptr)
{
    ResetContext();
    auto& ctx = Ctx();

    std::cerr << "[xrd] === Xrd-eXternalrEsolve AutoInit (SharedMem) ===\n";

    ctx.pid = detail::FindTargetProcess(processName);
    if (ctx.pid == 0)
    {
        return false;
    }

    auto shmemMem = std::make_unique<SharedMemoryAccessor>();
    if (!shmemMem->Open(ctx.pid))
    {
        std::cerr << "[xrd] 共享内存初始化失败（确认 ReiVM 驱动已加载）\n";
        return false;
    }

    u64 moduleBase = shmemMem->GetMainModule();
    if (!moduleBase)
    {
        std::cerr << "[xrd] 通过驱动获取主模块失败\n";
        return false;
    }

    ctx.mem = std::move(shmemMem);

    if (!GetMainModule(ctx.pid, ctx.mainModule))
    {
        ctx.mainModule.base = moduleBase;
        ctx.mainModule.size = 0x8000000; // 默认 128MB
        std::cerr << "[xrd] 模块枚举失败，使用驱动基址 0x"
                  << std::hex << moduleBase << std::dec << "\n";
    }
    else
    {
        ctx.mainModule.base = moduleBase;
    }

    std::cerr << "[xrd] 模块基址: 0x" << std::hex << ctx.mainModule.base
              << " 大小: 0x" << ctx.mainModule.size << std::dec << "\n";

    if (!detail::DoCommonScanAndDiscover())
    {
        return false;
    }

    detail::PrintInitSummary();
    std::cerr << "[xrd] === AutoInit (SharedMem) 完成 ===\n";
    return true;
}

} // namespace xrd
