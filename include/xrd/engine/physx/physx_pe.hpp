#pragma once
// Xrd-eXternalrEsolve - PhysX PE 导出表解析
// 通过解析 PxGetPhysics 导出函数中的 mov reg,[rip+disp32] 动态定位全局实例

#include "../../core/memory.hpp"
#include "../../core/process.hpp"
#include <cstring>

namespace xrd
{

// 在远程进程的 DLL 导出表中查找指定函数名的 RVA
inline bool FindExportRVA(
    const IMemoryAccessor& mem,
    uptr dllBase,
    const char* funcName,
    u32& outRVA)
{
    outRVA = 0;

    IMAGE_DOS_HEADER dos{};
    if (!mem.Read(dllBase, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    IMAGE_NT_HEADERS64 nt{};
    if (!mem.Read(dllBase + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    auto& exportDir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDir.VirtualAddress == 0 || exportDir.Size == 0)
    {
        std::cerr << "[xrd][PE] no export dir\n";
        return false;
    }

    IMAGE_EXPORT_DIRECTORY ed{};
    if (!mem.Read(dllBase + exportDir.VirtualAddress, &ed, sizeof(ed)))
    {
        std::cerr << "[xrd][PE] read export dir failed\n";
        return false;
    }

    u32 nameCount = ed.NumberOfNames;
    std::cerr << std::format("[xrd][PE] nameCount={} AddrOfNames=0x{:X}\n", nameCount, ed.AddressOfNames);
    if (nameCount == 0)
    {
        return false;
    }

    // 批量读取名称 RVA 数组
    std::vector<u32> nameRVAs(nameCount);
    if (!mem.Read(dllBase + ed.AddressOfNames, nameRVAs.data(), nameCount * sizeof(u32)))
    {
        std::cerr << "[xrd][PE] read name RVAs failed\n";
        return false;
    }

    // 逐个比较导出名
    std::size_t targetLen = std::strlen(funcName);
    char nameBuf[128]{};

    u32 readFail = 0, readOk = 0;
    for (u32 i = 0; i < nameCount; ++i)
    {
        if (!mem.Read(dllBase + nameRVAs[i], nameBuf, sizeof(nameBuf) - 1))
        {
            ++readFail;
            continue;
        }
        ++readOk;
        nameBuf[sizeof(nameBuf) - 1] = '\0';

        if (std::strcmp(nameBuf, funcName) == 0)
        {
            // 读取序号
            u16 ordinal = 0;
            if (!ReadValue(mem, dllBase + ed.AddressOfNameOrdinals + i * sizeof(u16), ordinal))
            {
                return false;
            }

            // 读取函数 RVA
            if (!ReadValue(mem, dllBase + ed.AddressOfFunctions + ordinal * sizeof(u32), outRVA))
            {
                return false;
            }
            return true;
        }
    }
    std::cerr << std::format("[xrd][PE] not found '{}': readOk={} readFail={}\n", funcName, readOk, readFail);
    return false;
}

// 在函数体内扫描 mov reg, [rip+disp32] 指令，提取目标全局变量的 RVA
// 指令格式: 48 8B XX YY YY YY YY（REX.W MOV reg, [RIP+disp32]）
// XX 的低 3 位为 101(=5)，即 ModRM r/m=RIP-relative
// RAX 优先：XX=0x05，其次 RCX=0x0D, RDX=0x15 等
inline bool ResolveRipRelativeTarget(
    const IMemoryAccessor& mem,
    uptr dllBase,
    u32 funcRVA,
    u32& outGlobalRVA)
{
    outGlobalRVA = 0;

    // 读取函数前 64 字节（足够覆盖简单 getter）
    constexpr u32 kScanSize = 64;
    u8 code[kScanSize]{};
    if (!mem.Read(dllBase + funcRVA, code, kScanSize))
    {
        return false;
    }

    for (u32 i = 0; i + 7 <= kScanSize; ++i)
    {
        // REX.W 前缀 + MOV opcode
        if (code[i] != 0x48 || code[i + 1] != 0x8B)
        {
            continue;
        }

        u8 modrm = code[i + 2];
        // mod=00, r/m=101 表示 RIP-relative
        if ((modrm & 0xC7) != 0x05)
        {
            continue;
        }

        // 提取 disp32（小端序）
        i32 disp = 0;
        std::memcpy(&disp, &code[i + 3], 4);

        // 目标地址 = 指令末尾地址(funcRVA + i + 7) + disp
        u32 targetRVA = funcRVA + i + 7 + static_cast<u32>(disp);
        outGlobalRVA = targetRVA;
        return true;
    }

    return false;
}

// 一步完成：查找 PhysX DLL → 解析 PxGetPhysics → 提取全局实例指针
inline bool ResolvePhysXGlobalInstance(
    const IMemoryAccessor& mem,
    u32 pid,
    uptr& outDllBase,
    uptr& outGlobalPtr)
{
    outDllBase = 0;
    outGlobalPtr = 0;

    // 查找 PhysX DLL（优先 PhysX3_x64.dll，兼容其他命名）
    const wchar_t* dllNames[] =
    {
        L"PhysX3_x64.dll",
        L"PhysX3.dll",
        L"PhysX_64.dll",
    };

    for (auto* name : dllNames)
    {
        outDllBase = FindModuleBase(pid, name);
        if (outDllBase != 0)
        {
            // 输出找到的模块名
            std::cerr << "[xrd][PhysX] DLL found: ";
            for (const wchar_t* p = name; *p; ++p)
            {
                std::cerr << static_cast<char>(*p);
            }
            std::cerr << std::format(" base=0x{:X}\n", outDllBase);
            break;
        }
    }

    if (outDllBase == 0)
    {
        std::cerr << "[xrd][PhysX] FAIL: PhysX DLL not found in process modules\n";
        return false;
    }

    // 解析导出表找 PxGetPhysics
    u32 globalRVA = 0;
    u32 funcRVA = 0;
    if (FindExportRVA(mem, outDllBase, "PxGetPhysics", funcRVA))
    {
        std::cerr << std::format("[xrd][PhysX] PxGetPhysics RVA=0x{:X}\n", funcRVA);
        // 解析 mov reg, [rip+disp32] 获取全局变量 RVA
        if (!ResolveRipRelativeTarget(mem, outDllBase, funcRVA, globalRVA))
        {
            std::cerr << "[xrd][PhysX] WARN: mov reg,[rip+disp32] not found\n";
        }
    }
    else
    {
        std::cerr << "[xrd][PhysX] WARN: export not found, using hardcoded RVA\n";
    }

    // Fallback：驱动读不到导出表时，使用 IDA 逆向得到的已知 RVA
    if (globalRVA == 0)
    {
        globalRVA = 0x1E8878;
        std::cerr << std::format("[xrd][PhysX] fallback globalRVA=0x{:X}\n", globalRVA);
    }
    std::cerr << std::format("[xrd][PhysX] global RVA=0x{:X} VA=0x{:X}\n",
                             globalRVA, outDllBase + globalRVA);

    // 读取全局指针值（NpPhysics* 实例）
    uptr instancePtr = 0;
    if (!ReadPtr(mem, outDllBase + globalRVA, instancePtr))
    {
        std::cerr << "[xrd][PhysX] FAIL: cannot read global pointer\n";
        return false;
    }
    if (!IsCanonicalUserPtr(instancePtr))
    {
        std::cerr << std::format("[xrd][PhysX] FAIL: invalid ptr 0x{:X}\n", instancePtr);
        return false;
    }

    outGlobalPtr = instancePtr;
    return true;
}

} // namespace xrd
