#pragma once
// Xrd-eXternalrEsolve - 进程与模块工具
// 进程查找、模块枚举、PE 段缓存

#include "types.hpp"
#include "memory.hpp"
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cwchar>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "user32.lib")

namespace xrd
{

struct ModuleInfo
{
    uptr base = 0;
    u32  size = 0;
    std::wstring name;
};

struct SectionCache
{
    uptr va   = 0;   // 目标进程中的虚拟地址
    u32  size = 0;
    std::string name;
    std::vector<u8> data; // 缓存的段数据副本
};

// ─── 进程查找 ───

inline u32 FindProcessId(const wchar_t* processName)
{
    if (!processName || processName[0] == L'\0')
    {
        return 0;
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, processName) == 0)
            {
                u32 pid = static_cast<u32>(entry.th32ProcessID);
                CloseHandle(snap);
                return pid;
            }
        }
        while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return 0;
}

// 通过窗口类名查找进程（UE 窗口类名通常为 "UnrealWindow"）
inline u32 FindProcessByWindowClass(const wchar_t* className)
{
    HWND hwnd = FindWindowW(className, nullptr);
    if (!hwnd)
    {
        return 0;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return static_cast<u32>(pid);
}

// 通过检测常见 UE 模块来自动查找 UE 进程
inline u32 FindUnrealProcessId()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe))
    {
        do
        {
            u32 pid = static_cast<u32>(pe.th32ProcessID);
            if (pid == 0)
            {
                continue;
            }

            HANDLE msnap = CreateToolhelp32Snapshot(
                TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                static_cast<DWORD>(pid)
            );
            if (msnap == INVALID_HANDLE_VALUE)
            {
                continue;
            }

            bool found = false;
            MODULEENTRY32W me{};
            me.dwSize = sizeof(me);

            if (Module32FirstW(msnap, &me))
            {
                do
                {
                    if (_wcsicmp(me.szModule, L"UE4-Win64-Shipping.dll") == 0 ||
                        _wcsicmp(me.szModule, L"UnrealEngine.dll") == 0)
                    {
                        found = true;
                        break;
                    }
                }
                while (Module32NextW(msnap, &me));
            }

            CloseHandle(msnap);
            if (found)
            {
                CloseHandle(snap);
                return pid;
            }
        }
        while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return 0;
}

inline HANDLE OpenProcessForRead(u32 pid)
{
    return OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE, static_cast<DWORD>(pid)
    );
}

inline HANDLE OpenProcessForReadWrite(u32 pid)
{
    return OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
        PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
        FALSE, static_cast<DWORD>(pid)
    );
}

// ─── 模块枚举 ───

inline bool GetRemoteModuleInfo(u32 pid, const wchar_t* moduleName, ModuleInfo& out)
{
    out = ModuleInfo{};

    HANDLE snap = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        static_cast<DWORD>(pid)
    );
    if (snap == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    BOOL ok = Module32FirstW(snap, &me);

    while (ok)
    {
        if (_wcsicmp(me.szModule, moduleName) == 0)
        {
            out.base = reinterpret_cast<uptr>(me.modBaseAddr);
            out.size = static_cast<u32>(me.modBaseSize);
            out.name = me.szModule;
            CloseHandle(snap);
            return true;
        }
        ok = Module32NextW(snap, &me);
    }

    CloseHandle(snap);
    return false;
}

inline uptr FindModuleBase(u32 pid, const wchar_t* moduleName)
{
    ModuleInfo mi;
    if (!GetRemoteModuleInfo(pid, moduleName, mi))
    {
        return 0;
    }
    return mi.base;
}

// 获取主模块（进程的第一个模块，即 exe 本身）
inline bool GetMainModule(u32 pid, ModuleInfo& out)
{
    out = ModuleInfo{};

    HANDLE snap = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        static_cast<DWORD>(pid)
    );
    if (snap == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);

    if (Module32FirstW(snap, &me))
    {
        out.base = reinterpret_cast<uptr>(me.modBaseAddr);
        out.size = static_cast<u32>(me.modBaseSize);
        out.name = me.szModule;
        CloseHandle(snap);
        return true;
    }

    CloseHandle(snap);
    return false;
}

} // namespace xrd
