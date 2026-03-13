#pragma once
// Xrd-eXternalrEsolve - 共享内存驱动访问器
// 通过 MDL 映射的共享内存 + adaptive spinning 实现零 IOCTL 读取
// 仅初始化和销毁时使用 IOCTL，运行时所有 Read 走共享内存

#include "memory.hpp"
#include "memory_driver.hpp"  // ReiVMProtocol 定义复用
#include <intrin.h>
#include <iostream>
#include <mutex>

namespace xrd
{

// 共享内存命令
namespace ShmemCmd
{
    inline constexpr LONG None  = 0;
    inline constexpr LONG Read  = 1;
    inline constexpr LONG Write = 2;
    inline constexpr LONG Mouse = 3;
    inline constexpr LONG MouseSetup = 4;
    inline constexpr LONG ReadBatch = 5;
}

// 共享内存参数（与驱动 shared_mem.h 一致）
namespace ShmemConst
{
    inline constexpr u32 PageCount     = 16;              // 16 页 = 64KB
    inline constexpr u32 TotalSize     = PageCount * 4096;
    inline constexpr u32 DataOffset    = 0x100;           // 256 字节控制头
    inline constexpr u32 MaxData       = TotalSize - DataOffset;
    inline constexpr u32 MaxSessions   = 10;              // 与驱动 MAX_SHMEM_SESSIONS 保持一致
    inline constexpr u32 SpinThreshold = 4000;            // 自旋迭代次数
    inline constexpr DWORD EventTimeoutMs = 50;           // Event 等待超时
}

// 读取方法（与驱动一致）
namespace ReadMethod
{
    inline constexpr u32 CR3 = ReiVMProtocol::READ_METHOD_CR3;
    inline constexpr u32 MmCpy = ReiVMProtocol::READ_METHOD_MMCPY;
}

struct SharedMouseInputData
{
    u16 UnitId;
    u16 Flags;
    u32 Buttons;
    u32 RawButtons;
    i32 LastX;
    i32 LastY;
    u32 ExtraInformation;
};

// 共享内存控制头（与驱动 SHARED_MEM_HEADER 布局完全一致）
struct SharedMemHeader
{
    volatile LONG requestReady;     // +0x00
    volatile LONG responseReady;    // +0x04
    volatile LONG shutdown;         // +0x08
    LONG command;                   // +0x0C
    u32 pid;                        // +0x10
    u32 size;                       // +0x14
    u64 address;                    // +0x18
    u32 method;                     // +0x20
    LONG status;                    // +0x24 (NTSTATUS)
    u32 bytesRead;                  // +0x28
    union
    {
        u32 reserved[5];            // +0x2C 兼容旧布局
        struct
        {
            u32 auxCount;
            u32 auxDescriptorBytes;
            u32 auxTransferBytes;
            u32 auxChunkIndex;
            u32 auxFlags;
        };
    };
    SharedMouseInputData mouseData; // +0x40
    u64 mouseSetupSubRsp70Rva;      // +0x58
    u32 mouseSetupStubSize;         // +0x60
    u32 reserved2;                  // +0x64
};

// IOCTL 初始化请求/响应（与驱动一致）
struct ShmemInitRequest
{
    u32 slotId;       // 0xFFFFFFFF = 自动分配
};

struct ShmemInitResponse
{
    u64 userVa;
    u64 requestEventHandle;
    u64 responseEventHandle;
    u32 totalSize;
    u32 dataOffset;
    u32 slotId;       // 分配到的 slot ID
    u32 reserved;
};

// IOCTL 编号
namespace ShmemIoctl
{
    inline constexpr DWORD Init =
        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x830, METHOD_BUFFERED, FILE_ANY_ACCESS);
    inline constexpr DWORD Destroy =
        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x831, METHOD_BUFFERED, FILE_ANY_ACCESS);
}

// 基于共享内存的内存访问器
class SharedMemoryAccessor : public IMemoryAccessor
{
public:
    SharedMemoryAccessor() = default;

    // 打开驱动并初始化共享内存通道（自动分配 slot）
    bool Open(u32 pid)
    {
        return Open(pid, 0xFFFFFFFF);
    }

    // 打开驱动并初始化共享内存通道（指定 slot，0xFFFFFFFF = 自动分配）
    bool Open(u32 pid, u32 requestedSlot)
    {
        m_pid = pid;
        m_requestedSlot = requestedSlot;

        // 打开驱动设备
        m_hDriver = CreateFileW(
            ReiVMProtocol::kDeviceSymbolicName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (m_hDriver == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        // 初始化共享内存
        if (!InitSharedMem())
        {
            CloseHandle(m_hDriver);
            m_hDriver = INVALID_HANDLE_VALUE;
            return false;
        }

        return true;
    }

    // 通过驱动获取主模块基址（仍需 IOCTL，只调用一次）
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

    // 通过共享内存读取——零 IOCTL 开销
    bool Read(uptr address, void* buffer, std::size_t size) const override
    {
        if (!m_header || !address || !buffer || size == 0)
        {
            return false;
        }

        // 超过共享内存数据区大小时分块读取
        if (size > ShmemConst::MaxData)
        {
            return ReadChunked(address, buffer, size);
        }

        return ReadSingle(address, buffer, static_cast<u32>(size));
    }

    bool Write(uptr address, const void* buffer, std::size_t size) const override
    {
        if (!m_header || !address || !buffer || size == 0)
        {
            return false;
        }

        if (size > ShmemConst::MaxData)
        {
            return WriteChunked(address, buffer, size);
        }

        return WriteSingle(address, buffer, static_cast<u32>(size));
    }

    // 批量读：优先合批走共享内存，超限时退化为分块批量或单次读。
    bool ReadBatch(ReadBatchDesc* descs, u32 count) const override
    {
        if (!m_header || !descs || count == 0)
        {
            return false;
        }

        bool allOk = true;
        u32 begin = 0;

        while (begin < count)
        {
            auto& first = descs[begin];
            if (!first.address || !first.buffer || first.size == 0)
            {
                ++begin;
                continue;
            }

            if (first.size > ShmemConst::MaxData)
            {
                if (!Read(first.address, first.buffer, first.size))
                {
                    allOk = false;
                }
                ++begin;
                continue;
            }

            u32 chunkCount = 0;
            std::size_t descriptorBytes = 0;
            std::size_t transferBytes = 0;

            while (begin + chunkCount < count)
            {
                const auto& desc = descs[begin + chunkCount];
                std::size_t nextDescriptorBytes =
                    (static_cast<std::size_t>(chunkCount) + 1u)
                    * sizeof(ReiVMProtocol::IoctlReadDesc);
                std::size_t nextTransferBytes = transferBytes;

                if (desc.address && desc.buffer && desc.size > 0)
                {
                    if (desc.size > ShmemConst::MaxData)
                    {
                        break;
                    }

                    nextTransferBytes += desc.size;
                }

                if (nextDescriptorBytes > ShmemConst::MaxData
                    || nextTransferBytes > ShmemConst::MaxData)
                {
                    break;
                }

                descriptorBytes = nextDescriptorBytes;
                transferBytes = nextTransferBytes;
                ++chunkCount;
            }

            if (chunkCount == 0)
            {
                if (!Read(first.address, first.buffer, first.size))
                {
                    allOk = false;
                }
                ++begin;
                continue;
            }

            if (!ReadBatchSingle(descs + begin, chunkCount))
            {
                allOk = false;
            }

            begin += chunkCount;
        }

        return allOk;
    }

    HANDLE GetDriverHandle() const { return m_hDriver; }
    u32 GetPid() const { return m_pid; }
    bool IsSharedMemReady() const { return m_header != nullptr; }

    bool SendMouseEvent(const SharedMouseInputData& mouseData)
    {
        if (!m_header)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_header->command = ShmemCmd::Mouse;
        ClearAuxFieldsLocked();
        m_header->status = 0;
        m_header->bytesRead = 0;
        m_header->mouseData = mouseData;
        return SubmitRequestLocked() && m_header->status >= 0;
    }

    bool SetupMouseHook(u64 subRsp70Rva, const void* stubBytes, u32 stubSize)
    {
        if (!m_header || !stubBytes || stubSize == 0 || stubSize > ShmemConst::MaxData)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        std::memcpy(m_dataPtr, stubBytes, stubSize);
        m_header->command = ShmemCmd::MouseSetup;
        ClearAuxFieldsLocked();
        m_header->status = 0;
        m_header->bytesRead = 0;
        m_header->mouseSetupSubRsp70Rva = subRsp70Rva;
        m_header->mouseSetupStubSize = stubSize;
        return SubmitRequestLocked() && m_header->status >= 0;
    }

    ~SharedMemoryAccessor()
    {
        DestroySharedMem();

        if (m_hDriver != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hDriver);
            m_hDriver = INVALID_HANDLE_VALUE;
        }
    }

    // 禁止拷贝
    SharedMemoryAccessor(const SharedMemoryAccessor&) = delete;
    SharedMemoryAccessor& operator=(const SharedMemoryAccessor&) = delete;

    // 允许移动
    SharedMemoryAccessor(SharedMemoryAccessor&& other) noexcept
        : m_hDriver(other.m_hDriver)
        , m_pid(other.m_pid)
        , m_requestedSlot(other.m_requestedSlot)
        , m_slotId(other.m_slotId)
        , m_header(other.m_header)
        , m_dataPtr(other.m_dataPtr)
        , m_requestEvent(other.m_requestEvent)
        , m_responseEvent(other.m_responseEvent)
    {
        other.m_hDriver = INVALID_HANDLE_VALUE;
        other.m_pid = 0;
        other.m_requestedSlot = 0xFFFFFFFF;
        other.m_slotId = 0;
        other.m_header = nullptr;
        other.m_dataPtr = nullptr;
        other.m_requestEvent = nullptr;
        other.m_responseEvent = nullptr;
    }

    SharedMemoryAccessor& operator=(SharedMemoryAccessor&& other) noexcept
    {
        if (this != &other)
        {
            DestroySharedMem();
            if (m_hDriver != INVALID_HANDLE_VALUE)
            {
                CloseHandle(m_hDriver);
            }

            m_hDriver = other.m_hDriver;
            m_pid = other.m_pid;
            m_requestedSlot = other.m_requestedSlot;
            m_slotId = other.m_slotId;
            m_header = other.m_header;
            m_dataPtr = other.m_dataPtr;
            m_requestEvent = other.m_requestEvent;
            m_responseEvent = other.m_responseEvent;

            other.m_hDriver = INVALID_HANDLE_VALUE;
            other.m_pid = 0;
            other.m_requestedSlot = 0xFFFFFFFF;
            other.m_slotId = 0;
            other.m_header = nullptr;
            other.m_dataPtr = nullptr;
            other.m_requestEvent = nullptr;
            other.m_responseEvent = nullptr;
        }
        return *this;
    }

private:
    void ClearAuxFieldsLocked() const
    {
        m_header->auxCount = 0;
        m_header->auxDescriptorBytes = 0;
        m_header->auxTransferBytes = 0;
        m_header->auxChunkIndex = 0;
        m_header->auxFlags = 0;
    }

    bool SubmitRequestLocked() const
    {
        _mm_sfence();  // 确保写入对驱动可见

        InterlockedExchange(&m_header->responseReady, 0);
        InterlockedExchange(&m_header->requestReady, 1);

        if (m_requestEvent)
        {
            SetEvent(m_requestEvent);
        }

        BOOL gotResponse = FALSE;

        for (u32 i = 0; i < ShmemConst::SpinThreshold; ++i)
        {
            if (InterlockedCompareExchange(&m_header->responseReady, 0, 1) == 1)
            {
                gotResponse = TRUE;
                break;
            }
            _mm_pause();
        }

        if (!gotResponse && m_responseEvent)
        {
            WaitForSingleObject(m_responseEvent, ShmemConst::EventTimeoutMs);
            if (InterlockedCompareExchange(&m_header->responseReady, 0, 1) == 1)
            {
                gotResponse = TRUE;
            }
        }

        if (!gotResponse)
        {
            return false;
        }

        _mm_lfence();  // 确保读取有序
        return true;
    }

    // 单次共享内存读取（size <= MaxData）
    // 共享内存通道是单通道，多线程并发必须序列化
    bool ReadSingle(uptr address, void* buffer, u32 size) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_header->command = ShmemCmd::Read;
        m_header->pid = m_pid;
        m_header->address = static_cast<u64>(address);
        m_header->size = size;
        m_header->method = ReadMethod::CR3;
        ClearAuxFieldsLocked();
        m_header->status = 0;
        m_header->bytesRead = 0;

        if (!SubmitRequestLocked())
        {
            return false;
        }

        if (m_header->status >= 0 && m_header->bytesRead > 0)
        {
            u32 toCopy = (m_header->bytesRead < size) ? m_header->bytesRead : size;
            std::memcpy(buffer, m_dataPtr, toCopy);
            return m_header->bytesRead == size;
        }

        return false;
    }

    bool WriteSingle(uptr address, const void* buffer, u32 size) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::memcpy(m_dataPtr, buffer, size);
        m_header->command = ShmemCmd::Write;
        m_header->pid = m_pid;
        m_header->address = static_cast<u64>(address);
        m_header->size = size;
        m_header->method = ReadMethod::CR3;
        ClearAuxFieldsLocked();
        m_header->status = 0;
        m_header->bytesRead = 0;

        if (!SubmitRequestLocked())
        {
            return false;
        }

        return m_header->status >= 0 && m_header->bytesRead == size;
    }

    // 分块读取（超过 MaxData 时自动分块）
    bool ReadChunked(uptr address, void* buffer, std::size_t totalSize) const
    {
        u8* dst = static_cast<u8*>(buffer);
        std::size_t remaining = totalSize;
        uptr currentAddr = address;

        while (remaining > 0)
        {
            u32 chunk = (remaining > ShmemConst::MaxData)
                ? ShmemConst::MaxData
                : static_cast<u32>(remaining);

            if (!ReadSingle(currentAddr, dst, chunk))
            {
                return false;
            }

            dst += chunk;
            currentAddr += chunk;
            remaining -= chunk;
        }

        return true;
    }

    bool WriteChunked(uptr address, const void* buffer, std::size_t totalSize) const
    {
        const u8* src = static_cast<const u8*>(buffer);
        std::size_t remaining = totalSize;
        uptr currentAddr = address;

        while (remaining > 0)
        {
            u32 chunk = (remaining > ShmemConst::MaxData)
                ? ShmemConst::MaxData
                : static_cast<u32>(remaining);

            if (!WriteSingle(currentAddr, src, chunk))
            {
                return false;
            }

            src += chunk;
            currentAddr += chunk;
            remaining -= chunk;
        }

        return true;
    }

    bool ReadBatchSingle(ReadBatchDesc* descs, u32 count) const
    {
        std::vector<ReiVMProtocol::IoctlReadDesc> descriptors(count);
        std::size_t descriptorBytes = descriptors.size() * sizeof(descriptors[0]);
        std::size_t transferBytes = 0;

        for (u32 i = 0; i < count; ++i)
        {
            descriptors[i].address = static_cast<u64>(descs[i].address);
            descriptors[i].size = descs[i].size;
            descriptors[i].reserved = 0;

            if (descs[i].address && descs[i].buffer && descs[i].size > 0)
            {
                transferBytes += descs[i].size;
            }
        }

        if (descriptorBytes > ShmemConst::MaxData || transferBytes > ShmemConst::MaxData)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        std::memcpy(m_dataPtr, descriptors.data(), descriptorBytes);
        m_header->command = ShmemCmd::ReadBatch;
        m_header->pid = m_pid;
        m_header->address = 0;
        m_header->size = static_cast<u32>(transferBytes);
        m_header->method = ReadMethod::CR3;
        m_header->auxCount = count;
        m_header->auxDescriptorBytes = static_cast<u32>(descriptorBytes);
        m_header->auxTransferBytes = static_cast<u32>(transferBytes);
        m_header->auxChunkIndex = 0;
        m_header->auxFlags = 0;
        m_header->status = 0;
        m_header->bytesRead = 0;

        if (!SubmitRequestLocked())
        {
            return false;
        }

        if (m_header->status < 0 || m_header->bytesRead != transferBytes)
        {
            return false;
        }

        std::size_t offset = 0;
        for (u32 i = 0; i < count; ++i)
        {
            if (descs[i].address && descs[i].buffer && descs[i].size > 0)
            {
                std::memcpy(
                    descs[i].buffer,
                    m_dataPtr + offset,
                    descs[i].size);
                offset += descs[i].size;
            }
        }

        return true;
    }

    // 初始化共享内存通道
    bool InitSharedMem()
    {
        ShmemInitRequest req{};
        req.slotId = m_requestedSlot;
        ShmemInitResponse resp{};
        DWORD bytesReturned = 0;

        BOOL ok = DeviceIoControl(
            m_hDriver,
            ShmemIoctl::Init,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned, nullptr);

        if (!ok || resp.userVa == 0)
        {
            return false;
        }

        m_header = reinterpret_cast<SharedMemHeader*>(resp.userVa);
        m_dataPtr = reinterpret_cast<u8*>(resp.userVa) + resp.dataOffset;
        m_requestEvent = reinterpret_cast<HANDLE>(resp.requestEventHandle);
        m_responseEvent = reinterpret_cast<HANDLE>(resp.responseEventHandle);
        m_slotId = resp.slotId;

        return true;
    }

    // 销毁共享内存通道
    void DestroySharedMem()
    {
        if (!m_header)
        {
            return;
        }

        std::cerr << "[xrd][Shmem] 开始销毁 slot=" << m_slotId
                  << " pid=" << m_pid << "\n";

        // 通知驱动工作线程退出
        InterlockedExchange(&m_header->shutdown, 1);
        if (m_requestEvent)
        {
            SetEvent(m_requestEvent);
        }

        // 发送销毁 IOCTL（传 slotId 只销毁自己的通道）
        if (m_hDriver != INVALID_HANDLE_VALUE)
        {
            DWORD bytesReturned = 0;
            BOOL destroyOk = DeviceIoControl(
                m_hDriver,
                ShmemIoctl::Destroy,
                &m_slotId, sizeof(m_slotId),
                nullptr, 0,
                &bytesReturned, nullptr);
            std::cerr << "[xrd][Shmem] Destroy IOCTL 返回 slot=" << m_slotId
                      << " ok=" << (destroyOk ? 1 : 0)
                      << " err=" << (destroyOk ? 0 : GetLastError()) << "\n";
        }

        m_header = nullptr;
        m_dataPtr = nullptr;
        m_requestEvent = nullptr;
        m_responseEvent = nullptr;
        std::cerr << "[xrd][Shmem] 销毁完成 slot=" << m_slotId << "\n";
    }

    HANDLE m_hDriver = INVALID_HANDLE_VALUE;
    u32 m_pid = 0;
    u32 m_requestedSlot = 0xFFFFFFFF;      // 请求的 slot（0xFFFFFFFF = 自动分配）
    u32 m_slotId = 0;                      // 实际分配到的 slot ID
    SharedMemHeader* m_header = nullptr;   // 驱动映射到用户空间的共享内存
    u8* m_dataPtr = nullptr;               // 数据区指针 (header + dataOffset)
    HANDLE m_requestEvent = nullptr;       // 请求事件（SetEvent 通知驱动）
    HANDLE m_responseEvent = nullptr;      // 响应事件（WaitForSingleObject 等完成）
    mutable std::mutex m_mutex;            // 序列化多线程共享内存访问
};

} // namespace xrd
