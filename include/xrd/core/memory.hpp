#pragma once
// Xrd-eXternalrEsolve - 内存抽象层
// IMemoryAccessor 接口 + WinAPI 实现 + 模板化读写辅助函数

#include "types.hpp"
#include <Windows.h>
#include <string>
#include <vector>
#include <cstring>

namespace xrd
{

// 批量读描述符
struct ReadBatchDesc
{
    uptr  address = 0;    // 源地址
    void* buffer  = nullptr; // 目标缓冲区
    u32   size    = 0;    // 读取字节数
};

// ─── 抽象内存访问接口，方便后续扩展驱动模式 ───
class IMemoryAccessor
{
public:
    virtual ~IMemoryAccessor() = default;
    virtual bool Read(uptr address, void* buffer, std::size_t size) const = 0;
    virtual bool Write(uptr address, const void* buffer, std::size_t size) const = 0;

    // 批量读：一次调用读取多个不连续地址
    // 默认实现逐个 Read，驱动子类可 override 为单次 IOCTL
    virtual bool ReadBatch(ReadBatchDesc* descs, u32 count) const
    {
        if (!descs || count == 0)
        {
            return false;
        }
        bool allOk = true;
        for (u32 i = 0; i < count; ++i)
        {
            auto& d = descs[i];
            if (d.address && d.buffer && d.size > 0)
            {
                if (!Read(d.address, d.buffer, d.size))
                {
                    allOk = false;
                }
            }
        }
        return allOk;
    }
};

// ─── 基于 ReadProcessMemory 的标准实现 ───
class WinApiMemoryAccessor : public IMemoryAccessor
{
public:
    explicit WinApiMemoryAccessor(HANDLE process)
        : m_process(process)
    {
    }

    bool Read(uptr address, void* buffer, std::size_t size) const override
    {
        if (!m_process || !address || !buffer || size == 0)
        {
            return false;
        }
        SIZE_T bytesRead = 0;
        BOOL ok = ReadProcessMemory(
            m_process,
            reinterpret_cast<LPCVOID>(address),
            buffer, size, &bytesRead
        );
        return ok != 0 && bytesRead == size;
    }

    bool Write(uptr address, const void* buffer, std::size_t size) const override
    {
        if (!m_process || !address || !buffer || size == 0)
        {
            return false;
        }
        SIZE_T bytesWritten = 0;
        BOOL ok = WriteProcessMemory(
            m_process,
            reinterpret_cast<LPVOID>(address),
            buffer, size, &bytesWritten
        );
        return ok != 0 && bytesWritten == size;
    }

    HANDLE GetProcessHandle() const { return m_process; }

private:
    HANDLE m_process = nullptr;
};

// ─── 模板化读写辅助函数 ───

template<typename T>
inline bool ReadValue(const IMemoryAccessor& mem, uptr address, T& out)
{
    out = T{};
    if (!address)
    {
        return false;
    }
    return mem.Read(address, &out, sizeof(T));
}

inline bool ReadPtr(const IMemoryAccessor& mem, uptr address, uptr& out)
{
    out = 0;
    if (!address)
    {
        return false;
    }
    return ReadValue(mem, address, out);
}

inline bool ReadI32(const IMemoryAccessor& mem, uptr address, i32& out)
{
    out = 0;
    if (!address)
    {
        return false;
    }
    return ReadValue(mem, address, out);
}

inline bool ReadU32(const IMemoryAccessor& mem, uptr address, u32& out)
{
    out = 0;
    if (!address)
    {
        return false;
    }
    return ReadValue(mem, address, out);
}

inline bool ReadCString(
    const IMemoryAccessor& mem,
    uptr address,
    std::string& out,
    std::size_t maxLen = 256)
{
    out.clear();
    if (!address || maxLen == 0)
    {
        return false;
    }

    std::size_t readLen = (maxLen > 4096) ? 4096 : maxLen;
    std::vector<char> buf(readLen + 1);
    if (!mem.Read(address, buf.data(), readLen))
    {
        return false;
    }

    buf[readLen] = '\0';
    std::size_t n = 0;
    for (; n < readLen && buf[n] != '\0'; ++n) {}
    out.assign(buf.data(), n);
    return true;
}

inline bool ReadWString(
    const IMemoryAccessor& mem,
    uptr address,
    std::wstring& out,
    std::size_t maxChars = 256)
{
    out.clear();
    if (!address || maxChars == 0)
    {
        return false;
    }

    std::size_t readChars = (maxChars > 2048) ? 2048 : maxChars;
    std::vector<wchar_t> buf(readChars + 1);
    if (!mem.Read(address, buf.data(), readChars * sizeof(wchar_t)))
    {
        return false;
    }

    buf[readChars] = L'\0';
    std::size_t n = 0;
    for (; n < readChars && buf[n] != L'\0'; ++n) {}
    out.assign(buf.data(), n);
    return true;
}

inline bool ReadBytes(const IMemoryAccessor& mem, uptr address, void* buffer, std::size_t size)
{
    return mem.Read(address, buffer, size);
}

template<typename T>
inline bool WriteValue(const IMemoryAccessor& mem, uptr address, const T& value)
{
    return mem.Write(address, &value, sizeof(T));
}

// ─── 统一批量读辅助：读 N 个同类型值（WinAPI 逐个读 / 驱动走 IOCTL） ───

template<typename T>
inline bool ReadBatchUniform(
    const IMemoryAccessor& mem,
    const uptr* addresses,
    T* outputs,
    u32 count)
{
    if (!addresses || !outputs || count == 0)
    {
        return false;
    }

    std::vector<ReadBatchDesc> descs(count);
    for (u32 i = 0; i < count; ++i)
    {
        descs[i].address = addresses[i];
        descs[i].buffer  = &outputs[i];
        descs[i].size    = sizeof(T);
    }

    return mem.ReadBatch(descs.data(), count);
}

} // namespace xrd
