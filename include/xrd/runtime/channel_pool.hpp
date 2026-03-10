#pragma once
// Xrd-eXternalrEsolve - 多通道共享内存池
// 将额外的共享内存读取通道统一收口到 xrd，避免上层项目重复维护。

#include "../memory/memory_shmem.hpp"
#include <vector>
#include <memory>
#include <iostream>

namespace xrd
{
class SharedMemoryChannelPool
{
public:
    static constexpr int kDefaultExtraChannels = 3;

    bool Initialize(u32 pid, int extraChannelCount = kDefaultExtraChannels)
    {
        Reset();

        if (extraChannelCount <= 0)
        {
            ready = true;
            return true;
        }

        channels.reserve(extraChannelCount);

        for (int i = 0; i < extraChannelCount; ++i)
        {
            auto channel = std::make_unique<SharedMemoryAccessor>();
            if (!channel->Open(pid))
            {
                std::cerr << "[xrd][ChannelPool] 通道 " << (i + 1) << " 打开失败\n";
                Reset();
                return false;
            }

            std::cerr << "[xrd][ChannelPool] 通道 " << (i + 1) << " 就绪\n";
            channels.push_back(std::move(channel));
        }

        ready = true;
        return true;
    }

    void Reset()
    {
        channels.clear();
        ready = false;
    }

    IMemoryAccessor* Get(int index)
    {
        if (!ready || index < 0 || index >= static_cast<int>(channels.size()))
        {
            return nullptr;
        }

        return channels[index].get();
    }

    int GetCount() const
    {
        return static_cast<int>(channels.size());
    }

    bool IsReady() const
    {
        return ready;
    }

private:
    std::vector<std::unique_ptr<SharedMemoryAccessor>> channels;
    bool ready = false;
};
} // namespace xrd
