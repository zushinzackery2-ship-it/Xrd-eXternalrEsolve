#pragma once
// Xrd-eXternalrEsolve - ReiVM 驱动内存访问器
// 通过 IOCTL 读写目标进程内存，支持单次读和批量读

#include "memory.hpp"
#include <winioctl.h>

namespace xrd
{

// ReiVM 驱动协议定义
namespace ReiVMProtocol
{
    inline constexpr wchar_t kDeviceSymbolicName[] = L"\\\\.\\ReiVMDrv";

    inline constexpr DWORD IOCTL_READ =
        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_OUT_DIRECT, FILE_ANY_ACCESS);
    inline constexpr DWORD IOCTL_GET_MAINMODULE =
        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_OUT_DIRECT, FILE_ANY_ACCESS);
    inline constexpr DWORD IOCTL_READ_BATCH =
        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_OUT_DIRECT, FILE_ANY_ACCESS);

#pragma pack(push, 8)
    struct DriverHeader
    {
        u32 flag;
        u32 pid;
        u64 address;
        u64 buffer;
        u32 size;
    };

    struct IoctlReadDesc
    {
        u64 address;
        u32 size;
        u32 reserved;
    };

    struct IoctlReadBatch
    {
        u32 pid;
        u32 count;
        u32 reserved0;
        u32 reserved1;
        IoctlReadDesc descriptors[1];
    };
#pragma pack(pop)
} // namespace ReiVMProtocol

// 基于 ReiVM 驱动的内存访问器
class DriverMemoryAccessor : public IMemoryAccessor
{
public:
    DriverMemoryAccessor() = default;

    // 打开驱动设备并绑定 PID
    bool Open(u32 pid)
    {
        m_pid = pid;
        m_hDriver = CreateFileW(
            ReiVMProtocol::kDeviceSymbolicName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        return m_hDriver != INVALID_HANDLE_VALUE;
    }

    // 通过驱动获取主模块基址
    u64 GetMainModule() const
    {
        if (m_hDriver == INVALID_HANDLE_VALUE || m_pid == 0)
        {
            return 0;
        }

        ReiVMProtocol::DriverHeader input{};
        input.pid = m_pid;

        u64 moduleBase = 0;
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_hDriver,
            ReiVMProtocol::IOCTL_GET_MAINMODULE,
            &input, sizeof(input),
            &moduleBase, sizeof(moduleBase),
            &bytesReturned, nullptr);

        if (!ok)
        {
            return 0;
        }
        return moduleBase;
    }

    bool Read(uptr address, void* buffer, std::size_t size) const override
    {
        if (m_hDriver == INVALID_HANDLE_VALUE || !address || !buffer || size == 0)
        {
            return false;
        }

        ReiVMProtocol::DriverHeader header{};
        header.pid = m_pid;
        header.address = static_cast<u64>(address);
        header.size = static_cast<u32>(size);

        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_hDriver,
            ReiVMProtocol::IOCTL_READ,
            &header, sizeof(header),
            buffer, static_cast<DWORD>(size),
            &bytesReturned, nullptr);

        return ok != 0 && bytesReturned == static_cast<DWORD>(size);
    }

    bool Write(uptr address, const void* buffer, std::size_t size) const override
    {
        // ReiVM 驱动不支持写入
        (void)address;
        (void)buffer;
        (void)size;
        return false;
    }

    // 批量读：一次 IOCTL 读取多个不连续地址
    bool ReadBatch(ReadBatchDesc* descs, u32 count) const override
    {
        if (m_hDriver == INVALID_HANDLE_VALUE || !descs || count == 0)
        {
            return false;
        }

        // 计算每个描述符需要的输出大小和总输出大小
        u32 totalOutSize = 0;
        for (u32 i = 0; i < count; ++i)
        {
            totalOutSize += descs[i].size;
        }
        if (totalOutSize == 0)
        {
            return false;
        }

        // 构建 IOCTL 输入
        std::size_t batchInSize = sizeof(ReiVMProtocol::IoctlReadBatch)
            + (static_cast<std::size_t>(count) - 1u) * sizeof(ReiVMProtocol::IoctlReadDesc);
        std::vector<u8> batchInBuf(batchInSize);
        auto* batch = reinterpret_cast<ReiVMProtocol::IoctlReadBatch*>(batchInBuf.data());
        batch->pid = m_pid;
        batch->count = count;
        batch->reserved0 = 0;
        batch->reserved1 = 0;

        for (u32 i = 0; i < count; ++i)
        {
            batch->descriptors[i].address = static_cast<u64>(descs[i].address);
            batch->descriptors[i].size = descs[i].size;
            batch->descriptors[i].reserved = 0;
        }

        // IOCTL 输出缓冲区
        std::vector<u8> outBuf(totalOutSize);

        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_hDriver,
            ReiVMProtocol::IOCTL_READ_BATCH,
            batch, static_cast<DWORD>(batchInSize),
            outBuf.data(), totalOutSize,
            &bytesReturned, nullptr);

        if (!ok || bytesReturned != totalOutSize)
        {
            return false;
        }

        // 将结果拷贝到各描述符的 buffer 中
        u32 offset = 0;
        for (u32 i = 0; i < count; ++i)
        {
            if (descs[i].buffer && descs[i].size > 0)
            {
                std::memcpy(descs[i].buffer, outBuf.data() + offset, descs[i].size);
            }
            offset += descs[i].size;
        }

        return true;
    }

    HANDLE GetDriverHandle() const { return m_hDriver; }
    u32 GetPid() const { return m_pid; }

    ~DriverMemoryAccessor()
    {
        if (m_hDriver != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hDriver);
            m_hDriver = INVALID_HANDLE_VALUE;
        }
    }

    // 禁止拷贝
    DriverMemoryAccessor(const DriverMemoryAccessor&) = delete;
    DriverMemoryAccessor& operator=(const DriverMemoryAccessor&) = delete;

    // 允许移动
    DriverMemoryAccessor(DriverMemoryAccessor&& other) noexcept
        : m_hDriver(other.m_hDriver), m_pid(other.m_pid)
    {
        other.m_hDriver = INVALID_HANDLE_VALUE;
        other.m_pid = 0;
    }

    DriverMemoryAccessor& operator=(DriverMemoryAccessor&& other) noexcept
    {
        if (this != &other)
        {
            if (m_hDriver != INVALID_HANDLE_VALUE)
            {
                CloseHandle(m_hDriver);
            }
            m_hDriver = other.m_hDriver;
            m_pid = other.m_pid;
            other.m_hDriver = INVALID_HANDLE_VALUE;
            other.m_pid = 0;
        }
        return *this;
    }

private:
    HANDLE m_hDriver = INVALID_HANDLE_VALUE;
    u32 m_pid = 0;
};

} // namespace xrd
